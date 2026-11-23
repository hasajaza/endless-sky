# Testing report (1.0.0, 2026-11-20)

## Environment

- Build machine: Linux (Ubuntu), no Windows, no screen reader, no sound device.
- Game: Endless Sky v0.11.4 from the official repository (the supplied sounds folder
  matches v0.11.3 and v0.11.4 exactly; 0.11.4 is the latest release, so it was used).

## Verified

Windows build
- The complete game with the accessibility layer cross-compiles for 64-bit Windows with
  MinGW-w64 GCC 13 (Release, link-time optimisation), with no warnings in the new code.
- Every DLL the executable imports is either a Windows system DLL or included in the
  package (dependency walk with objdump).
- UniversalSpeech.dll (64-bit, from the supplied archive) exports speechSay, speechStop,
  speechGetValue, speechSetValue and speechGetString, which the game loads at run time.

Linux build of the same source (used to run the game)
- Unit tests: all 106 test cases of the game pass, including 10 new accessibility test
  cases (75 assertions): bearings, clock positions, compass directions, number and list
  formatting, motion, speech text cleaning, typing echo, paragraph splitting and screen
  text capture.
- All 45 of the game's own integration tests (scripted gameplay: landing, flight,
  saving and loading, conversations and name entry, job board and mission offers,
  shipyard and outfitter missions, ship sales, outfit storage, capture overrides,
  wormhole navigation and more) pass with the accessibility layer active, which
  indicates that game behaviour is unchanged.
- The game was run under a virtual display and driven only by simulated key presses.
  The speech output (logged) was checked for: loading, main menu, F2 help, new pilot
  scenarios, name entry with echo, conversations with responses, game help dialogs,
  shipyard browsing and purchase with ship naming, planet arrival and services, trading
  (buy, sell, x5, x20 with Control+Enter, failures), job board, hiring, outfitter
  catalogue, player info, ship info, preferences (controls, rebinding, settings values,
  audio volume keys), load and save screen, departure, Control+S, Control+N, Control+T,
  Control+D, Control+L, Control+O, Control+H, Control+F, Control+G, Control+E,
  Control+M, the flight review lists with target selection, galaxy map with the system
  browser and route planning, hyperspace jump, autopilot landing, pause, main menu.
  docs/sample-session-log.txt is the speech log of one such session.
- A Control chord whose Control key is released before the letter (for example
  Control+L) no longer reaches the flight controls (this was found and fixed in
  testing).

## Not verified (needs testing on Windows)

- Speech through NVDA, JAWS and SAPI 5 via Universal Speech; engine switching; braille.
- Window focus announcements on Windows.
- The audio beacon and cues (no sound device here).
- The PowerShell installer and uninstaller (not executed; written for Windows
  PowerShell 5.1).
- Running the Windows executable at all: Wine could not start in the build
  environment.
- Boarding, capture, mining collection, combat alerts (attackers, missiles, damage
  thresholds) and hails were implemented from the game's source but not reached in the
  scripted sessions.

## How to report problems

Send %APPDATA%\endless-sky\accessibility log.txt and errors.txt, and say what you pressed
and what you expected.
