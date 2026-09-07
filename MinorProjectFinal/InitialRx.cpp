/*
 * ============================================================
 * OPTICAL LINK - RECEIVER (NRZ Decoded Story Stream)
 * Fixed: Null-byte sanitization forces printing garbled text
 * ============================================================
 * Hardware : Photodiode/Op-Amp -> ESP32 GPIO4
 * Threshold: Hardcoded 1700 ADC counts
 * Bit Rate : 1000 bps (1000 us per bit)
 * Scheme   : NRZ Decoding with 5-Sample Majority Voting
 * Metrics  : Signal Voltage, BER, SNR, Ambient Light
 * ============================================================
 */

#define ADC_PIN 4

// ============================================================
// TIMING & PAYLOAD PARAMETERS
// ============================================================
const uint32_t BIT_TIME_US   = 1000;   // 1000 us/bit (1 kbps)
const uint32_t SYNC_TIME_US  = 100000; // 100 ms
const uint32_t GUARD_TIME_US = 20000;  // 20 ms

const char EXPECTED_TEXT[] = 
  "A father went to say good night to his seven year old son, very well knowing that if he didn't his son would have trouble sleeping. "
  "It was a nightly routine between them. He entered the dimly lit room where his son waited under his blanket. "
  "With the first glance the father could tell there was something unusual about his son tonight, but couldn't put his finger on it. "
  "He looked the same but had a grin that drew from ear to ear. "
  "\"You okay, buddy?\" the father asked. "
  "The son nodded, still with the grin, before saying, \"Daddy, check for monsters under my bed.\" "
  "The father chuckled a bit before getting on his knees to check only to satisfy his son. "
  "There, under the bed, pale and afraid, was his son. His real son. He whispered, \"Daddy, there someone on my bed\".";

const int MSG_CHAR_LEN = sizeof(EXPECTED_TEXT) - 1; 
const int TOTAL_RUN_BITS = MSG_CHAR_LEN * 8;        

// Threshold & Detection Setup
const int HARDCODED_SYNC_THRESHOLD = 1700; 
const uint32_t SYNC_CONFIRM_MS      = 5;     
const uint32_t SYNC_WAIT_TIMEOUT_MS  = 15000; 
const uint32_t SYNC_DEBUG_PRINT_MS   = 500;   

// ============================================================
// MAJORITY-VOTE OVERSAMPLING (5 Samples per Bit Period)
// ============================================================
const int NUM_SAMPLES_PER_BIT = 5;
uint32_t sampleOffsetsUs[NUM_SAMPLES_PER_BIT];

void computeSampleOffsets()
{
  uint32_t windowStart = BIT_TIME_US * 20 / 100; // 200 us
  uint32_t windowEnd   = BIT_TIME_US * 80 / 100; // 800 us
  uint32_t windowSpan  = windowEnd - windowStart;

  for (int j = 0; j < NUM_SAMPLES_PER_BIT; j++)
  {
    sampleOffsetsUs[j] = windowStart + (windowSpan * j) / (NUM_SAMPLES_PER_BIT - 1);
  }
}

const int ADC_MAX = 4095;

int readADC()
{
  return analogRead(ADC_PIN);
}

int measureAmbientPeriod(uint32_t durationMs, double* meanOut)
{
  uint32_t start = millis();
  uint32_t samples = 0;
  double sum = 0;

  while ((uint32_t)(millis() - start) < durationMs)
  {
    int value = readADC();
    sum += value;
    samples++;
    delayMicroseconds(100);
  }

  *meanOut = (samples > 0) ? (sum / samples) : 0;
  return 0;
}

uint32_t waitForSyncStart(int syncThreshold)
{
  uint32_t waitStart = millis();
  uint32_t lastDebugPrint = 0;

  while (true)
  {
    if ((uint32_t)(millis() - waitStart) > SYNC_WAIT_TIMEOUT_MS)
    {
      Serial.println("  [SYNC TIMEOUT] Re-measuring ambient...");
      return 0;
    }

    int value = readADC();

    if ((uint32_t)(lastDebugPrint == 0 || millis() - lastDebugPrint) >= SYNC_DEBUG_PRINT_MS)
    {
      Serial.print("  Waiting for SYNC... ADC=");
      Serial.print(value);
      Serial.print(" | Threshold=");
      Serial.println(syncThreshold);
      lastDebugPrint = millis();
    }

    if (value >= syncThreshold)
    {
      uint32_t confirmStart = millis();
      bool sustained = true;

      while ((uint32_t)(millis() - confirmStart) < SYNC_CONFIRM_MS)
      {
        if (readADC() < syncThreshold)
        {
          sustained = false;
          break;
        }
      }

      if (sustained)
      {
        Serial.println("  [SYNC Lock] Laser HIGH detected. Waiting for falling edge...");
        
        uint32_t highTimeout = millis();
        while (readADC() >= syncThreshold)
        {
          if (millis() - highTimeout > 500) return 0;
        }

        return micros(); // Exact falling edge of SYNC pulse
      }
    }
  }
}

// Receive NRZ bit using majority vote sampling
uint8_t receiveBitMajorityVote(uint32_t bitStartTime, int syncThreshold, double* meanValueOut)
{
  int onesCount = 0;
  double sum = 0;

  for (int j = 0; j < NUM_SAMPLES_PER_BIT; j++)
  {
    uint32_t sampleTime = bitStartTime + sampleOffsetsUs[j];
    while ((int32_t)(micros() - sampleTime) < 0) {}

    int value = readADC();
    sum += value;
    if (value >= syncThreshold) onesCount++;
  }

  *meanValueOut = sum / NUM_SAMPLES_PER_BIT;
  return (onesCount > NUM_SAMPLES_PER_BIT / 2) ? 1 : 0;
}

