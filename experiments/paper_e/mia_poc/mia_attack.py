"""
E1: MIA Privacy PoC — Paper E (#70)

Test: can an adversary reconstruct images from 128-d MobileNetV3-Small embeddings?
Attacks: (1) Fredrikson-style gradient inversion, (2) Zhu DLG
Metrics: SSIM, LPIPS, L2 pixel error

Usage:
    python3 mia_attack.py --backbone mobilenet_v3_small --dim 128 --attack fredrikson
    python3 mia_attack.py --backbone mobilenet_v3_small --dim 128 --attack dlg
    python3 mia_attack.py --run-all          # all backbones × attacks × dims

Output: results/mia_results.json + reconstructed images in results/recon/
"""

import argparse
import json
import os
import random
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
import torchvision
import torchvision.transforms as T
from PIL import Image
from skimage.metrics import structural_similarity as ssim_fn
import lpips

DEVICE = "cuda" if torch.cuda.is_available() else "cpu"
RESULTS_DIR = Path(__file__).parent / "results"
RECON_DIR = RESULTS_DIR / "recon"
RESULTS_DIR.mkdir(exist_ok=True)
RECON_DIR.mkdir(exist_ok=True)

# ── Feature extractor ────────────────────────────────────────────────────────

class EmbeddingExtractor(nn.Module):
    """Wraps a backbone, replaces classifier with FC(dim)."""

    def __init__(self, backbone_name: str, dim: int):
        super().__init__()
        self.dim = dim
        self.backbone_name = backbone_name

        if backbone_name == "mobilenet_v3_small":
            base = torchvision.models.mobilenet_v3_small(
                weights=torchvision.models.MobileNet_V3_Small_Weights.IMAGENET1K_V1
            )
            # last pooling → 576-d → project to dim
            in_features = base.classifier[0].in_features
            base.classifier = nn.Identity()
            self.base = base
            self.proj = nn.Linear(in_features, dim)

        elif backbone_name == "efficientnet_lite0":
            import timm
            base = timm.create_model("efficientnet_lite0", pretrained=True, num_classes=0)
            in_features = base.num_features
            self.base = base
            self.proj = nn.Linear(in_features, dim)

        elif backbone_name == "squeezenet":
            base = torchvision.models.squeezenet1_1(
                weights=torchvision.models.SqueezeNet1_1_Weights.IMAGENET1K_V1
            )
            base.classifier = nn.AdaptiveAvgPool2d((1, 1))
            self.base = base
            self.proj = nn.Linear(512, dim)

        elif backbone_name == "resnet18":
            import timm
            base = timm.create_model("resnet18", pretrained=True, num_classes=0)
            in_features = base.num_features  # 512
            self.base = base
            self.proj = nn.Linear(in_features, dim)

        elif backbone_name == "mobilenetv2":
            # MCUNet-V2 proxy: MobileNetV2 0.5× (~1.5M params)
            import timm
            base = timm.create_model("mobilenetv2_050", pretrained=True, num_classes=0)
            in_features = base.num_features
            self.base = base
            self.proj = nn.Linear(in_features, dim)

        else:
            raise ValueError(f"Unknown backbone: {backbone_name}")

    def forward(self, x):
        feat = self.base(x)
        if feat.dim() > 2:
            feat = feat.flatten(1)
        emb = self.proj(feat)
        return emb  # (B, dim)


# ── Differential Privacy noise injection ─────────────────────────────────────

def add_dp_noise(emb, epsilon, delta=1e-5):
    """
    Add Gaussian DP noise to embedding before transmission.
    Sensitivity = L2 norm of embedding (clip to unit sphere first).
    Returns noised embedding, actual noise std.
    """
    # L2 normalize (clip to unit sphere → sensitivity = 1)
    emb_norm = emb / (emb.norm(dim=-1, keepdim=True).clamp(min=1e-8))
    # Gaussian mechanism: sigma = sqrt(2*ln(1.25/delta)) / epsilon
    sigma = (2 * np.log(1.25 / delta)) ** 0.5 / epsilon
    noise = torch.randn_like(emb_norm) * sigma
    return emb_norm + noise, sigma


# ── Transforms ───────────────────────────────────────────────────────────────

def get_transform(train=False):
    if train:
        return T.Compose([
            T.Resize((96, 96)),
            T.RandomHorizontalFlip(),
            T.ColorJitter(0.2, 0.2, 0.2),
            T.ToTensor(),
            T.Normalize([0.485, 0.456, 0.406], [0.229, 0.224, 0.225]),
        ])
    return T.Compose([
        T.Resize((96, 96)),
        T.ToTensor(),
        T.Normalize([0.485, 0.456, 0.406], [0.229, 0.224, 0.225]),
    ])


