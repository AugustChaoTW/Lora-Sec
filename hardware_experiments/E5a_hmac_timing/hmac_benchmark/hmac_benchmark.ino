/*
 * E5a: HMAC-SHA256 Timing Benchmark for LoRa-Sec Paper
 *
 * Board  : Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)
 * Target : Arduino IDE → Board: "Heltec WiFi LoRa 32(V3)"
 *          (Partition: Minimal SPIFFS, CPU: 240MHz)
 *
 * Measures HMAC-SHA256 computation time on ESP32-S3 @ 240 MHz
 * using mbedTLS (included in Arduino ESP32 framework).
 * NOTE: LoRa radio is NOT used in this sketch — pure CPU benchmark.
 *       (Phase 2 / E5b will use LoRa @ 923 MHz, AS923 Plan, Taiwan)
 *
 * Parameters match the paper claim (IOTJ-Aug.tex §6.1):
 *   - 64-byte payload  (LoRaMesher control message size)
 *   - 16-byte HMAC tag (truncated SHA256)
 *   - 10,000 iterations
 *   - Expected: ~300 µs mean
 *
 * Output (115200 baud serial):
 *   JSON lines for easy parsing + human-readable summary
 */

#include "mbedtls/md.h"
#include "esp_timer.h"
#include <math.h>

// --- Experiment parameters ---
static const int    N_WARMUP     = 100;
static const int    N_ITER       = 10000;
static const int    PAYLOAD_LEN  = 64;   // bytes — LoRaMesher Hello/RouteReply
static const int    TAG_LEN      = 16;   // bytes — truncated HMAC-SHA256
static const int    KEY_LEN      = 16;   // bytes — 128-bit long-term key

// Stable test vectors (fixed to prevent compiler from optimising away)
static const uint8_t KEY[KEY_LEN] = {
  0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};
static uint8_t payload[PAYLOAD_LEN];
static uint8_t tag[32];  // full SHA256 output (we truncate to TAG_LEN)

static double samples[N_ITER];
static double sorted[N_ITER];  // global to avoid 80KB stack overflow

// --- Helpers ---
static double mean(double *a, int n) {
  double s = 0;
  for (int i = 0; i < n; i++) s += a[i];
  return s / n;
}

static double stddev(double *a, int n, double m) {
  double s = 0;
  for (int i = 0; i < n; i++) s += (a[i] - m) * (a[i] - m);
  return sqrt(s / n);
}

static double percentile(double *sorted, int n, double p) {
  int idx = (int)(p / 100.0 * n);
  if (idx >= n) idx = n - 1;
  return sorted[idx];
}

static int cmp_double(const void *a, const void *b) {
  double da = *(double *)a, db = *(double *)b;
  return (da > db) - (da < db);
}

// --- Single HMAC-SHA256 computation ---
static void do_hmac(const uint8_t *key, size_t klen,
                    const uint8_t *data, size_t dlen,
                    uint8_t *out) {
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, key, klen);
  mbedtls_md_hmac_update(&ctx, data, dlen);
  mbedtls_md_hmac_finish(&ctx, out);
  mbedtls_md_free(&ctx);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Fill payload with incrementing bytes (representative control message)
  for (int i = 0; i < PAYLOAD_LEN; i++) payload[i] = (uint8_t)i;

  Serial.println();
  Serial.println("=== E5a: HMAC-SHA256 Benchmark ===");
  Serial.printf("Chip:     %s rev%d\n",
    ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("CPU freq: %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("SDK:      %s\n", ESP.getSdkVersion());
  Serial.printf("Payload:  %d bytes | Tag: %d bytes | Key: %d bytes\n",
    PAYLOAD_LEN, TAG_LEN, KEY_LEN);
  Serial.printf("Warm-up:  %d | Iterations: %d\n\n", N_WARMUP, N_ITER);

  // Warm-up: allow CPU caches / mbedTLS internal state to settle
  Serial.println("Warming up...");
  for (int i = 0; i < N_WARMUP; i++) {
    do_hmac(KEY, KEY_LEN, payload, PAYLOAD_LEN, tag);
    payload[0] ^= tag[0];  // prevent dead-code elimination
  }

  // Main benchmark
  Serial.println("Benchmarking...");
  for (int i = 0; i < N_ITER; i++) {
    int64_t t0 = esp_timer_get_time();  // microseconds
    do_hmac(KEY, KEY_LEN, payload, PAYLOAD_LEN, tag);
    int64_t t1 = esp_timer_get_time();
    samples[i] = (double)(t1 - t0);
    payload[i % PAYLOAD_LEN] ^= tag[0];  // prevent loop unrolling

    if ((i + 1) % 1000 == 0) {
      Serial.printf("  %d / %d done\n", i + 1, N_ITER);
    }
  }

  // Statistics
  double m   = mean(samples, N_ITER);
  double sd  = stddev(samples, N_ITER, m);
  double mn  = samples[0], mx = samples[0];
  for (int i = 1; i < N_ITER; i++) {
    if (samples[i] < mn) mn = samples[i];
    if (samples[i] > mx) mx = samples[i];
  }

  // Sort for percentiles (global array — avoid stack overflow)
  memcpy(sorted, samples, sizeof(samples));
  qsort(sorted, N_ITER, sizeof(double), cmp_double);

  double p50 = percentile(sorted, N_ITER, 50);
  double p95 = percentile(sorted, N_ITER, 95);
  double p99 = percentile(sorted, N_ITER, 99);

  Serial.println("\n=== RESULTS ===");
  Serial.printf("mean:   %.1f µs  (%.3f ms)\n", m,  m  / 1000.0);
  Serial.printf("std:    %.1f µs\n", sd);
  Serial.printf("min:    %.1f µs\n", mn);
  Serial.printf("p50:    %.1f µs\n", p50);
  Serial.printf("p95:    %.1f µs\n", p95);
  Serial.printf("p99:    %.1f µs\n", p99);
  Serial.printf("max:    %.1f µs\n", mx);
  Serial.printf("LoRa air-time ref: ~165000 µs  "
                "→ HMAC overhead: %.2f%%\n", m / 165000.0 * 100.0);
  Serial.println();

  // Machine-readable JSON for parse_results.py
  Serial.println("JSON_BEGIN");
  Serial.printf("{"
    "\"exp\":\"E5a\","
    "\"chip\":\"%s\","
    "\"chip_rev\":%d,"
    "\"cpu_mhz\":%u,"
    "\"sdk\":\"%s\","
    "\"payload_bytes\":%d,"
    "\"tag_bytes\":%d,"
    "\"key_bytes\":%d,"
    "\"n_iter\":%d,"
    "\"mean_us\":%.2f,"
    "\"std_us\":%.2f,"
    "\"min_us\":%.2f,"
    "\"p50_us\":%.2f,"
    "\"p95_us\":%.2f,"
    "\"p99_us\":%.2f,"
    "\"max_us\":%.2f,"
    "\"mean_ms\":%.4f,"
    "\"lora_airtime_ref_us\":165000,"
    "\"overhead_pct\":%.4f"
    "}\n",
    ESP.getChipModel(), ESP.getChipRevision(), ESP.getCpuFreqMHz(),
    ESP.getSdkVersion(),
    PAYLOAD_LEN, TAG_LEN, KEY_LEN, N_ITER,
    m, sd, mn, p50, p95, p99, mx,
    m / 1000.0,
    m / 165000.0 * 100.0
  );
  Serial.println("JSON_END");

  Serial.println("\nDone. Reset to run again.");
}

void loop() {
  // nothing — single-shot benchmark
  delay(10000);
}
