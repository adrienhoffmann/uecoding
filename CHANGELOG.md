# CHANGELOG

## Unreleased

### Fixed
- Fix minimap click/ping handling: allow Ctrl/Alt pings inside the clickable area even when outside the visible minimap, clamp click positions to minimap bounds, fix right-click path update to use latest target, and avoid invisible widget areas from consuming Ctrl+click. (See `PlayerHUDWidget.cpp`, `MOBAPlayerController.cpp`)

