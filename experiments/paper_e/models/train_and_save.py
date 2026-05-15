"""
Train MobileNetV3-Small + 128-d projection head on CIFAR-10 4-class proxy.
Save checkpoint for TFLite conversion.

Usage:
    python3 train_and_save.py

Output:
    mobilenetv3_128d_trained.pt   -- full model state dict
    mobilenet_classifier_4cls.pt  -- 4-class head state dict
"""
import random
import sys
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
import torchvision
import torchvision.transforms as T

sys.path.insert(0, str(Path(__file__).parent.parent / "mia_poc"))
from mia_attack import EmbeddingExtractor, CIFAR_CLASSES, DEVICE

SAVE_DIR = Path(__file__).parent
random.seed(42); torch.manual_seed(42); np.random.seed(42)

def get_transform(train=True):
    if train:
        return T.Compose([
            T.Resize((96, 96)),
            T.RandomHorizontalFlip(),
            T.ToTensor(),
            T.Normalize([0.485, 0.456, 0.406], [0.229, 0.224, 0.225]),
        ])
    return T.Compose([
        T.Resize((96, 96)),
        T.ToTensor(),
        T.Normalize([0.485, 0.456, 0.406], [0.229, 0.224, 0.225]),
    ])

def load_dataset(n_train=4000, n_test=400):
    cifar_train = torchvision.datasets.CIFAR10("/tmp/cifar10", train=True,  download=True, transform=get_transform(True))
    cifar_test  = torchvision.datasets.CIFAR10("/tmp/cifar10", train=False, download=True, transform=get_transform(False))
    def filter_ds(ds, n):
        idx = [i for i, (_, l) in enumerate(ds) if l in CIFAR_CLASSES]
        random.shuffle(idx)
        return torch.utils.data.Subset(ds, idx[:n])
    return filter_ds(cifar_train, n_train), filter_ds(cifar_test, n_test)

def train(model, classifier, train_ds, epochs=20, lr=1e-3):
    model = model.to(DEVICE); classifier = classifier.to(DEVICE)
    opt = optim.Adam(list(model.parameters()) + list(classifier.parameters()), lr=lr)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(opt, T_max=epochs)
    loader = torch.utils.data.DataLoader(train_ds, batch_size=64, shuffle=True, num_workers=2)
    criterion = nn.CrossEntropyLoss()
    for epoch in range(epochs):
        model.train(); classifier.train()
        total_loss = 0
        for imgs, labels in loader:
            imgs = imgs.to(DEVICE)
            labels = torch.tensor([CIFAR_CLASSES[l.item()] for l in labels]).to(DEVICE)
            opt.zero_grad()
            emb = model(imgs)
            loss = criterion(classifier(emb), labels)
            loss.backward()
            opt.step()
            total_loss += loss.item()
        scheduler.step()
        print(f"  epoch {epoch+1}/{epochs}  loss={total_loss/len(loader):.4f}")
    return model, classifier

def eval_acc(model, classifier, test_ds):
    model.eval(); classifier.eval()
    loader = torch.utils.data.DataLoader(test_ds, batch_size=64, num_workers=2)
    correct = total = 0
    with torch.no_grad():
        for imgs, labels in loader:
            imgs = imgs.to(DEVICE)
            labels = torch.tensor([CIFAR_CLASSES[l.item()] for l in labels]).to(DEVICE)
            pred = classifier(model(imgs)).argmax(1)
            correct += (pred == labels).sum().item()
            total += len(labels)
    return correct / total

if __name__ == "__main__":
    print("Loading dataset...")
    train_ds, test_ds = load_dataset()

    print("Building model...")
    model = EmbeddingExtractor("mobilenet_v3_small", 128)
    classifier = nn.Linear(128, 4)

    print("Training...")
    model, classifier = train(model, classifier, train_ds, epochs=20)

    acc = eval_acc(model, classifier, test_ds)
    print(f"Test accuracy: {acc:.3f}")

    torch.save(model.state_dict(), SAVE_DIR / "mobilenetv3_128d_trained.pt")
    torch.save(classifier.state_dict(), SAVE_DIR / "mobilenet_classifier_4cls.pt")
    print(f"Saved to {SAVE_DIR}/mobilenetv3_128d_trained.pt")
    print(f"Saved to {SAVE_DIR}/mobilenet_classifier_4cls.pt")
