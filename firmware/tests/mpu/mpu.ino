// mpu.ino — MPU-6050 streaming bring-up test.
//
// Brings up the IMU on PIN_MPU_SDA / PIN_MPU_SCL (from src/config.h) and
// streams one sample per line over USB serial in the form
//   MPU,<ax>,<ay>,<az>,<gx>,<gy>,<gz>
// at ~50 Hz, where accel is in g and gyro is in °/s. Pair with the
// terminal visualizer (`make test-mpu-view`) for a live readout.
//
// Unlike the main firmware — which bit-bangs the MPU bus because the
// hardware Wire is owned by the OLED — this test has nothing else on the
// bus, so we just use Wire directly. Same pins, same MPU init sequence,
// same ±4 g / ±500 °/s ranges as src/imu/mpu6050.cpp.
//
// Build:  arduino-cli compile --profile c3
// Flash:  arduino-cli upload  --profile c3 --port /dev/cu.usbmodem<…>

#include <Arduino.h>
#include <Wire.h>

#include "../../src/config.h"

namespace {

// Register map — only what we touch.
constexpr uint8_t REG_SMPLRT_DIV = 0x19;
constexpr uint8_t REG_CONFIG = 0x1A;
constexpr uint8_t REG_GYRO_CONFIG = 0x1B;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;  // first byte of the 14-byte burst
constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;

constexpr float ACCEL_LSB_PER_G = 8192.0f;   // matches ACCEL_CONFIG = 0x08 (±4 g)
constexpr float GYRO_LSB_PER_DPS = 65.5f;    // matches GYRO_CONFIG  = 0x08 (±500 °/s)

constexpr uint32_t SAMPLE_INTERVAL_MS = 20;  // 50 Hz — plenty for terminal viz

bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readBurst(uint8_t reg, uint8_t* buf, size_t n) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  // Repeated start (endTransmission(false) keeps the bus) so the slave's
  // internal register pointer stays parked on REG_ACCEL_XOUT_H for the
  // burst read that follows.
  if (Wire.endTransmission(false) != 0) return false;
  size_t got = Wire.requestFrom((int)MPU_ADDR, (int)n);
  if (got != n) return false;
  for (size_t i = 0; i < n; ++i) buf[i] = Wire.read();
  return true;
}

bool mpuInit() {
  if (!writeReg(REG_PWR_MGMT_1, 0x80)) return false;  // soft reset
  delay(100);
  if (!writeReg(REG_PWR_MGMT_1, 0x01)) return false;   // wake, PLL = gyro-X
  if (!writeReg(REG_CONFIG, 0x03)) return false;       // 44 Hz DLPF
  if (!writeReg(REG_SMPLRT_DIV, 0x04)) return false;   // 200 Hz internal rate
  if (!writeReg(REG_GYRO_CONFIG, 0x08)) return false;  // ±500 °/s
  if (!writeReg(REG_ACCEL_CONFIG, 0x08)) return false; // ±4 g
  return true;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  // Let USB CDC enumerate before the host opens the port, so the first
  // line ("streaming at …") doesn't get swallowed.
  delay(500);

  Wire.begin(PIN_MPU_SDA, PIN_MPU_SCL);
  Wire.setClock(400000);  // 400 kHz fast mode — alone on this bus

  Serial.println("mpu: probing MPU-6050 at 0x68");
  Wire.beginTransmission(MPU_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("mpu: no device — check SDA=GPIO7 / SCL=GPIO8 / VCC / GND");
    while (true) delay(1000);
  }
  if (!mpuInit()) {
    Serial.println("mpu: init failed (I2C write rejected)");
    while (true) delay(1000);
  }
  Serial.println("mpu: streaming at 50 Hz — fields: MPU,ax,ay,az,gx,gy,gz");
}

void loop() {
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  if (now - lastMs < SAMPLE_INTERVAL_MS) return;
  lastMs = now;

  uint8_t buf[14];
  if (!readBurst(REG_ACCEL_XOUT_H, buf, sizeof(buf))) {
    Serial.println("mpu: read error");
    return;
  }

  // Big-endian 16-bit signed words. buf[6..7] is temperature — skipped.
  int16_t ax = (int16_t)((buf[0] << 8) | buf[1]);
  int16_t ay = (int16_t)((buf[2] << 8) | buf[3]);
  int16_t az = (int16_t)((buf[4] << 8) | buf[5]);
  int16_t gx = (int16_t)((buf[8] << 8) | buf[9]);
  int16_t gy = (int16_t)((buf[10] << 8) | buf[11]);
  int16_t gz = (int16_t)((buf[12] << 8) | buf[13]);

  char line[96];
  snprintf(line, sizeof(line),
           "MPU,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f",
           ax / ACCEL_LSB_PER_G, ay / ACCEL_LSB_PER_G, az / ACCEL_LSB_PER_G,
           gx / GYRO_LSB_PER_DPS, gy / GYRO_LSB_PER_DPS, gz / GYRO_LSB_PER_DPS);
  Serial.println(line);
}
