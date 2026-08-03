#include <mishmesh/applets/TextEntryApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>

namespace mishmesh {

TextEntryApplet::TextEntryApplet()
    : Applet("Text"), _ctx(nullptr), _buf(_own), _src(nullptr), _cap(KeypadApplet::KP_MAX),
      _title("Text"), _onConfirm(nullptr), _onConfirmCtx(nullptr), _len(0), _cursor(0),
      _confirming(false) {
  _own[0] = 0;
}

bool TextEntryApplet::isDirty() const {
  const char* orig = _src ? _src : "";   // standalone: original was empty
  return strcmp(_buf, orig) != 0;
}

void TextEntryApplet::insertCharAt(uint16_t pos, char ch) {
  if (_len >= _cap) return;
  for (uint16_t i = _len; i > pos; i--) _buf[i] = _buf[i - 1];
  _buf[pos] = ch;
  _len++;
  _buf[_len] = 0;
}

void TextEntryApplet::deleteCharAt(uint16_t pos) {
  if (pos >= _len) return;
  for (uint16_t i = pos; i + 1 < _len; i++) _buf[i] = _buf[i + 1];
  _len--;
  _buf[_len] = 0;
}

void TextEntryApplet::configure(char* dst, uint16_t cap, const char* title,
                                KeypadConfirmFn onConfirm, void* ctx) {
  _src = dst;                                  // written only on OK; never during editing
  _cap = cap < KeypadApplet::KP_MAX ? cap : KeypadApplet::KP_MAX;
  _title = title;
  _onConfirm = onConfirm; _onConfirmCtx = ctx;
  _buf = _own;                                 // seed the working copy from the source
  uint16_t n = 0;
  if (dst) while (dst[n] && n < _cap) { _own[n] = dst[n]; n++; }
  _own[n] = 0;
  _len = n; _cursor = n;
}

void TextEntryApplet::onStart(AppletContext& ctx) {
  _ctx = &ctx;
  if (!_src) { _own[0] = 0; _len = 0; _cursor = 0; }  // standalone: fresh buffer (configure() already seeded)
  _buf = _own;
  _confirming = false;
  _confirm.reset();
}

void TextEntryApplet::confirmAndExit() {
  if (_src && _src != _buf) {            // commit the working copy to the source on OK
    memcpy(_src, _buf, _len);
    _src[_len] = 0;
  }
  const char* result = _src ? _src : _buf;
  if (_onConfirm) _onConfirm(_onConfirmCtx, result);
  if (_ctx && _ctx->host) {
    if (!_onConfirm) _ctx->host->postToast(_len ? "Saved" : "(empty)");
    _ctx->host->pop();
  }
}

int TextEntryApplet::onRender(Canvas& c) {
  const Font* f = fontBody();
  int w = c.width(), h = c.height();
  const int padX = 2, padY = 1;

  if (_len == 0 && _title && _title[0]) {
    // Empty buffer: show the configured title as a recessive placeholder, same
    // idiom as KeypadApplet::drawBuffer - it clears on the first keypress.
    const Font* hf = fontCaption();
    int fh = c.fontHeight(f);
    c.fillRect(0, 0, 1, fh, DisplayDriver::LIGHT);
    c.drawText(hf, 4, padY, _title, DisplayDriver::LIGHT);
    return 500;
  }

  // Cursor is a real caret bar drawn at its own pixel position (rather than a
  // marker character woven into the text), located via measureWrappedCursor -
  // which walks mcufont's own word-wrap pass, so line breaks still match
  // drawTextWrapped exactly with no need to duplicate its line-breaking here.
  int textW = w - 2 * padX;
  int cx, cy;
  c.measureWrappedCursor(f, textW, _buf, _cursor, cx, cy);

  // Scroll so the cursor's line stays visible - same "draw at its natural
  // position minus a scroll offset, let the canvas clip do the rest" idiom
  // MessageThreadApplet uses for its message list.
  int cursorBottomY = cy + c.lineHeight(f);
  int scrollY = cursorBottomY > (h - padY) ? cursorBottomY - (h - padY) : 0;

  c.drawTextWrapped(f, padX, padY - scrollY, textW, _buf, DisplayDriver::LIGHT);

  if ((c.now() / 500) % 2) {   // blink-on half of the cycle
    c.fillRect(padX + cx, padY + cy - scrollY, 1, c.fontHeight(f), DisplayDriver::LIGHT);
  }
  return 500;   // blink cadence
}

bool TextEntryApplet::onInput(InputEvent ev) {
  if (_confirming) {                       // discard dialog is modal: route input to it
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r != ConfirmResult::None) {
        bool discard = (r == ConfirmResult::Confirmed);
        _confirming = false; _confirm.reset();
        if (discard && _ctx && _ctx->host) _ctx->host->pop();   // drop edits, exit
      }
    }
    return true;                           // swallow everything while the dialog is up
  }
  switch (ev) {
    case InputEvent::NavLeft:
      if (_cursor > 0) _cursor--;
      return true;
    case InputEvent::NavRight:
      if (_cursor < _len) _cursor++;
      return true;
    case InputEvent::Select:
      confirmAndExit();
      return true;
    case InputEvent::Back:                  // Back = exit, like every other screen
    case InputEvent::BackLong:
      if (isDirty()) { _confirm.configure("Discard changes?"); _confirming = true; return true; }
      return false;                         // unchanged -> host pops (exit)
    default:
      return false;
  }
}

bool TextEntryApplet::onChar(char ch) {
  if (ch == 8) {                            // Backspace: remove before cursor
    if (_cursor > 0) { deleteCharAt(_cursor - 1); _cursor--; }
    return true;
  }
  if (ch >= 0x20 && ch < 0x7F) {             // printable ASCII
    insertCharAt(_cursor, ch);
    _cursor++;
    return true;
  }
  return false;
}

TextEntryApplet& textEntryApplet() {
  static TextEntryApplet instance;
  return instance;
}

}  // namespace mishmesh
