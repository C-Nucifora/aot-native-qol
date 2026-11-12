# Existing portable checks included

Six original, existing synthetic host checks are included, with their source
bodies unchanged and without game fixtures or absolute paths:

- repeat-check.c: press/release/repeat timing at multiple rates and stock30 bypass.
- camera-check.c: wall-time angular equivalence and unknown/relative input bypass.
- ui-clock-check.c: duration, pause/time scale and no double correction.
- settings-check.c: strict seven targets, mocked atomic IO failures and sidecar
  ownership cleanup. Filesystem calls are mocked; it writes no real settings.
- native-state-check.c: Apply/recovery state and failure restoration callbacks.
- commit-check.c: process-lifetime copied transaction, matching outcome and
  persistence/rollback coordination with mocked session/storage callbacks.

Run sh tools/check_portable.sh. It uses Clang with AddressSanitizer and UBSan,
keeps assertions enabled, writes executables only in build/checks, and neither
reads game files nor accesses a game process. These are not new tests added to
fill a checklist and are not gameplay proof.

Excluded private integration checks require generated original-image guards,
full mapped-image fixtures, or developer-local paths: native menu/resource
bootstrap, original bridge, full21patch startup/rollback/coexistence, emitted
native relay/provider integration and watchdog/native mode observations. Their
source fixtures/reports are not bundled. Local guard generation is available in
tools with explicit supported-image inputs, but packaging a portable version of
those integration harnesses remains a gap. No copied original instruction fixture
is substituted. Existing glyph/BC3 and archive checks execute during local asset
generation as documented; malformed archive regression fixtures remain private.