void setup()
{
  Serial.begin(115200);
  pinMode(ADC_PIN, INPUT);
  analogReadResolution(12);
  delay(1000);

  computeSampleOffsets();

  Serial.println();
  Serial.println("========================================");
  Serial.println("   OPTICAL LINK - RX (NRZ Mode)");
  Serial.println("========================================");
  Serial.print("Target String Length : "); Serial.print(MSG_CHAR_LEN); Serial.println(" characters");
  Serial.print("Total Bitstream      : "); Serial.print(TOTAL_RUN_BITS); Serial.println(" bits");
  Serial.print("Sync Threshold       : "); Serial.println(HARDCODED_SYNC_THRESHOLD);
  Serial.println("========================================");
  Serial.println();
}

void loop()
{
  Serial.println("========================================");
  Serial.println("             STARTING RECEIVE");
  Serial.println("========================================");

  uint32_t syncFallingEdge = 0;
  double ambientMean = 0;

  while (syncFallingEdge == 0)
  {
    Serial.println("Sampling Ambient Background (3.5s)...");
    measureAmbientPeriod(3500, &ambientMean);

    Serial.print("Ambient Baseline ADC: ");
    Serial.println(ambientMean, 2);

    Serial.println("Waiting for SYNC Pulse...");
    syncFallingEdge = waitForSyncStart(HARDCODED_SYNC_THRESHOLD);
  }

  Serial.println("SYNC Lock Confirmed. Decoding NRZ Story Payload...");

  uint32_t payloadStart = syncFallingEdge + GUARD_TIME_US;

  uint32_t totalBitErrors = 0;
  double onSum = 0;
  uint32_t onCount = 0;
  double offSum = 0;
  double offSquareSum = 0;
  uint32_t offCount = 0;

  // Storage buffer for decoded text
  char decodedText[MSG_CHAR_LEN + 1];
  decodedText[MSG_CHAR_LEN] = '\0';

  uint32_t bitCounter = 0;

  for (int c = 0; c < MSG_CHAR_LEN; c++)
  {
    char expectedChar = EXPECTED_TEXT[c];
    uint8_t rxCharByte = 0;

    for (int b = 7; b >= 0; b--)
    {
      uint32_t bitStartTime = payloadStart + (bitCounter * BIT_TIME_US);
      double bitMeanADC = 0;

      uint8_t rxBit = receiveBitMajorityVote(bitStartTime, HARDCODED_SYNC_THRESHOLD, &bitMeanADC);
      uint8_t expectedBit = (expectedChar >> b) & 0x01;

      if (rxBit) rxCharByte |= (1 << b);
      if (rxBit != expectedBit) totalBitErrors++;

      if (expectedBit == 1)
      {
        onSum += bitMeanADC;
        onCount++;
      }
      else
      {
        offSum += bitMeanADC;
        offSquareSum += bitMeanADC * bitMeanADC;
        offCount++;
      }

      bitCounter++;
    }

    // FIX: If obstruction produces a null byte (0x00), map it to '?' 
    // to prevent early C-string truncation during Serial.print
    if (rxCharByte == 0x00)
    {
      decodedText[c] = '?';
    }
    else
    {
      decodedText[c] = (char)rxCharByte;
    }
  }

  Serial.println("\n--- Decoded Story Payload ---");
  Serial.print("Received: \"");
  Serial.print(decodedText);
  Serial.println("\"");

  // ============================================================
  // ML FEATURE CALCULATIONS
  // ============================================================
  double ber = (double)totalBitErrors / TOTAL_RUN_BITS;

  double meanOn  = (onCount > 0)  ? (onSum / onCount)   : 0;
  double meanOff = (offCount > 0) ? (offSum / offCount) : 0;

  double netSignalADC = meanOn - meanOff;
  if (netSignalADC < 0) netSignalADC = 0;

  double signalVoltage = netSignalADC * 3.3 / ADC_MAX;
  double ambientVoltage = ambientMean * 3.3 / ADC_MAX;

  double noiseVariance = 0;
  if (offCount > 1)
  {
    double mean = offSum / offCount;
    noiseVariance = (offSquareSum / offCount) - (mean * mean);
    if (noiseVariance < 0) noiseVariance = 0;
  }

  double noiseStdADC = sqrt(noiseVariance);
  double snrDB = (noiseStdADC > 0 && netSignalADC > 0) ? (20.0 * log10(netSignalADC / noiseStdADC)) : -100.0;

  Serial.println();
  Serial.println("========================================");
  Serial.println("          FINAL RUN RESULTS");
  Serial.println("========================================");
  Serial.print("Total Bits Sent : "); Serial.println(TOTAL_RUN_BITS);
  Serial.print("Total Bit Errors: "); Serial.println(totalBitErrors);
  Serial.println("----------------------------------------");
  Serial.println("ML FEATURES:");
  Serial.print("1. Signal Voltage : "); Serial.print(signalVoltage, 4); Serial.println(" V");
  Serial.print("2. BER            : "); Serial.println(ber, 6);
  Serial.print("3. SNR            : "); Serial.print(snrDB, 2); Serial.println(" dB");
  Serial.print("4. Ambient Light  : "); Serial.print(ambientVoltage, 4); Serial.println(" V");
  Serial.println("----------------------------------------");
  Serial.print("ML Vector: [");
  Serial.print(signalVoltage, 4); Serial.print(", ");
  Serial.print(ber, 6); Serial.print(", ");
  Serial.print(snrDB, 2); Serial.print(", ");
  Serial.print(ambientVoltage, 4); Serial.println("]");
  Serial.println("========================================");
  Serial.println();

  while (true)
  {
    delay(1000);
  }
}