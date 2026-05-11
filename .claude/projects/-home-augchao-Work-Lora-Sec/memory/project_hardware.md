---
name: Hardware Devices Obtained
description: 實體 ESP32-S3 LoRa 設備已取得，可進行硬體實驗以關閉 Reviewer Risk R2
type: project
---

ESP32-S3 設備已取得（2026-05-05），連接在 augustmacbook-pro，port: /dev/cu.usbmodem134201（USB JTAG serial debug unit, VID=0x303A）。

**Why:** 論文最大 reviewer risk R2（severity: medium）是 HMAC 0.3ms 數字來自 mbedTLS benchmark 而非實測。有硬體後可直接量測，將 R2 status: open → closed，C3 confidence 0.92 → 0.98。

**How to apply:** 規劃分三 phase：Phase 1 HMAC micro-benchmark（1 device，2-3hr）→ Phase 2 PoC Attack Demo（2-3 devices with LoRa radio，4-6hr）→ Phase 3 OTA Latency（optional）。完整計畫在 HARDWARE_EXPERIMENT_PLAN.md。設備數量和 LoRa radio 型號待確認。