def denorm(t):
    """Denormalize tensor to [0,1]."""
    mean = torch.tensor([0.485, 0.456, 0.406], device=t.device).view(3, 1, 1)
    std  = torch.tensor([0.229, 0.224, 0.225], device=t.device).view(3, 1, 1)
    return (t * std + mean).clamp(0, 1)


# ── Dataset: CIFAR-10 proxy (fast, no download needed beyond torch) ──────────
# CIFAR-10 has 10 classes; we use 4 of them as proxy for human/vehicle/animal/none
# (cat=3, dog=5, automobile=1, truck=9 ≈ animal/animal/vehicle/vehicle)
# For paper: replace with actual 4-class dataset

CIFAR_CLASSES = {3: 0, 5: 1, 1: 2, 9: 3}  # cat, dog, auto, truck → 0..3

def load_proxy_dataset(n_train=2000, n_test=200):
    """Load CIFAR-10 subset as 4-class proxy dataset."""
    cifar_train = torchvision.datasets.CIFAR10(
        root="/tmp/cifar10", train=True, download=True, transform=get_transform(train=True)
    )
    cifar_test = torchvision.datasets.CIFAR10(
        root="/tmp/cifar10", train=False, download=True, transform=get_transform(train=False)
    )

    def filter_dataset(ds, n):
        indices = [i for i, (_, lbl) in enumerate(ds) if lbl in CIFAR_CLASSES]
        random.shuffle(indices)
        indices = indices[:n]
        subset = torch.utils.data.Subset(ds, indices)
        return subset

    train_ds = filter_dataset(cifar_train, n_train)
    test_ds  = filter_dataset(cifar_test,  n_test)
    return train_ds, test_ds


# ── Train embedding model ────────────────────────────────────────────────────

def train_model(model, train_ds, epochs=10, lr=1e-3):
    """Fine-tune with 4-class head."""
    classifier = nn.Linear(model.dim, 4).to(DEVICE)
    model = model.to(DEVICE)
    opt = optim.Adam(list(model.parameters()) + list(classifier.parameters()), lr=lr)
    loader = torch.utils.data.DataLoader(train_ds, batch_size=64, shuffle=True, num_workers=2)
    criterion = nn.CrossEntropyLoss()

    model.train(); classifier.train()
    for epoch in range(epochs):
        total_loss = 0
        for imgs, labels in loader:
            imgs = imgs.to(DEVICE)
            # remap labels
            labels = torch.tensor([CIFAR_CLASSES[l.item()] for l in labels]).to(DEVICE)
            opt.zero_grad()
            emb = model(imgs)
            loss = criterion(classifier(emb), labels)
            loss.backward()
            opt.step()
            total_loss += loss.item()
        print(f"  epoch {epoch+1}/{epochs} loss={total_loss/len(loader):.4f}")

    return model, classifier


def eval_accuracy(model, classifier, test_ds):
    model.eval(); classifier.eval()
    loader = torch.utils.data.DataLoader(test_ds, batch_size=64, num_workers=2)
    correct = total = 0
    with torch.no_grad():
        for imgs, labels in loader:
            imgs = imgs.to(DEVICE)
            labels = torch.tensor([CIFAR_CLASSES[l.item()] for l in labels]).to(DEVICE)
            emb = model(imgs)
            pred = classifier(emb).argmax(1)
            correct += (pred == labels).sum().item()
            total += len(labels)
    return correct / total


# ── Attack 1: Fredrikson-style gradient inversion ────────────────────────────

def attack_fredrikson(model, target_emb, n_iters=3000, lr=0.05, tv_weight=1e-4):
    """
    Minimize ||model(x_recon) - target_emb||^2 + TV(x_recon) via gradient descent.
    White-box: full access to model weights.
    """
    model.eval()
    # random init
    x = torch.randn(1, 3, 96, 96, requires_grad=True, device=DEVICE)
    opt = optim.Adam([x], lr=lr)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(opt, T_max=n_iters)

    best_loss = float('inf')
    best_x = x.detach().clone()

    for i in range(n_iters):
        opt.zero_grad()
        emb_recon = model(x)
        loss_emb = ((emb_recon - target_emb) ** 2).sum()
        # total variation regularization
        tv = (((x[:, :, 1:, :] - x[:, :, :-1, :]) ** 2).sum() +
              ((x[:, :, :, 1:] - x[:, :, :, :-1]) ** 2).sum())
        loss = loss_emb + tv_weight * tv
        loss.backward()
        opt.step()
        scheduler.step()

        if loss_emb.item() < best_loss:
            best_loss = loss_emb.item()
            best_x = x.detach().clone()

    return denorm(best_x.squeeze(0))  # (3, H, W) in [0,1]


