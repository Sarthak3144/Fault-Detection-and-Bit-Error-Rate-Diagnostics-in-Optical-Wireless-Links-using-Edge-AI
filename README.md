# Fault Detection & Bit Error Rate Diagnostics in Optical Wireless Links Using Edge-AI

An autonomous, point-to-point Free-Space Optical Wireless Communication (OWC) testbed integrated with real-time, on-device Edge-AI link diagnostics. The receiver node performs local telemetry extraction, bit-error recovery, and operating condition classification directly on an ESP32-S3 microcontroller using TensorFlow Lite Micro.

---

## **System Architecture**

```text
+---------------------------+             +-------------------------------------------------------+
|   ESP32 Transmitter Node  |             |         ESP32-S3 Receiver Node with Edge-AI           |
|                           |  Free Space |                                                       |
| [ Known Binary Payload ]  |   Optical   | [ Photodiode BPW34 + LM358 Signal Conditioning ]     |
|             |             |    Beam     |                           |                           |
| [ OOK Modulation Driver ] | ------------> [ Bit Recovery & Telemetry Feature Extraction ]     |
|             |             |             |                           |                           |
| [ 650nm HW-493 Laser ]    |             | [ StandardScaler Normalization (Mean/Std Embed) ]     |
+---------------------------+             |                           |                           |
                                          | [ TensorFlow Lite Micro INT8 Inference Engine ]       |
                                          |                           |                           |
                                          | [ Real-Time Diagnostic Condition Output ]             |
                                          +-------------------------------------------------------+



## Diagnostic Features & Operating Conditions**

### Input Features (Telemetry Vector)**
1. Signal Voltage (V): Direct peak DC voltage proportional to received optical intensity.
2. Bit Error Rate ({BER}): Quantifies communication reliability by comparing recovered bits against the known payload.
3. Signal-to-Noise Ratio ({SNR}): Ratio of peak AC signal amplitude to background noise floor ($20 \log_{10}(V_{\text{signal}} / V_{\text{noise}})$).
4. **Ambient Light ($\text{Lux}$): Ambient baseline illuminance derived from photodiode DC offset calibration.

### Target Diagnostic Classes
* Class 0: Normal Condition (Clean line-of-sight)
* Class 1: Optical Filter / Polarization Attenuation
* Class 2: High Ambient Light Interference
* Class 3: Severe Misalignment / Obstruction



## **Model Benchmarks & Deployment Specifications**

The dataset consists of 395 empirical telemetry samples** collected directly from physical hardware experiments (80% Train / 20% Test split).

| Metric / Model | Baseline Neural Network (FP32) | **Quantized INT8 Model (Deployed) | Random Forest Classifier |
| :--- | :--- | :--- | :--- |
|  Accuracy** | 78.48% | 74.68% | 77.22% |
|  Memory Size** | ~30–40 kB | **3.32 kB** | High tree-logic overhead |
|  Execution Platform | Host / PC Python | ESP32-S3 (TensorFlow Lite Micro) | Host / PC Python |
|  Latency / Power | High / Cloud Dependent | **Ultra-Low / Fully Autonomous** | Moderate |



## **Hardware Setup & Components**

* Transmitter Node: ESP32 Board, 650 nm HW-493 Laser Module, 2N2222 NPN Transistor, 1kohm Base Resistor.
* Receiver Node: ESP32-S3 Microcontroller, BPW34 PIN Photodiode, LM358 Op-Amp Signal Conditioning Circuit.
* Transmission Specifications: 1000 bit/s OOK with NRZ-L line coding, 100 ms sync pulse, 20 ms guard interval, majority voting bit recovery (5 samples per bit across 60% central window).



