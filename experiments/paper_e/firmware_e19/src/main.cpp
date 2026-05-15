/*
 * E19 — TFLite Micro MobileNetV3-Small 128-d Inference on EoRa PI (ESP32-S3)
 *
 * Measures: latency (ms), memory footprint, output embedding norm.
 * Input: dummy 96×96 RGB image (all 0.5 = grey), plus one ramp image.
 * No OV2640 required for latency/memory profiling.
 *
 * Expected output (from Python reference):
 *   Embedding: 128 float32 values, unit-norm (||e||₂ ≈ 1.0)
 *
 * Hardware: EoRa PI (ESP32-S3, 8 MB flash, 2 MB PSRAM)
 * USB CDC: GPIO19/20 (ARDUINO_USB_CDC_ON_BOOT=1)
 */

#include <Arduino.h>
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "mobilenetv3_128d_int8.h"

// ─── Tensor arena ──────────────────────────────────────────────────────────
static size_t kArenaSize = 1900 * 1024;  // 1.9 MB — fits in 2045 KB PSRAM with overhead
static uint8_t *tensor_arena = nullptr;

// ─── TFLite globals ────────────────────────────────────────────────────────
static tflite::MicroErrorReporter micro_error_reporter;
static tflite::ErrorReporter *error_reporter = &micro_error_reporter;
static const tflite::Model *model_ptr = nullptr;
static tflite::MicroInterpreter *interpreter = nullptr;
static TfLiteTensor *input_tensor  = nullptr;
static TfLiteTensor *output_tensor = nullptr;

// ─── Op resolver — register ops used by MobileNetV3-Small INT8 ─────────────
// Ops: Conv2D, DepthwiseConv2D, AveragePool2D, FullyConnected, HardSwish,
//      Mul, Add, Reshape, ResizeBilinear, Mean, L2Normalization
using Resolver = tflite::MicroMutableOpResolver<15>;
static Resolver resolver;

static void registerOps() {
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddAveragePool2D();
    resolver.AddFullyConnected();
    resolver.AddHardSwish();
    resolver.AddMul();
    resolver.AddAdd();
    resolver.AddReshape();
    resolver.AddResizeBilinear();
    resolver.AddMean();
    resolver.AddL2Normalization();
    resolver.AddQuantize();
    resolver.AddDequantize();
    resolver.AddPad();
    resolver.AddSub();
}

// ─── Normalise uint8 pixel to float ────────────────────────────────────────
// ImageNet normalisation: mean=[0.485,0.456,0.406], std=[0.229,0.224,0.225]
static const float MEAN[3] = {0.485f, 0.456f, 0.406f};
static const float STD[3]  = {0.229f, 0.224f, 0.225f};

static inline float normPixel(uint8_t val, int ch) {
    return (val / 255.0f - MEAN[ch]) / STD[ch];
}

// ─── Fill input tensor from raw RGB bytes ──────────────────────────────────
// Input tensor is FLOAT32 [1,96,96,3] based on model signature.
static void fillInput(const uint8_t *rgb_hwc, int h, int w) {
    float *in = input_tensor->data.f;
    for (int i = 0; i < h * w * 3; i += 3) {
        int px = i / 3;
        (void)px;
        for (int c = 0; c < 3; c++)
            *in++ = normPixel(rgb_hwc[i + c], c);
    }
}

// ─── Run one inference, return latency in ms ───────────────────────────────
static uint32_t runInference(const uint8_t *rgb_hwc) {
    fillInput(rgb_hwc, 96, 96);
    uint32_t t0 = millis();
    TfLiteStatus status = interpreter->Invoke();
    uint32_t dt = millis() - t0;
    if (status != kTfLiteOk) {
        Serial.println("ERROR: Invoke() failed");
    }
    return dt;
}

