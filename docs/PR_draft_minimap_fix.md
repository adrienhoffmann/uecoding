# PR: Fix minimap click/ping handling

Summary
-------
This PR fixes several minimap click edge-cases:

- Allow Ctrl/Alt+click pings when clicking inside the adjusted clickable area (even when outside the visible minimap). Previously such clicks could be rejected by geometry-based bounds checks.
- Ensure right-click movement trace and minimap path update properly by syncing `LastClickWorld`/`LastPathTarget` when path updates arrive.
- Prevent invisible widget areas from consuming Ctrl+clicks (allow ping wheel to show for Ctrl/Alt+left).
- Add verbose logs and debug overlays to make the clickable vs visible minimap areas visible.

Files changed
-------------
- Source/coding/PlayerHUDWidget.cpp / .h — main logic & debug
- Source/coding/MOBAPlayerController.cpp — safe routing to HUD
- docs/SESSION_2025-12-11.md — session notes
- docs/session-2025-12-13.md — session plan (new)

Testing & verification
----------------------
- Play In Editor (PIE) and reproduce the ping wheel using Ctrl+Left in the green clickable region outside visible minimap (upper/outside area).
- Verify right-click path now updates to new clicks regardless of where the click originates (world or minimap).
- Confirm verbose logs show either rejection or clamping messages in `ScreenPositionToWorld` and `HandleMinimapClickViewport`.

Suggested reviewers: @adrienhoffmann

Notes
-----
I did not include large asset edits (WBP/minimap map) in this PR unless you want them added — they were changed while testing in editor. I can include them if desired.
