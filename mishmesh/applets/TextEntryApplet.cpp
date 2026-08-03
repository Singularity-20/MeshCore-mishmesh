#include <mishmesh/applets/TextEntryApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/EmojiCatalog.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/widgets/Modal.h>
#include <string.h>
#include <stdio.h>

namespace mishmesh {

TextEntryApplet::TextEntryApplet()
    : Applet("Text"), _ctx(nullptr), _buf(_own), _src(nullptr), _cap(KeypadApplet::KP_MAX),
      _title("Text"), _onConfirm(nullptr), _onConfirmCtx(nullptr), _len(0), _cursor(0),
      _showCharCount(false), _confirming(false) {
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

void TextEntryApplet::insertString(uint16_t pos, const char* s) {
  uint16_t sl = (uint16_t)strlen(s);
  if (sl == 0 || (uint32_t)_len + sl > _cap) return;
  memmove(_buf + pos + sl, _buf + pos, (size_t)(_len - pos + 1));  // move NUL too
  memcpy(_buf + pos, s, sl);
  _len = (uint16_t)(_len + sl);
}

// Codepoint-boundary helpers - only needed because the emoji picker can
// insert multi-byte UTF-8 (typed input itself stays ASCII-only, see the
// class comment); on plain ASCII these are equivalent to a plain +-1.
uint16_t TextEntryApplet::prevCodepoint(uint16_t pos) const {
  if (pos == 0) return 0;
  uint16_t i = (uint16_t)(pos - 1);
  while (i > 0 && ((unsigned char)_buf[i] & 0xC0) == 0x80) i--;    // skip continuation bytes
  return i;
}

uint16_t TextEntryApplet::nextCodepoint(uint16_t pos) const {
  if (pos >= _len) return _len;
  unsigned char b = (unsigned char)_buf[pos];
  uint16_t adv = (b < 0x80) ? 1 : ((b >> 5) == 0x6) ? 2
               : ((b >> 4) == 0xE) ? 3 : ((b >> 3) == 0x1E) ? 4 : 1;
  uint16_t n = (uint16_t)(pos + adv);
  return n > _len ? _len : n;
}

const char* TextEntryApplet::cellLabel(int r, int c) const {
  int i = r * 4 + c;
  if (i < 0 || i >= 12) return "";
  return _emojiCells[i];
}

void TextEntryApplet::fillEmojiCells() {
  int cnt = emojiCatalogCount();
  for (int i = 0; i < 12; i++) {
    int idx = _emojiPageIdx * 12 + i;
    if (idx < cnt) KeypadApplet::utf8Encode(emojiCatalogAt((uint16_t)idx), _emojiCells[i]);
    else _emojiCells[i][0] = 0;
  }
}

int TextEntryApplet::emojiPageCount() const {
  int cnt = emojiCatalogCount();
  return cnt > 0 ? (cnt + 11) / 12 : 0;
}

void TextEntryApplet::nextEmojiPage() {
  int pc = emojiPageCount(); if (pc <= 1) return;
  _emojiPageIdx = (uint8_t)((_emojiPageIdx + 1) % pc); fillEmojiCells();
}

void TextEntryApplet::prevEmojiPage() {
  int pc = emojiPageCount(); if (pc <= 1) return;
  _emojiPageIdx = (uint8_t)((_emojiPageIdx + pc - 1) % pc); fillEmojiCells();
}

void TextEntryApplet::insertSelectedEmoji() {
  const char* s = cellLabel(_emojiGrid.focusedRow(), _emojiGrid.focusedCol());
  if (!s || !s[0]) return;
  insertString(_cursor, s);
  _cursor = (uint16_t)(_cursor + strlen(s));
}

void TextEntryApplet::configure(char* dst, uint16_t cap, const char* title,
                                KeypadConfirmFn onConfirm, void* ctx,
                                bool showCharCount) {
  _src = dst;                                  // written only on OK; never during editing
  _cap = cap < KeypadApplet::KP_MAX ? cap : KeypadApplet::KP_MAX;
  _title = title;
  _onConfirm = onConfirm; _onConfirmCtx = ctx;
  _showCharCount = showCharCount;
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
  _pickingEmoji = false;
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
  if (_confirming) {                       // discard dialog overlays the text screen
    _confirm.draw(c, 0, 0, c.width(), c.height());
    return 100;
  }
  if (_pickingEmoji) {                     // emoji picker overlays the text screen
    Canvas box = drawModalChrome(c);
    char tag[8];
    snprintf(tag, sizeof(tag), "%d/%d", _emojiPageIdx + 1, emojiPageCount());
    box.drawText(fontCaption(), box.width() - 1, 1, tag, DisplayDriver::LIGHT, TextAlign::Right);
    const int tagH = 9;
    _emojiGrid.draw(box, 0, tagH, box.width(), box.height() - tagH);
    return 100;
  }

  const Font* f = fontBody();
  const Font* cf = fontCaption();
  int w = c.width(), h = c.height();
  const int padX = 2, padY = 1;
  // Reserve a bottom strip for the "n/cap" counter and/or the emoji hint so
  // wrapped text/caret never run underneath them - only when at least one is
  // actually shown for this field.
  bool showEmojiHint = emojiCatalogCount() > 0;
  int footerH = (_showCharCount || showEmojiHint) ? c.lineHeight(cf) : 0;

  if (_len == 0 && _title && _title[0]) {
    // Empty buffer: show the configured title as a recessive placeholder, same
    // idiom as KeypadApplet::drawBuffer - it clears on the first keypress.
    // (A persistent always-on header was tried and reverted: once the field
    // scrolled, wrapped text ran right through the header row - Canvas has no
    // clip between them - and the always-on look wasn't well liked anyway.)
    int fh = c.fontHeight(f);
    c.fillRect(0, 0, 1, fh, DisplayDriver::LIGHT);
    c.drawText(cf, 4, padY, _title, DisplayDriver::LIGHT);
  } else {
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
    int bottomLimit = h - padY - footerH;
    int cursorBottomY = cy + c.lineHeight(f);
    int scrollY = cursorBottomY > bottomLimit ? cursorBottomY - bottomLimit : 0;

    c.drawTextWrapped(f, padX, padY - scrollY, textW, _buf, DisplayDriver::LIGHT);

    if ((c.now() / 500) % 2) {   // blink-on half of the cycle
      c.fillRect(padX + cx, padY + cy - scrollY, 1, c.fontHeight(f), DisplayDriver::LIGHT);
    }
  }

  if (_showCharCount) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%u/%u", (unsigned)_len, (unsigned)_cap);
    c.drawText(cf, w - padX, h - padY - footerH, buf, DisplayDriver::LIGHT, TextAlign::Right);
  }
  if (showEmojiHint) {
    c.drawText(cf, padX, h - padY - footerH, "tab: emoji", DisplayDriver::LIGHT);
  }

  return 500;   // blink cadence
}

bool TextEntryApplet::onInput(InputEvent ev) {
  if (_pickingEmoji) {                     // emoji picker is modal: route nav/select to it
    switch (ev) {
      case InputEvent::NavUp:
      case InputEvent::NavDown:
        _emojiGrid.onInput(ev);
        return true;
      case InputEvent::NavLeft:
      case InputEvent::NavRight: {
        // Edge-of-page nav pages instead of wrapping in place, same wiring as
        // KeypadApplet's emoji grid.
        int nc = _emojiGrid.focusedCol();
        if (ev == InputEvent::NavRight && nc == cols() - 1) { nextEmojiPage(); return true; }
        if (ev == InputEvent::NavLeft  && nc == 0)          { prevEmojiPage(); return true; }
        _emojiGrid.onInput(ev);
        return true;
      }
      case InputEvent::Select:
        insertSelectedEmoji();
        _pickingEmoji = false;
        return true;
      case InputEvent::Back:
      case InputEvent::BackLong:
        _pickingEmoji = false;             // cancel, no insert
        return true;
      default:
        return true;                       // swallow everything else while picking
    }
  }
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
      _cursor = prevCodepoint(_cursor);
      return true;
    case InputEvent::NavRight:
      _cursor = nextCodepoint(_cursor);
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
  if (_confirming || _pickingEmoji) return true;   // modal state: swallow raw typing
  if (ch == 9) {                            // Tab: open emoji picker (no-op if catalog is empty)
    if (emojiCatalogCount() > 0) {
      _pickingEmoji = true;
      _emojiPageIdx = 0;
      fillEmojiCells();
      _emojiGrid.setModel(this);
    }
    return true;
  }
  if (ch == 8) {                            // Backspace: remove the codepoint before cursor
    if (_cursor > 0) {
      uint16_t p = prevCodepoint(_cursor);
      while (_cursor > p) { deleteCharAt(_cursor - 1); _cursor--; }
    }
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
