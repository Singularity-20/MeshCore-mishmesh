#include <mishmesh/core/CardKbSource.h>
#include <mishmesh/core/InputMapping.h>

namespace mishmesh {

bool CardKbSource::begin(TwoWire& wire) {
  wire.beginTransmission(CARDKB_I2C_ADDR);
  bool present = (wire.endTransmission() == 0);
  if (present) _wire = &wire;
  return present;
}

uint8_t CardKbSource::readRegister() {
  if (_wire->requestFrom((int)CARDKB_I2C_ADDR, 1) < 1) return 0;
  if (!_wire->available()) return 0;
  return (uint8_t)_wire->read();
}

bool CardKbSource::poll(InputReport& out) {
  // Edge-triggered like ButtonGestureSource/DirectionalSource: fire once when
  // a key first reads nonzero, then typematic-repeat on a timer while held,
  // and return false the rest of the time - the host's pumpInput() drains a
  // source in a `while (poll())` loop, so a poll() that kept returning true
  // for the whole duration a key is held would hang that loop.
  uint8_t raw = readRegister();
  uint32_t now = millis();

  char ch = 0;
  InputEvent ev = (raw != 0) ? mapCardKbByte(raw, ch) : InputEvent::None;

  // Confirmed on hardware: the register reads back 0 again immediately after
  // being read, even mid-hold, so there's no continuous "held" byte to poll -
  // just a one-shot pulse per press. Stretch each directional press into a
  // short synthetic hold (see heldMask()) so a frame-polled reader gets at
  // least one true sample; this is independent of the edge/repeat gating
  // below, which paces *dispatched* events, not live state.
  if (ev == InputEvent::NavUp || ev == InputEvent::NavDown ||
      ev == InputEvent::NavLeft || ev == InputEvent::NavRight) {
    _pulseMask = maskBit(ev);
    _pulseUntil = now + HELD_SUSTAIN_MS;
  }

  if (raw == 0) {
    _pressed = false;
    return false;
  }

  bool freshEdge = !_pressed || raw != _lastRaw;
  if (freshEdge) {
    _pressed = true;
    _lastRaw = raw;
    _nextRepeat = now + REPEAT_DELAY_MS;
  } else if ((int32_t)(now - _nextRepeat) >= 0) {
    _nextRepeat = now + REPEAT_INTERVAL_MS;
    out.repeat = true;
  } else {
    return false;   // held, not yet due for a repeat
  }

  if (ev == InputEvent::None) return false;
  out.event = ev;
  out.ch = ch;
  return true;
}

}  // namespace mishmesh