# ── Attack 2: Zhu DLG (deep leakage from gradients) ─────────────────────────

def attack_dlg(model, classifier, target_img, target_label, n_iters=300, lr=1.0):
    """
    DLG: reconstruct input from gradient of training loss w.r.t. model params.
    NOTE: requires backbone with differentiable activations (no hardsigmoid).
    Falls back to fredrikson-style if second-order grad fails.
    """
    model.eval(); classifier.eval()
    criterion = nn.CrossEntropyLoss()

    target_img_d = target_img.unsqueeze(0).to(DEVICE)
    target_label_t = torch.tensor([target_label], device=DEVICE)

    # compute real gradients (first-order only — detached)
    emb_real = model(target_img_d)
    loss_real = criterion(classifier(emb_real), target_label_t)
    real_grad = torch.autograd.grad(loss_real, list(model.parameters()) + list(classifier.parameters()),
                                    create_graph=False)
    real_grad = [g.detach() for g in real_grad]

    # dummy reconstruction
    x = torch.randn_like(target_img_d, requires_grad=True)
    dummy_label = torch.randn(1, 4, requires_grad=True, device=DEVICE)
    opt = optim.LBFGS([x, dummy_label], lr=lr, max_iter=20)

    params = list(model.parameters()) + list(classifier.parameters())

    def closure():
        opt.zero_grad()
        try:
            emb_d = model(x)
            loss_d = criterion(classifier(emb_d),
                               dummy_label.softmax(1).argmax(1).detach())
            dummy_grad = torch.autograd.grad(loss_d, params, create_graph=True,
                                             allow_unused=True)
            grad_diff = sum(
                ((dg - rg) ** 2).sum()
                for dg, rg in zip(dummy_grad, real_grad)
                if dg is not None
            )
            grad_diff.backward()
            return grad_diff
        except RuntimeError:
            # hardsigmoid / non-differentiable activation fallback:
            # minimise embedding distance directly (same as fredrikson)
            emb_d = model(x)
            with torch.no_grad():
                target_emb = model(target_img_d)
            loss_fb = ((emb_d - target_emb) ** 2).sum()
            loss_fb.backward()
            return loss_fb

    for _ in range(n_iters // 20):
        opt.step(closure)
        with torch.no_grad():
            x.clamp_(-3, 3)

    return denorm(x.detach().squeeze(0))


# ── Attack 3: Trained Inversion Network (GAN proxy / Yang 2019 style) ────────

class InversionDecoder(nn.Module):
    """Decoder: embedding dim → 3×96×96 image. Trained on paired (img, emb) data."""
    def __init__(self, dim):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(dim, 512), nn.ReLU(),
            nn.Linear(512, 1024), nn.ReLU(),
            nn.Unflatten(1, (64, 4, 4)),          # (B, 64, 4, 4)
            nn.ConvTranspose2d(64, 64, 4, 2, 1),  # → 8×8
            nn.ReLU(),
            nn.ConvTranspose2d(64, 32, 4, 2, 1),  # → 16×16
            nn.ReLU(),
            nn.ConvTranspose2d(32, 16, 4, 2, 1),  # → 32×32
            nn.ReLU(),
            nn.ConvTranspose2d(16, 8,  4, 2, 1),  # → 64×64
            nn.ReLU(),
            nn.ConvTranspose2d(8,  3,  4, 2, 1),  # → 128×128
            nn.AdaptiveAvgPool2d((96, 96)),
            nn.Sigmoid(),
        )
    def forward(self, z):
        return self.net(z)


_inversion_decoders = {}  # cache: (backbone_name, dim) → trained decoder

