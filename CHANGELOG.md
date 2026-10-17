# Changelog

## [0.1.1 (10-17-2026)]

### Fixed
- Game running at 3/4 speed. Tanks, turning, timers and everything else
  now run at the original speed
- Leftover picture at the sides of the menus after quitting a game in
  widescreen

## [0.1.0 (10-15-2026)]

First public beta.

### Added
- Native Windows and Linux builds, including the Steam Deck
- ROM picker. Accepts .z64, .n64 and .v64
- Widescreen with the HUD moved out to the screen edges
- Black bars beside cutscenes in widescreen
- High framerate (Display) with smooth motion for tanks, the camera,
  scenery and explosions
- Controller Pak saves
- Fire (Z) on the right trigger by default
- crash_log.txt generates upon crash
- Licenses
- App icon

### Fixed
- Jitter and stutter while driving and turning at high framerate
- Explosions and particles flickering at high framerate
- Scenery popping in when the framerate dipped (draw distance now stays at
  the max)
- Uneven game speed from timing wobble on PC
- 1-pixel gap at the right and bottom edges of the screen
- Anchored HUD missing on Vulkan (Linux / Steam Deck)
