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

  char ch = 0;
  InputEvent ev = mapCardKbByte(raw, ch);
  if (ev == InputEvent::None) return false;
  out.event = ev;
  out.ch = ch;
  return true;
}

}  // namespace mishmesh