def train_inversion_decoder(model, train_ds, dim, epochs=20):
    """Train decoder on (embedding, image) pairs from train_ds."""
    cache_key = (model.backbone_name, dim)
    if cache_key in _inversion_decoders:
        return _inversion_decoders[cache_key]

    decoder = InversionDecoder(dim).to(DEVICE)
    opt = optim.Adam(decoder.parameters(), lr=1e-3)
    loader = torch.utils.data.DataLoader(train_ds, batch_size=64, shuffle=True, num_workers=2)
    model.eval()

    for epoch in range(epochs):
        total = 0
        for imgs, _ in loader:
            imgs = imgs.to(DEVICE)
            with torch.no_grad():
                emb = model(imgs)
            recon = decoder(emb)
            orig_01 = denorm(imgs)
            loss = nn.functional.mse_loss(recon, orig_01)
            opt.zero_grad(); loss.backward(); opt.step()
            total += loss.item()
        if (epoch + 1) % 5 == 0:
            print(f"  [decoder] epoch {epoch+1}/{epochs} loss={total/len(loader):.4f}")

    _inversion_decoders[cache_key] = decoder
    return decoder


def attack_inversion_net(model, target_emb, train_ds, dim):
    """Pass target embedding through trained inversion decoder."""
    decoder = train_inversion_decoder(model, train_ds, dim)
    decoder.eval()
    with torch.no_grad():
        recon = decoder(target_emb)
    return recon.squeeze(0)  # (3, H, W) in [0,1]


# ── Metrics ──────────────────────────────────────────────────────────────────

_lpips_fn = None

def compute_metrics(orig_tensor, recon_tensor):
    """
    orig_tensor, recon_tensor: (3, H, W) in [0,1], on CPU.
    Returns dict: ssim, lpips, l2_pixel
    """
    global _lpips_fn
    if _lpips_fn is None:
        _lpips_fn = lpips.LPIPS(net='alex').to(DEVICE)

    orig_np  = orig_tensor.permute(1, 2, 0).numpy()   # (H,W,3)
    recon_np = recon_tensor.permute(1, 2, 0).numpy()

    s = float(ssim_fn(orig_np, recon_np, data_range=1.0, channel_axis=2))
    l2 = float(np.mean((orig_np - recon_np) ** 2))

    orig_lp  = (orig_tensor.clamp(0,1) * 2 - 1).unsqueeze(0).to(DEVICE)
    recon_lp = (recon_tensor.clamp(0,1) * 2 - 1).unsqueeze(0).to(DEVICE)
    with torch.no_grad():
        lp = float(_lpips_fn(orig_lp, recon_lp).item())

    if np.isnan(s) or np.isnan(lp):
        return {"ssim": None, "lpips": None, "l2_pixel": None, "error": "nan"}

    return {"ssim": round(s, 4), "lpips": round(lp, 4), "l2_pixel": round(l2, 6)}


# ── Save image ────────────────────────────────────────────────────────────────

def save_img(tensor, path):
    arr = (tensor.permute(1, 2, 0).numpy() * 255).astype(np.uint8)
    Image.fromarray(arr).save(path)


# ── Main experiment ──────────────────────────────────────────────────────────

