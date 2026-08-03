#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/applets/KeypadApplet.h>   // KeypadConfirmFn, KP_MAX - shared seam
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/widgets/GridView.h>

namespace mishmesh {

// Open free-text box for keyboard-style input sources (CardKB): the
// CardKB-present counterpart to KeypadApplet's multi-tap grid. Same
// configure()/push() seam as KeypadApplet so callers (e.g.
// MessageThreadApplet::startCompose()) can pick either with a one-line branch
// and share the same backing buffer. Typed input is ASCII-only (CardKB's own
// protocol has no multi-byte codepoints), so ordinary cursor movement/
// insert/delete stay byte-based - but Tab opens an emoji picker (reusing
// KeypadApplet's EmojiCatalog + GridView), and an inserted emoji is UTF-8, so
// the picker path alone needs the codepoint-boundary helpers below.
class TextEntryApplet : public Applet, public GridModel {
public:
  TextEntryApplet();

  // Same seam as KeypadApplet::configure(); call before push(). showCharCount
  // reserves a bottom-right "n/cap" footer - only the real outbound message
  // compose field (MessageThreadApplet::startCompose) sets this; every other
  // free-text field (renames, settings, hex fields...) leaves it off.
  void configure(char* dst, uint16_t cap, const char* title,
                 KeypadConfirmFn onConfirm = nullptr, void* ctx = nullptr,
                 bool showCharCount = false);

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  bool onChar(char ch) override;

  // GridModel - the emoji picker's 3x4 page grid (Tab opens it; see onChar).
  int rows() const override { return 3; }
  int cols() const override { return 4; }
  const char* cellLabel(int r, int c) const override;

  const char* text() const { return _buf; }
  uint16_t length() const { return _len; }
  uint16_t cursor() const { return _cursor; }

private:
  bool isDirty() const;
  void insertCharAt(uint16_t pos, char ch);
  void deleteCharAt(uint16_t pos);
  void confirmAndExit();

  AppletContext* _ctx;
  char  _own[KeypadApplet::KP_MAX + 1];
  char* _buf;            // always points at _own
  char* _src;            // configure() destination; written only on OK. null = standalone
  uint16_t _cap;
  const char* _title;
  KeypadConfirmFn _onConfirm;
  void* _onConfirmCtx;
  uint16_t _len;
  uint16_t _cursor;      // insertion index, 0.._len
  bool _showCharCount;

  ConfirmDialog _confirm;   // discard-changes guard, same pattern as KeypadApplet
  bool _confirming;

  // Emoji picker: Tab (onChar) opens it when the catalog is non-empty; modal
  // like _confirming above, drawn via the shared drawModalChrome() box rather
  // than KeypadApplet's own grid (this applet has no grid otherwise).
  void insertString(uint16_t pos, const char* s);      // multi-byte insert at pos
  uint16_t prevCodepoint(uint16_t pos) const;          // codepoint boundary before pos
  uint16_t nextCodepoint(uint16_t pos) const;          // codepoint boundary at/after pos
  void fillEmojiCells();
  int  emojiPageCount() const;      // ceil(count/12); 0 when no catalog
  void nextEmojiPage();             // wraps
  void prevEmojiPage();             // wraps
  void insertSelectedEmoji();

  bool _pickingEmoji = false;
  uint8_t _emojiPageIdx = 0;
  char _emojiCells[12][5];          // UTF-8 of the current page's cells ("" = empty)
  GridView _emojiGrid;
};

TextEntryApplet& textEntryApplet();

}  // namespace mishmesh
