# Changelog

## [Unreleased]

### Added
- Mouse support, like in Global Assault. Mouse buttons, the side buttons
  and the scroll wheel can be bound on the Controls tab. Left click fires
  the main gun and right click is R by default
- Mouse Stick on the General tab: turn the tank with the mouse (Turn),
  drive with it too (Full), or turn the turret on its own (Turret), with a
  sensitivity slider. Off by default. Turret turns on the GameShark code
  that frees the turret by itself
- The mouse cursor is hidden over the game while you play (it shows again
  in the menu)

### Fixed
- The game kept one CPU core busy at all times, even on the title screen,
  while waiting for the next frame. Better battery life on the Steam Deck

## [0.1.2 (11-05-2026)]

### Added
- Cheats tab: the game's own codes as on/off toggles, no typing needed
- Hacks tab: GameShark codes for this game (radar, cloak, infinite
  ammo and weapons, green or invisible tanks, go through walls and more)
- Screen Filter (CRT, Scanlines, LCD) and Bloom options at the end of the
  Graphics tab, like in Global Assault. Off by default

### Fixed
- Radar arrows over the other tanks stuttering at high framerate. They now
  move as smoothly as the tanks, like in Global Assault, and stay on their
  tanks in widescreen
- Jitter when turning (and driving) at high framerate. Your tank's turning
  wasn't smoothed between frames; now it is, like in Global Assault. The
  game's frame time also now comes from the screen refreshes instead of
  the PC clock, so it's exactly even
- Slowdown and memory use climbing in busy scenes above the Original
  framerate, like a long 2-player match in the Arena

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
