#include <Sodaq_DS3231.h>
#include <stdint.h>
#include "Led.cpp"

const uint8_t kPinPWMLedR = 9;
const uint8_t kPinPWMLedG = 10;
const uint8_t kPinLedB = 11;
const uint8_t kPinInterruptBtn = 2;
const uint8_t kPinRecordBtn = 8;
const uint8_t kPinFastForwardBtn = 7;

// Arduino handles integers as 16 bit values, so 12 hours (43200s) would
// overflow and become a negative value, if there wasn't a type cast.
uint32_t shiftTimeSec = (uint32_t)60 * 60 * 12;

DualPwmLed dualLedRG = DualPwmLed(kPinPWMLedG, kPinPWMLedR, PwmBoundaryValues::kHighPwm);
Led ledB = Led(kPinLedB, DigitalBoundaryValues::kHighDig);

uint32_t lastResetTs = 0;

volatile bool resetTsFlag = false;

Accelerator ffAccelerator(1.0);

// bool isRecording = false;
bool wasRecordPressedLastLoop = false;
uint32_t recordStartTs = 0;

void setup() {
  dualLedRG.begin();
  ledB.begin();

  rtc.begin();
  lastResetTs = rtc.now().getEpoch();

  // For debug
  Serial.begin(115200);

  // Buttons
  pinMode(kPinRecordBtn, INPUT);
  pinMode(kPinFastForwardBtn, INPUT);
  attachInterrupt(digitalPinToInterrupt(kPinInterruptBtn), resetTs, RISING);
}

void loop() {
  DateTime now = rtc.now(); // get the current date-time
  uint32_t ts = now.getEpoch();

  // Reset
  if (resetTsFlag) {
    lastResetTs = ts;

    resetTsFlag = false;
  }

  // Fast forward
  if (digitalRead(kPinFastForwardBtn)) {
    ffAccelerator.accelerate();
    lastResetTs = (uint32_t)constrain(
      lastResetTs - (uint32_t)ffAccelerator.getValue(), (uint32_t)0, ts
    );
  }
  else {
    ffAccelerator.reset();
  }

  // Record
  if (digitalRead(kPinRecordBtn)) {
    // Start recording
    if (recordStartTs == 0 && !wasRecordPressedLastLoop) {
      ledB.setValue((uint8_t)DigitalBoundaryValues::kHighDig);
      recordStartTs = ts;
    }

    // Stop recording
    uint32_t newShiftTime = ts - recordStartTs;
    if (recordStartTs != 0 && newShiftTime > 0 && !wasRecordPressedLastLoop) {
      shiftTimeSec = newShiftTime;
      recordStartTs = 0;
      ledB.setValue((uint8_t)DigitalBoundaryValues::kLowDig);
    }

    wasRecordPressedLastLoop = true;
  }
  else {
    wasRecordPressedLastLoop = false;
  }

  // Set main led value
  lastResetTs = (uint32_t)constrain(lastResetTs, (uint32_t)0, ts);
  double ledRGValue = 1.0 - ((double)(ts - lastResetTs) / shiftTimeSec);
  dualLedRG.setBipolarValue(ledRGValue);

  // Debug prints
  Serial.print(";  LedValue: ");
  Serial.print(ledRGValue);
  Serial.print(";  ts: ");
  Serial.print(ts);
  Serial.print(";  lastResetTs: ");
  Serial.print(lastResetTs);
  Serial.print(";  ts - lastResetTs: ");
  Serial.print(ts - lastResetTs);
  Serial.print(";  shiftTimeSec: ");
  Serial.print(shiftTimeSec);
  Serial.print(";  (double)(ts - lastResetTs) / shiftTimeSec: ");
  Serial.print((double)(ts - lastResetTs) / shiftTimeSec);
  Serial.print(";  ffSpeed: ");
  Serial.print(ffAccelerator.getValue());
  Serial.print(";  recordStartTs: ");
  Serial.println(recordStartTs);

  delay(100);
}

void resetTs() {
  resetTsFlag = true;
}
