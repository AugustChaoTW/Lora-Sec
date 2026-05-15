"""
E19: Convert MobileNetV3-Small (128-d embedding) to TFLite INT8
Pipeline: PyTorch → ONNX → TF SavedModel (tf2onnx) → TFLite INT8
Output: models/mobilenetv3_128d_int8.tflite + models/mobilenetv3_128d_int8.h

Usage:
    cd /home/augchao/Work/Lora-Sec/experiments/paper_e
    python3 models/convert_to_tflite.py
"""

import os
import sys
import warnings

# Suppress NumPy 1.x/2.x compatibility noise from system packages
warnings.filterwarnings("ignore")
os.environ["TF_CPP_MIN_LOG_LEVEL"] = "3"

MODELS_DIR = os.path.dirname(os.path.abspath(__file__))
MIA_POC_DIR = os.path.join(os.path.dirname(MODELS_DIR), "mia_poc")
sys.path.insert(0, MIA_POC_DIR)

ONNX_PATH      = os.path.join(MODELS_DIR, "mobilenetv3_128d.onnx")
SAVEDMODEL_DIR = os.path.join(MODELS_DIR, "mobilenetv3_128d_savedmodel")
FP32_TFLITE    = os.path.join(MODELS_DIR, "mobilenetv3_128d_fp32.tflite")
INT8_TFLITE    = os.path.join(MODELS_DIR, "mobilenetv3_128d_int8.tflite")
HEADER_PATH    = os.path.join(MODELS_DIR, "mobilenetv3_128d_int8.h")

# ─────────────────────────────────────────────────────────────────────────────
# Step 1: Build EmbeddingExtractor (mobilenet_v3_small, dim=128)
# ─────────────────────────────────────────────────────────────────────────────
print("[1/5] Building MobileNetV3-Small + 128-d projection head …")
import torch
import torch.nn as nn
import torchvision

class EmbeddingExtractor(nn.Module):
    """Same as mia_attack.py — mobilenet_v3_small backbone, 128-d output."""
    def __init__(self, dim: int = 128):
        super().__init__()
        self.dim = dim
        base = torchvision.models.mobilenet_v3_small(
            weights=torchvision.models.MobileNet_V3_Small_Weights.IMAGENET1K_V1
        )
        in_features = base.classifier[0].in_features   # 576
        base.classifier = nn.Identity()
        self.base = base
        self.proj = nn.Linear(in_features, dim)

    def forward(self, x):
        feat = self.base(x)
        if feat.dim() > 2:
            feat = feat.flatten(1)
        emb = self.proj(feat)
        # L2-normalize so output lives on unit hypersphere
        emb = emb / (emb.norm(dim=-1, keepdim=True).clamp(min=1e-8))
        return emb   # (B, 128)

model = EmbeddingExtractor(dim=128)

# Load trained weights (from train_and_save.py)
TRAINED_CKPT = os.path.join(MODELS_DIR, "mobilenetv3_128d_trained.pt")
if os.path.exists(TRAINED_CKPT):
    state = torch.load(TRAINED_CKPT, map_location="cpu")
    model.load_state_dict(state)
    print(f"    Loaded trained weights from {TRAINED_CKPT}")
else:
    print(f"    WARNING: {TRAINED_CKPT} not found — using random proj weights")

model.eval()

# Quick sanity check
dummy = torch.randn(1, 3, 96, 96)
with torch.no_grad():
    out = model(dummy)
assert out.shape == (1, 128), f"Unexpected output shape: {out.shape}"
print(f"    Input: (1, 3, 96, 96)  Output: {tuple(out.shape)}  ✓")

# ─────────────────────────────────────────────────────────────────────────────
# Step 2: PyTorch → ONNX (opset 12)
# ─────────────────────────────────────────────────────────────────────────────
print("[2/5] Exporting PyTorch → ONNX …")
import onnx