def run_experiment(backbone_name, dim, attack_name, n_samples=20, dp_epsilon=None):
    print(f"\n{'='*60}")
    print(f"Backbone={backbone_name} dim={dim} attack={attack_name}")
    print(f"{'='*60}")

    random.seed(42); torch.manual_seed(42); np.random.seed(42)

    # model
    model = EmbeddingExtractor(backbone_name, dim).to(DEVICE)

    # data
    print("Loading dataset...")
    train_ds, test_ds = load_proxy_dataset()

    # train
    print("Training classification head...")
    model, classifier = train_model(model, train_ds, epochs=15)

    acc = eval_accuracy(model, classifier, test_ds)
    print(f"Test accuracy: {acc:.3f}")

    # collect test samples
    loader = torch.utils.data.DataLoader(test_ds, batch_size=1, shuffle=True)
    samples = []
    for imgs, labels in loader:
        if len(samples) >= n_samples:
            break
        samples.append((imgs[0], CIFAR_CLASSES[labels[0].item()]))

    # attack
    model.eval()
    results = []
    for idx, (orig_img, label) in enumerate(samples):
        orig_img = orig_img.to(DEVICE)
        with torch.no_grad():
            target_emb = model(orig_img.unsqueeze(0))

        if dp_epsilon is not None:
            target_emb, dp_sigma = add_dp_noise(target_emb, dp_epsilon)

        t0 = time.time()
        if attack_name == "fredrikson":
            recon = attack_fredrikson(model, target_emb)
        elif attack_name == "dlg":
            recon = attack_dlg(model, classifier, orig_img, label)
        elif attack_name == "inversion_net":
            recon = attack_inversion_net(model, target_emb, train_ds, dim)
        else:
            raise ValueError(f"Unknown attack: {attack_name}")
        elapsed = time.time() - t0

        metrics = compute_metrics(
            denorm(orig_img).cpu(),
            recon.cpu()
        )
        metrics["sample_idx"] = idx
        metrics["attack_time_s"] = round(elapsed, 2)
        results.append(metrics)

        # save images
        tag = f"{backbone_name}_{dim}d_{attack_name}_{idx:02d}"
        save_img(denorm(orig_img).cpu(), RECON_DIR / f"{tag}_orig.png")
        save_img(recon.cpu(),            RECON_DIR / f"{tag}_recon.png")

        if metrics.get("error") == "nan":
            print(f"  [{idx:02d}] SSIM=NaN (skipped)  t={elapsed:.1f}s")
        else:
            print(f"  [{idx:02d}] SSIM={metrics['ssim']:.4f}  "
                  f"LPIPS={metrics['lpips']:.4f}  "
                  f"L2={metrics['l2_pixel']:.6f}  t={elapsed:.1f}s")

    # aggregate (skip samples where metrics are None)
    ssim_vals  = [r["ssim"]  for r in results if r["ssim"]  is not None]
    lpips_vals = [r["lpips"] for r in results if r["lpips"] is not None]
    summary = {
        "backbone": backbone_name,
        "dim": dim,
        "attack": attack_name,
        "dp_epsilon": dp_epsilon,
        "n_samples": n_samples,
        "accuracy": round(acc, 4),
        "ssim_mean":  round(float(np.mean(ssim_vals)),  4),
        "ssim_std":   round(float(np.std(ssim_vals)),   4),
        "ssim_max":   round(float(np.max(ssim_vals)),   4),
        "lpips_mean": round(float(np.mean(lpips_vals)), 4),
        "lpips_std":  round(float(np.std(lpips_vals)),  4),
        "privacy_claim_holds": float(np.mean(ssim_vals)) < 0.4,
        "samples": results,
    }

    print(f"\nSummary: SSIM={summary['ssim_mean']:.4f}±{summary['ssim_std']:.4f} "
          f"(max={summary['ssim_max']:.4f})  "
          f"LPIPS={summary['lpips_mean']:.4f}  "
          f"acc={acc:.3f}")
    print(f"Privacy claim (SSIM<0.4): {'✅ HOLDS' if summary['privacy_claim_holds'] else '❌ FAILS'}")

    return summary


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--backbone", default="mobilenet_v3_small",
                        choices=["mobilenet_v3_small", "efficientnet_lite0",
                                 "squeezenet", "resnet18", "mobilenetv2"])
    parser.add_argument("--dim",     type=int, default=128)
    parser.add_argument("--attack",  default="fredrikson",
                        choices=["fredrikson", "dlg", "inversion_net"])
    parser.add_argument("--samples", type=int, default=20)
    parser.add_argument("--run-all", action="store_true",
                        help="Run full 5×3×2 grid (backbones×attacks×dims)")
    parser.add_argument("--dp-epsilon", type=float, default=None,
                        help="DP epsilon for Gaussian noise (None=no noise)")
    args = parser.parse_args()

    all_results = []

    if args.run_all:
        # full #75 E6 grid
        backbones = ["mobilenet_v3_small", "efficientnet_lite0",
                     "squeezenet", "resnet18", "mobilenetv2"]
        dims      = [64, 128]
        attacks   = ["fredrikson", "dlg", "inversion_net"]
        for bb in backbones:
            for d in dims:
                for atk in attacks:
                    r = run_experiment(bb, d, atk, n_samples=args.samples)
                    all_results.append(r)
    else:
        r = run_experiment(args.backbone, args.dim, args.attack,
                           n_samples=args.samples, dp_epsilon=args.dp_epsilon)
        all_results.append(r)

    out_path = RESULTS_DIR / "mia_results.json"
    with open(out_path, "w") as f:
        json.dump(all_results, f, indent=2)
    print(f"\nResults saved → {out_path}")

    # print pivot decision
    print("\n" + "="*60)
    print("PIVOT DECISION")
    print("="*60)
    for r in all_results:
        status = "✅ SSIM<0.4 → Option B (backbone comparison)" \
                 if r["privacy_claim_holds"] \
                 else "❌ SSIM≥0.4 → Option A (DP noise)"
        print(f"  {r['backbone']} {r['dim']}d {r['attack']}: "
              f"SSIM={r['ssim_mean']:.4f}  {status}")


if __name__ == "__main__":
    main()
