/*
 * E20 — PIR Event-Triggered Energy Measurement on EoRa PI (ESP32-S3)
 *
 * Goal: measure duty-cycle savings from PIR-triggered inference+TX vs always-on.
 * No real TFLite model needed — inference is simulated by a 6203 ms delay,
 * matching the measured E19 latency.
 *
 * Hardware: EoRa PI (ESP32-S3 + SX1262 LoRa)
 * PIR sensor: HC-SR501 connected to GPIO 14
 *   - VCC → 3.3 V (HC-SR501 works from 3.3 V–5 V; sensitivity may be lower at 3.3 V)
 *   - GND → GND
 *   - OUT → GPIO 14 (goes HIGH for ~2 s on motion)
 *
 * Power states:
 *   Idle  : deep sleep, GPIO14 ext0 wake — ~10 µA
 *   Active: inference (simulated) + LoRa TX — ~80 mA for ~6253 ms
 *
 * USB CDC: ARDUINO_USB_CDC_ON_BOOT=1 (GPIO19/20), 115200 baud
 *
 * Compile: pio run -e eora_pi_e20
 * Flash  : pio run -e eora_pi_e20 -t upload
 */

#include <Arduino.h>
#include "esp_sleep.h"
#include "driver/gpio.h"

// ─── Pin assignment ───────────────────────────────────────────────────────────
// GPIO 14: PIR signal input (HC-SR501 OUT).
// Change this if you wire PIR to a different pin; update esp_sleep_enable_ext0_wakeup() too.
static constexpr gpio_num_t PIR_PIN = GPIO_NUM_14;

// ─── Timing constants (all in ms) ────────────────────────────────────────────
static constexpr uint32_t INFERENCE_MS = 6203;  // E19 measured TFLite latency
static constexpr uint32_t TX_MS        =   50;  // LoRa TX ~142 bytes @ SF9 BW125
static constexpr uint32_t ACTIVE_MS    = INFERENCE_MS + TX_MS;  // 6253 ms

// ─── Energy model constants ───────────────────────────────────────────────────
static constexpr float IDLE_CURRENT_MA   =  0.01f;  // 10 µA deep sleep
static constexpr float ACTIVE_CURRENT_MA = 80.0f;   // ESP32-S3 + SX1262 TX combined
static constexpr float BATTERY_MAH       = 3000.0f;

// ─── RTC-retained wake counter ───────────────────────────────────────────────
// Survives deep sleep so we can count total events across wakes.
RTC_DATA_ATTR static uint32_t wake_count = 0;
RTC_DATA_ATTR static uint32_t total_active_ms = 0;  // cumulative active time (ms)

// ─── Helpers ─────────────────────────────────────────────────────────────────

static void printSeparator() {
    Serial.println("----------------------------------------");
}

/*
 * Print energy statistics for this event and the running session total.
 *
 * duty_cycle  = active_time / (active_time + idle_time)
 *             Approximated here per-event as: active_ms / cycle_period_ms
 * avg_current = duty_cycle * active_mA + (1-duty_cycle) * idle_mA
 * battery_life_days = battery_mAh / avg_current_mA / 24
 *
 * We compute for a range of assumed inter-event intervals so the result is
 * self-contained without needing to know the real PIR trigger rate.
 */
static void printEnergyStats() {
    printSeparator();
    Serial.printf("active_ms        = %u\n", ACTIVE_MS);
    Serial.printf("idle_current_mA  = %.4f  (deep sleep ~10 µA)\n", IDLE_CURRENT_MA);
    Serial.printf("active_current_mA= %.1f\n", ACTIVE_CURRENT_MA);
    Serial.printf("battery_mAh      = %.0f\n", BATTERY_MAH);
    Serial.println();

    // Compute for representative event rates (events/hour)
    Serial.println("--- Duty-Cycle Table (per event rate) ---");
    Serial.printf("%-14s %-16s %-20s %-18s\n",
                  "Rate(ev/hr)", "Period(s)", "DutyCycle(%)", "BattLife(days)");

    const float rates[] = {1.0f, 5.0f, 10.0f, 30.0f, 60.0f};
    for (float rate : rates) {
        float period_ms   = 3600000.0f / rate;                     // ms between events
        float duty        = (float)ACTIVE_MS / period_ms;          // fraction
        float avg_mA      = duty * ACTIVE_CURRENT_MA + (1.0f - duty) * IDLE_CURRENT_MA;
        float life_days   = BATTERY_MAH / avg_mA / 24.0f;

        Serial.printf("%-14.0f %-16.1f %-20.4f %-18.1f\n",
                      rate,
                      period_ms / 1000.0f,
                      duty * 100.0f,
                      life_days);
    }

    // Always-on baseline: 80 mA continuous
    float baseline_life = BATTERY_MAH / ACTIVE_CURRENT_MA / 24.0f;
    Serial.println();
    Serial.printf("Always-on baseline (%.0f mA): %.2f days\n",
                  ACTIVE_CURRENT_MA, baseline_life);
    printSeparator();
}

