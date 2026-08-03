#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/applets/KeypadApplet.h>   // KeypadConfirmFn, KP_MAX - shared seam
#include <mishmesh/widgets/ConfirmDialog.h>

namespace mishmesh {

// Open free-text box for keyboard-style input sources (CardKB): the
// CardKB-present counterpart to KeypadApplet's multi-tap grid. Same
// configure()/push() seam as KeypadApplet so callers (e.g.
// MessageThreadApplet::startCompose()) can pick either with a one-line branch
// and share the same backing buffer. ASCII-only (CardKB's own protocol has no
// multi-byte codepoints), so - unlike KeypadApplet - no UTF-8 boundary logic
// is needed for cursor movement or insert/delete.
class TextEntryApplet : public Applet {
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
};

TextEntryApplet& textEntryApplet();

}  // namespace mishmesh