// ─── Compute L2 norm of output ─────────────────────────────────────────────
static float outputNorm() {
    float *out = output_tensor->data.f;
    float sum = 0;
    for (int i = 0; i < 128; i++) sum += out[i] * out[i];
    return sqrtf(sum);
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(2000);  // wait for USB CDC
    Serial.println("\n=== E19 TFLite Micro MobileNetV3-Small 128-d ===");

    // Report memory
    Serial.printf("Internal free heap: %u KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf("PSRAM total: %u KB  free: %u KB\n",
        ESP.getPsramSize() / 1024, ESP.getFreePsram() / 1024);

    // Allocate tensor arena from PSRAM, fallback to heap
    if (ESP.getPsramSize() > 0) {
        tensor_arena = (uint8_t *)ps_malloc(kArenaSize);
        Serial.printf("Tensor arena: %u KB from PSRAM\n", kArenaSize / 1024);
    }
    if (!tensor_arena) {
        // Fallback: smaller arena from internal RAM
        const size_t kFallbackArena = 300 * 1024;
        Serial.printf("PSRAM unavailable — trying %u KB internal RAM...\n",
                      kFallbackArena / 1024);
        tensor_arena = (uint8_t *)malloc(kFallbackArena);
        if (tensor_arena) {
            kArenaSize = kFallbackArena;
        }
    }
    if (!tensor_arena) {
        Serial.println("FATAL: allocation failed");
        while (1) {}
    }

    // Load model
    model_ptr = tflite::GetModel(mobilenetv3_128d_int8);
    if (model_ptr->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("FATAL: model schema %d != runtime %d\n",
                      model_ptr->version(), TFLITE_SCHEMA_VERSION);
        while (1) {}
    }
    Serial.printf("Model loaded: %u bytes\n", mobilenetv3_128d_int8_len);

    // Register ops and build interpreter
    registerOps();
    static tflite::MicroInterpreter static_interp(
        model_ptr, resolver, tensor_arena, kArenaSize, error_reporter);
    interpreter = &static_interp;

    TfLiteStatus alloc_status = interpreter->AllocateTensors();
    if (alloc_status != kTfLiteOk) {
        Serial.println("FATAL: AllocateTensors() failed");
        while (1) {}
    }
    size_t used = interpreter->arena_used_bytes();
    Serial.printf("Arena used: %u KB / %u KB\n", used / 1024, kArenaSize / 1024);

    input_tensor  = interpreter->input(0);
    output_tensor = interpreter->output(0);
    Serial.printf("Input  shape: [%d,%d,%d,%d] type=%d\n",
        input_tensor->dims->data[0], input_tensor->dims->data[1],
        input_tensor->dims->data[2], input_tensor->dims->data[3],
        input_tensor->type);
    Serial.printf("Output shape: [%d,%d] type=%d\n",
        output_tensor->dims->data[0], output_tensor->dims->data[1],
        output_tensor->type);

    // ── Warmup run ──────────────────────────────────────────────────────────
    static uint8_t grey_img[96 * 96 * 3];
    memset(grey_img, 128, sizeof(grey_img));  // 0.5 grey
    Serial.println("Warmup...");
    runInference(grey_img);

    // ── Benchmark: 10 runs ──────────────────────────────────────────────────
    Serial.println("Benchmarking 10 runs...");
    uint32_t total_ms = 0;
    for (int i = 0; i < 10; i++) {
        uint32_t dt = runInference(grey_img);
        total_ms += dt;
        Serial.printf("  run %02d: %u ms  ||e||=%.4f\n", i, dt, outputNorm());
    }
    Serial.printf("\nMean latency: %.1f ms over 10 runs\n", total_ms / 10.0f);
    Serial.printf("Throughput:   %.2f fps\n", 1000.0f / (total_ms / 10.0f));

    // ── Ramp image (synthetic gradient) ────────────────────────────────────
    static uint8_t ramp_img[96 * 96 * 3];
    for (int i = 0; i < 96 * 96; i++) {
        uint8_t v = (uint8_t)(i % 256);
        ramp_img[i * 3 + 0] = v;
        ramp_img[i * 3 + 1] = (uint8_t)(255 - v);
        ramp_img[i * 3 + 2] = (uint8_t)(v / 2 + 64);
    }
    uint32_t dt_ramp = runInference(ramp_img);
    float *emb = output_tensor->data.f;
    Serial.println("\nRamp image embedding (first 8 dims):");
    for (int i = 0; i < 8; i++) Serial.printf("  e[%d] = %.5f\n", i, emb[i]);
    Serial.printf("  ||e||₂ = %.4f  (should be ≈ 1.0)\n", outputNorm());
    Serial.printf("  latency = %u ms\n", dt_ramp);

    Serial.println("\n=== E19 DONE ===");
}

void loop() {
    // After benchmark, idle
    delay(5000);
    Serial.println("[idle] rerun benchmark? press reset");
}