// ─── Arduino setup() — runs on every wake (boot + deep-sleep wake) ───────────
void setup() {
    Serial.begin(115200);
    delay(300);  // let USB CDC enumerate; shorter than E19 since no model load

    wake_count++;

    // Check wake reason
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    bool pir_wake = (cause == ESP_SLEEP_WAKEUP_EXT0);

    if (!pir_wake) {
        // Cold boot: run one self-test cycle (simulates PIR event) before sleeping.
        // This lets serial monitoring verify firmware without physical PIR wiring.
        Serial.println("[E20] Cold boot — running self-test event (no PIR needed).");
        Serial.printf("  PIR pin: GPIO%d  (HC-SR501 OUT)\n", (int)PIR_PIN);
        // fall through to PIR-wake path below
    } else {

    } // end else (pir_wake)

    // ── PIR-triggered wake (or self-test on cold boot) ───────────────────────
    uint32_t t_wake = millis();
    Serial.printf("[E20] %s, t=%u ms  (event #%u)\n",
                  pir_wake ? "PIR triggered" : "Self-test", t_wake, wake_count);

    // ── Step 1: Simulated TFLite inference (6203 ms) ─────────────────────────
    Serial.printf("  Simulating inference... (%u ms)\n", INFERENCE_MS);
    delay(INFERENCE_MS);
    Serial.println("  Inference done.");

    // ── Step 2: Simulated LoRa TX (50 ms = ~142 bytes @ SF9 BW125) ───────────
    Serial.println("  Simulating LoRa TX (142-byte embedding, SF9, BW125)...");
    delay(TX_MS);
    Serial.println("  TX done.");

    // ── Step 3: Accumulate active time ───────────────────────────────────────
    total_active_ms += ACTIVE_MS;

    // ── Step 4: Print per-event energy stats ─────────────────────────────────
    Serial.println();
    Serial.println("=== E20 Energy Stats (this event) ===");
    printEnergyStats();

    // Running session summary (across all wakes since cold boot)
    Serial.println("=== Session Summary ===");
    Serial.printf("  Total wake events     : %u\n", wake_count);
    Serial.printf("  Total active time     : %u ms\n", total_active_ms);
    uint32_t session_ms = millis();  // crude proxy for session wall time
    if (session_ms > 0) {
        float duty = (float)total_active_ms / (float)(wake_count * 3600000UL / 10UL);
        // Assume 1-event/10-min rate for session duty estimate
        (void)duty;
    }

    // ── Step 5: Return to deep sleep (PIR wake) or idle loop (cold boot) ────
    if (pir_wake) {
        Serial.println("[E20] Returning to deep sleep (PIR wake armed on GPIO14 HIGH)...");
        Serial.flush();
        delay(50);
        esp_sleep_enable_ext0_wakeup(PIR_PIN, 1);
        esp_deep_sleep_start();
        // unreachable
    } else {
        // Cold boot self-test: stay awake so USB CDC remains visible
        Serial.println("[E20] Self-test complete. Staying awake (USB CDC). Reset to re-test.");
        Serial.println("[E20] In deployment, cold boot goes to deep sleep immediately.");
        while (1) { delay(5000); Serial.println("[E20] idle..."); }
    }
}

void loop() {
    // Never reached — device is always in deep sleep between PIR events
}
