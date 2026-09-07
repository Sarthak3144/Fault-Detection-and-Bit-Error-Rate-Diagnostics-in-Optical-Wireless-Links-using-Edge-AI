/*
 * ============================================================
 * OPTICAL LINK - TRANSMITTER (NRZ Encoded Story Stream)
 * ============================================================
 * Hardware: LASER_PIN -> GPIO23
 * Bit Rate: 1000 bps (1000 us per bit)
 * Scheme  : Non-Return-to-Zero (NRZ-L)
 *           - Bit '1': HIGH for full 1000 us
 *           - Bit '0': LOW for full 1000 us
 * ============================================================
 */

#define LASER_PIN 23

// ============================================================
// TIMING PARAMETERS
// ============================================================
const uint32_t OFF_TIME_MS   = 3500;    // 3.5s OFF window for RX ambient sampling
const uint32_t SYNC_TIME_US  = 100000;  // 100 ms SYNC pulse
const uint32_t GUARD_TIME_US = 20000;   // 20 ms GUARD interval
const uint32_t BIT_TIME_US   = 1000;    // 1000 us/bit (1 kbps)

// Target Story Payload
const char PAYLOAD_TEXT[] = 
  "A father went to say good night to his seven year old son, very well knowing that if he didn't his son would have trouble sleeping. "
  "It was a nightly routine between them. He entered the dimly lit room where his son waited under his blanket. "
  "With the first glance the father could tell there was something unusual about his son tonight, but couldn't put his finger on it. "
  "He looked the same but had a grin that drew from ear to ear. "
  "\"You okay, buddy?\" the father asked. "
  "The son nodded, still with the grin, before saying, \"Daddy, check for monsters under my bed.\" "
  "The father chuckled a bit before getting on his knees to check only to satisfy his son. "
  "There, under the bed, pale and afraid, was his son. His real son. He whispered, \"Daddy, there someone on my bed\".";

// Pre-calculated parameters
const int MSG_CHAR_LEN = sizeof(PAYLOAD_TEXT) - 1; 
const int MSG_BIT_LEN  = MSG_CHAR_LEN * 8;         

// Microsecond precise timing helper
void waitUntil(uint32_t targetTime)
{
  while ((int32_t)(micros() - targetTime) < 0)
  {
    // Busy wait loop
  }
}

void setup()
{
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW);

  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("========================================");
  Serial.println("   OPTICAL LINK - TX (NRZ Mode)");
  Serial.println("========================================");
  Serial.print("Text Length    : "); Serial.print(MSG_CHAR_LEN); Serial.println(" bytes");
  Serial.print("Total Bits     : "); Serial.print(MSG_BIT_LEN); Serial.println(" bits");
  Serial.print("Transmission T : ~"); Serial.print((float)MSG_BIT_LEN / 1000.0, 2); Serial.println(" seconds");
  Serial.println("========================================");
  Serial.println();
}

void loop()
{
  Serial.println("========================================");
  Serial.println("         STARTING TRANSMISSION");
  Serial.println("========================================");

  // 1. OFF Phase (Ambient sampling window for RX)
  digitalWrite(LASER_PIN, LOW);
  Serial.print("OFF phase (Ambient Sampling): ");
  Serial.print(OFF_TIME_MS);
  Serial.println(" ms");
  delay(OFF_TIME_MS);

  // 2. SYNC Pulse (100 ms HIGH)
  Serial.println("Sending SYNC (100 ms HIGH)...");
  digitalWrite(LASER_PIN, HIGH);
  uint32_t syncEnd = micros() + SYNC_TIME_US;
  waitUntil(syncEnd);

  // 3. GUARD Interval (20 ms LOW)
  digitalWrite(LASER_PIN, LOW);
  Serial.println("Sending GUARD (20 ms LOW)...");
  uint32_t guardEnd = micros() + GUARD_TIME_US;
  waitUntil(guardEnd);

  // 4. Send NRZ Encoded Story Stream
  Serial.println("Transmitting NRZ Payload...");
  uint32_t payloadStart = guardEnd; 

  uint32_t bitCounter = 0;
  for (int c = 0; c < MSG_CHAR_LEN; c++)
  {
    char ch = PAYLOAD_TEXT[c];
    for (int b = 7; b >= 0; b--)
    {
      uint8_t bit = (ch >> b) & 0x01;
      
      // NRZ Output Logic
      digitalWrite(LASER_PIN, bit ? HIGH : LOW);

      bitCounter++;
      uint32_t nextBitTime = payloadStart + (bitCounter * BIT_TIME_US);
      waitUntil(nextBitTime);
    }
  }

  // Turn laser off and stop
  digitalWrite(LASER_PIN, LOW);
  Serial.println();
  Serial.println("========================================");
  Serial.println("     TRANSMISSION COMPLETE");
  Serial.println("========================================");
  Serial.println("TX complete.");

  while (true)
  {
    delay(1000);
  }
}