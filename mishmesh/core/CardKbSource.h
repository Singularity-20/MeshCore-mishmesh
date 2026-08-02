#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <mishmesh/core/InputSource.h>

namespace mishmesh {

// DFRobot-protocol I2C keyboard (CardKB). Runtime-detected: begin() probes the
// bus and returns whether a device answered at CARDKB_I2C_ADDR, so the host
// only registers this source (and only advertises the capability) when the
// module is physically attached. Same firmware binary works either way.
class CardKbSource : public InputSource {
  static const uint8_t CARDKB_I2C_ADDR = 0x5F;
  static const uint32_t REPEAT_DELAY_MS = 350;     // hold this long before repeats start
  static const uint32_t REPEAT_INTERVAL_MS = 90;   // then one repeat per this many ms

  TwoWire* _wire;
  uint8_t  _lastRaw;      // last nonzero byte read, for repeat-while-held
  bool     _pressed;      // a key is currently down (edge-tracking)
  uint32_t _nextRepeat;

  uint8_t readRegister();   // 0 if idle or on read failure

public:
  CardKbSource() : _wire(nullptr), _lastRaw(0), _pressed(false), _nextRepeat(0) {}

  // Probes the bus for the module; stores `wire` and returns true only if it
  // acked. Caller should not addSource() this instance when this returns false.
  bool begin(TwoWire& wire);

  bool poll(InputReport& out) override;
  // setHoldRepeat() left as the InputSource default no-op: a keyboard's own
  // typematic repeat is a hardware-level expectation, not a per-applet
  // preference like the Back button's.
};

}  // namespace mishmesh
