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
  // The module's register clears back to 0 the instant it's read, even while
  // the key is still physically held - confirmed on hardware: no continuous
  // "held" byte is ever observable, only a one-shot pulse per press. Frame-
  // polled consumers (the Arduboy bridge behind 2048, via heldMask()) need a
  // sustained true sample to register a move, so a single edge is stretched
  // into a short synthetic hold instead of reporting the raw (unsustainable)
  // register state. See heldMask().
  static const uint32_t HELD_SUSTAIN_MS = 150;

  TwoWire* _wire;
  uint8_t  _lastRaw;      // last nonzero byte read, for repeat-while-held
  bool     _pressed;      // a key is currently down (edge-tracking)
  uint32_t _nextRepeat;
  uint16_t _pulseMask;    // direction bit from the most recent press, if any
  uint32_t _pulseUntil;   // millis() deadline for the synthetic hold above

  uint8_t readRegister();   // 0 if idle or on read failure

public:
  CardKbSource()
      : _wire(nullptr), _lastRaw(0), _pressed(false), _nextRepeat(0),
        _pulseMask(0), _pulseUntil(0) {}

  // Probes the bus for the module; stores `wire` and returns true only if it
  // acked. Caller should not addSource() this instance when this returns false.
  bool begin(TwoWire& wire);

  bool poll(InputReport& out) override;
  // setHoldRepeat() left as the InputSource default no-op: a keyboard's own
  // typematic repeat is a hardware-level expectation, not a per-applet
  // preference like the Back button's.

  // Live directional held-state for real-time applets (e.g. the Arduboy
  // bridge used by 2048, via InputState::isDown()) - synthesized from the
  // last press's sustain window (see HELD_SUSTAIN_MS), not a real-time
  // register read, since the hardware can't sustain one. Recomputed each
  // call from _pulseMask/_pulseUntil, refreshed by poll().
  uint16_t heldMask() const override {
    return ((int32_t)(millis() - _pulseUntil) < 0) ? _pulseMask : 0;
  }
};

}  // namespace mishmesh
