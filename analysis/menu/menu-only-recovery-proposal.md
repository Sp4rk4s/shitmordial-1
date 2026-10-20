# Proposed controlled menu-only test — not implemented or executed

The existing guide forbids live subsystem-root calls and guessedGlobalData.
An explicit scope choice is pending. The original startup investigation remains
the permitted route until the user changes that boundary.

The alternative is a separate test of the original menu's window tree and native
drawing, preserving Steam staging and the current patcher architecture. It would
not claim gameplay initialization or working gameplay features.

Reviewable evidence already prepared:

- Guarded ownership and empty-selector fixes compile under
  build/menu-render-guard/ and pass all19 tests. No live deployment occurred.
- Constructor2E04D0 completes in the controlled model without input arguments,
  creating four windows. It uses only the two reconstructed CPU-guard fields in
  the modeled loader context. Complete GlobalData remains unknown.
- Primary window draw351AB0, ECX447F33B0, completes with balanced stack and62 text
  intents. This test excludes frame-manager/input/periodic feature callbacks.
- Two original icon TTFs have verified SFNT checksums and recovered family names.
- Renderer refresh360080 now completes offline, including both embedded-font
  registration requests and15 cached font-setting requests. All six main-menu
  font records have exact associated face/size/weight/flags/ranges in
  font-refresh-settings.json. Font handles and glyph measurements remain models.
- Current Source factory versions, panel method41, Surface rendering methods,
  engine signon predicate and timing-pointer assignment have retained evidence.

The first live test would need guarded binding of current Source objects, real
font registration and measurement, a scoped reconstructed CPU-guard context,
and execution of the recovered constructor on a valid UI thread. It would then
draw only the original primary window in a verified paint context. Start with
read-only menu preview and Insert visibility control before enabling native
input/callbacks. Original body-font settings are recovered for the six menu
records. Complete protected/runtime contracts and compatibility of the private
FontManager language field remain open; current FontManager+28 is an inline
string, whereas the old constructor temporarily writes a pointer there.

Every code/input profile must fail closed if it differs from this exact dump/game
build. Keep the original binary untouched. Keep the static TLS companion and all
339 imports/module bindings. Use a fresh windowed/insecure process, distinguish
entry/preview status from actual menu confirmation, stop after an unsupported
dependency or first failure, and preserve the user's IDA/debugger sessions.

This is a proposed investigation step requiring the user's changed scope, not a
menu fix, an implemented helper, or permission already granted.