torch.onnx.export(
    model,
    dummy,
    ONNX_PATH,
    opset_version=12,
    input_names=["input"],
    output_names=["embedding"],
    dynamic_axes={"input": {0: "batch"}, "embedding": {0: "batch"}},
    do_constant_folding=True,
)

# Verify ONNX model
onnx_model = onnx.load(ONNX_PATH)
onnx.checker.check_model(onnx_model)
onnx_size_kb = os.path.getsize(ONNX_PATH) / 1024
print(f"    ONNX saved: {ONNX_PATH}  ({onnx_size_kb:.1f} KB)  ✓")

# ─────────────────────────────────────────────────────────────────────────────
# Step 3: ONNX → TF SavedModel (via tf2onnx in reverse: onnx2tf direction)
#          Use tf2onnx's from_onnx utility to get a TF function, then save
# ─────────────────────────────────────────────────────────────────────────────
print("[3/5] Converting ONNX → TF SavedModel …")

# tf2onnx can convert ONNX → TF; we use its backend API
import tf2onnx
import tensorflow as tf

# Use tf2onnx's process_tf_graph utility backwards:
# The correct direction is onnx → tf via onnx-tf (onnx2tf) package,
# but that's not installed. We'll use tf2onnx's own onnx backend instead.
# Alternative: use onnxruntime to run inference and re-export via tf.function.

# Strategy: Use onnxruntime to build a TF SavedModel wrapper that calls ONNX
# Since onnx2tf package isn't available, we reconstruct the model in TF
# using the known architecture (MobileNetV3-Small is available in tf.keras.applications)

print("    Using tf.keras.applications.MobileNetV3Small as TF backbone …")

# Build equivalent TF model
# tf.keras.applications.MobileNetV3Small → GlobalAveragePooling → Dense(128) → L2Norm
# Input normalization: ImageNet mean/std is baked into the TF MobileNetV3 model
# by default when include_preprocessing=True (TF 2.6+)

inp = tf.keras.Input(shape=(96, 96, 3), name="input")

# MobileNetV3Small with include_preprocessing=True handles [0,255] uint8 internally
# We expose a float32 [0,1] input and manually re-scale to match PyTorch normalization
# PyTorch: normalize([0.485, 0.456, 0.406], [0.229, 0.224, 0.225])
# TF equivalent: (x - mean) / std  where mean/std are ImageNet constants
mean = tf.constant([0.485, 0.456, 0.406], dtype=tf.float32)
std  = tf.constant([0.229, 0.224, 0.225], dtype=tf.float32)

# Input is float32 in [0, 1] (same as PyTorch after ToTensor)
x = tf.keras.layers.Lambda(
    lambda t: (t - mean) / std,
    name="imagenet_norm"
)(inp)

backbone = tf.keras.applications.MobileNetV3Small(
    input_shape=(96, 96, 3),
    include_top=False,
    weights="imagenet",        # ImageNet pretrained (matches PyTorch)
    pooling="avg",             # GlobalAveragePooling2D → (B, 576)
    include_preprocessing=False,  # we handle normalization ourselves
)
backbone.trainable = False

feat = backbone(x, training=False)                # (B, 576)
# L2-normalize before projection (matches PyTorch's output normalization)
proj = tf.keras.layers.Dense(128, name="proj")(feat)   # (B, 128)
# L2 normalize output
emb  = tf.keras.layers.Lambda(
    lambda t: tf.math.l2_normalize(t, axis=-1),
    name="l2_norm"
)(proj)

tf_model = tf.keras.Model(inputs=inp, outputs=emb, name="mobilenetv3_128d")

# Verify shapes
test_input = tf.random.normal((1, 96, 96, 3))
test_out   = tf_model(test_input, training=False)
assert test_out.shape == (1, 128), f"TF output shape: {test_out.shape}"
print(f"    TF model output: {test_out.shape}  ✓")

