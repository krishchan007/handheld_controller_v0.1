#include "IP5306.h"

IP5306::IP5306(TwoWire &wire, uint8_t address) : wire_(&wire), address_(address) {}

bool IP5306::begin() {
  uint8_t value = 0;
  return readRegister(REG_SYS_0, value);
}

uint8_t IP5306::address() const {
  return address_;
}

bool IP5306::readRegister(uint8_t reg, uint8_t &value) const {
  wire_->beginTransmission(address_);
  wire_->write(reg);

  if (wire_->endTransmission(false) != 0) {
    return false;
  }

  if (wire_->requestFrom(static_cast<int>(address_), 1) != 1) {
    return false;
  }

  value = wire_->read();
  return true;
}

bool IP5306::writeRegister(uint8_t reg, uint8_t value) {
  wire_->beginTransmission(address_);
  wire_->write(reg);
  wire_->write(value);
  return wire_->endTransmission(true) == 0;
}

bool IP5306::readBits(uint8_t reg, uint8_t bitIndex, uint8_t bitCount, uint8_t &value) const {
  if (bitCount == 0 || bitCount > 8 || bitIndex > 7 || (bitIndex + bitCount) > 8) {
    return false;
  }

  uint8_t regValue = 0;
  if (!readRegister(reg, regValue)) {
    return false;
  }

  const uint8_t mask = static_cast<uint8_t>((1U << bitCount) - 1U);
  value = static_cast<uint8_t>((regValue >> bitIndex) & mask);
  return true;
}

bool IP5306::updateBits(uint8_t reg, uint8_t bitIndex, uint8_t bitCount, uint8_t value) {
  if (bitCount == 0 || bitCount > 8 || bitIndex > 7 || (bitIndex + bitCount) > 8) {
    return false;
  }

  uint8_t regValue = 0;
  if (!readRegister(reg, regValue)) {
    return false;
  }

  const uint8_t mask = static_cast<uint8_t>(((1U << bitCount) - 1U) << bitIndex);
  regValue = static_cast<uint8_t>((regValue & ~mask) | ((value << bitIndex) & mask));
  return writeRegister(reg, regValue);
}