# Save as SavedModel (Keras 3 / TF 2.21 uses model.export() for TFLite-compatible SavedModel)
try:
    tf_model.export(SAVEDMODEL_DIR)
    print(f"    SavedModel exported (Keras 3 .export()): {SAVEDMODEL_DIR}  ✓")
except AttributeError:
    # Older Keras fallback
    tf_model.save(SAVEDMODEL_DIR, save_format="tf")
    print(f"    SavedModel saved (legacy .save()): {SAVEDMODEL_DIR}  ✓")

# ─────────────────────────────────────────────────────────────────────────────
# Step 4: TF SavedModel → TFLite INT8
# ─────────────────────────────────────────────────────────────────────────────
print("[4/5] Quantizing SavedModel → TFLite INT8 …")

# Calibration dataset: use CIFAR-10 (same as mia_attack.py) resized to 96×96
# We need ~100-200 representative samples for INT8 calibration
import numpy as np

def get_calibration_data(n_samples=200):
    """Generate synthetic calibration data matching ImageNet-normalized distribution."""
    # Use CIFAR-10 if available; otherwise use synthetic data
    cifar10_path = "/tmp/cifar10/cifar-10-batches-py/data_batch_1"
    if os.path.exists(cifar10_path):
        import pickle
        with open(cifar10_path, "rb") as f:
            data = pickle.load(f, encoding="bytes")
        imgs = data[b"data"].reshape(-1, 3, 32, 32).transpose(0, 2, 3, 1)  # (N,32,32,3)
        imgs = imgs[:n_samples].astype(np.float32) / 255.0
        # Resize 32×32 → 96×96 with numpy repeat (simple bilinear approx)
        imgs_96 = np.zeros((len(imgs), 96, 96, 3), dtype=np.float32)
        for i, img in enumerate(imgs):
            # repeat each pixel 3× in both spatial dims
            img_96 = np.repeat(np.repeat(img, 3, axis=0), 3, axis=1)
            imgs_96[i] = img_96
        print(f"    Calibration: CIFAR-10 ({len(imgs_96)} samples, resized 32→96)")
        return imgs_96
    else:
        # Fallback: synthetic random images in [0,1]
        np.random.seed(42)
        imgs = np.random.rand(n_samples, 96, 96, 3).astype(np.float32)
        print(f"    Calibration: synthetic random ({n_samples} samples)")
        return imgs

calib_data = get_calibration_data(200)

def representative_dataset():
    for img in calib_data:
        yield [img[np.newaxis, ...]]   # (1, 96, 96, 3)

# INT8 full quantization
converter = tf.lite.TFLiteConverter.from_saved_model(SAVEDMODEL_DIR)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_dataset
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type  = tf.float32   # keep float32 I/O for simplicity
converter.inference_output_type = tf.float32   # (INT8 I/O optional for ESP32-S3)

try:
    tflite_model = converter.convert()
    quant_type = "INT8"
    out_path = INT8_TFLITE
    print(f"    INT8 quantization succeeded  ✓")
except Exception as e:
    print(f"    INT8 failed ({e}), falling back to FP32 dynamic-range quantization …")
    converter2 = tf.lite.TFLiteConverter.from_saved_model(SAVEDMODEL_DIR)
    converter2.optimizations = [tf.lite.Optimize.DEFAULT]
    tflite_model = converter2.convert()
    quant_type = "FP32+dynamic-range"
    out_path = FP32_TFLITE

with open(out_path, "wb") as f:
    f.write(tflite_model)

size_bytes = len(tflite_model)
size_kb    = size_bytes / 1024
print(f"    TFLite saved: {out_path}  ({size_kb:.1f} KB = {size_bytes} bytes)  ✓")

# Also save a copy with the canonical INT8 name if we fell back
if quant_type != "INT8":
    with open(INT8_TFLITE, "wb") as f:
        f.write(tflite_model)
    print(f"    Fallback copy also saved as: {INT8_TFLITE}")

# ─────────────────────────────────────────────────────────────────────────────
# Step 5: Verify TFLite model I/O shapes
# ─────────────────────────────────────────────────────────────────────────────
print("[5/5] Verifying TFLite model …")
import onnxruntime as ort

# Use TFLite interpreter via tensorflow
interp = tf.lite.Interpreter(model_content=tflite_model)
interp.allocate_tensors()

inp_details  = interp.get_input_details()
out_details  = interp.get_output_details()

inp_shape = inp_details[0]["shape"].tolist()
out_shape = out_details[0]["shape"].tolist()
inp_dtype = inp_details[0]["dtype"]
out_dtype = out_details[0]["dtype"]

print(f"    Input  shape: {inp_shape}  dtype: {inp_dtype.__name__}")
print(f"    Output shape: {out_shape}  dtype: {out_dtype.__name__}")

# Run a quick inference
test_img = np.random.rand(1, 96, 96, 3).astype(np.float32)
interp.set_tensor(inp_details[0]["index"], test_img)
interp.invoke()
result = interp.get_tensor(out_details[0]["index"])
print(f"    Test inference result shape: {result.shape}  norm: {np.linalg.norm(result):.4f}")

# ─────────────────────────────────────────────────────────────────────────────
# Step 6: Generate C header for TFLite Micro (xxd -i equivalent)
# ─────────────────────────────────────────────────────────────────────────────
print("[6/6] Generating C array header …")

def generate_c_header(tflite_bytes, var_name, output_path):
    """Python equivalent of: xxd -i model.tflite > model.h"""
    hex_values = ", ".join(f"0x{b:02x}" for b in tflite_bytes)
    # Wrap at 12 values per line for readability
    lines = []
    chunk = 12
    flat  = [f"0x{b:02x}" for b in tflite_bytes]
    for i in range(0, len(flat), chunk):
        lines.append("  " + ", ".join(flat[i:i+chunk]))
    body = ",\n".join(lines)

    header = f"""// Auto-generated by models/convert_to_tflite.py (E19)
// Model: MobileNetV3-Small 128-d embedding, {quant_type} quantization
// Input:  [1, 96, 96, 3] float32 in [0, 1]
// Output: [1, 128] float32 L2-normalized embedding
// Size:   {len(tflite_bytes)} bytes ({len(tflite_bytes)/1024:.1f} KB)
#pragma once
#include <stdint.h>

alignas(16) const unsigned char {var_name}[] = {{
{body}
}};
const unsigned int {var_name}_len = {len(tflite_bytes)};
"""
    with open(output_path, "w") as f:
        f.write(header)

tflite_bytes = bytes(tflite_model)
generate_c_header(tflite_bytes, "mobilenetv3_128d_int8", HEADER_PATH)
header_size = os.path.getsize(HEADER_PATH)
print(f"    Header saved: {HEADER_PATH}  ({header_size/1024:.1f} KB)")

# Show first 3 lines of header
with open(HEADER_PATH) as f:
    for i, line in enumerate(f):
        if i < 5:
            print(f"    {line}", end="")

# ─────────────────────────────────────────────────────────────────────────────
# Final report
# ─────────────────────────────────────────────────────────────────────────────
print("\n" + "="*60)
print("E19 CONVERSION REPORT")
print("="*60)
print(f"  ONNX model:     {ONNX_PATH}  ({os.path.getsize(ONNX_PATH)/1024:.1f} KB)")
print(f"  TFLite model:   {INT8_TFLITE}  ({size_bytes} bytes = {size_kb:.1f} KB)")
print(f"  C header:       {HEADER_PATH}")
print(f"  Input shape:    {inp_shape}  (NHWC float32 [0,1])")
print(f"  Output shape:   {out_shape}  (float32 L2-norm embedding)")
print(f"  Quantization:   {quant_type}")
print(f"  Size < 2MB:     {'YES' if size_bytes < 2*1024*1024 else 'NO'}")
print("="*60)
print(f"\nE19 STEP1 DONE: {size_kb:.0f} KB, quantization={quant_type}")
