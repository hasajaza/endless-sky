# Endless Sky Accessible

An unofficial screen reader accessibility mod for [Endless Sky](https://github.com/endless-sky/endless-sky) 0.11.4, so that blind players can play the whole game with the keyboard and speech.

This is not an official Endless Sky release and is not made or endorsed by the Endless Sky developers. Please do not report problems with this mod to the Endless Sky project; open an issue in this repository instead.

The mod was developed by Hassan with the help of an AI assistant (Claude, by Anthropic). The Endless Sky project does not accept AI-assisted contributions, which is why this mod is published separately and is not offered to the official game.

## What it does

- Speaks every screen: menus, dialogs, conversations, planets, trading, job board, bank, hiring, shipyard, outfitter, map, player and ship information, logbook, message log, boarding and hailing.
- Speech through NVDA, JAWS or other screen readers via Universal Speech, with Windows SAPI as a fallback. Speech priorities, interruption, repeat (Control+R) and a history of the last 300 announcements.
- Flight: targets with direction (degrees or clock position) and distance, shields, hull, fuel, attacks, incoming missiles, weapon range, hyperspace and landing, plus an optional stereo audio beacon toward the target.
- Travel help: the map lists mission destinations and reachable systems with Left and Right, plans the route with Enter, and gives trade prices per system. After every arrival a hint says what to do next, and the game can pause on arrival (Control+P) so you have time to listen.
- Shops: ship and outfit values one at a time with Space, compared with your own ship, or all at once with A.
- Information commands at any time: location (Control+L), status (Control+S), target (Control+T), destination (Control+D), nearby planets (Control+N), hostiles (Control+H), fleet (Control+F), missions (Control+M), cargo (Control+K) and more.
- F2 describes the current screen and then reads its keys one at a time. Control+F2 loads the full key reference.
- The game's own keys and mechanics are unchanged. Nothing hidden is revealed: unexplored systems stay unexplored.

## Download and install (Windows 10 or 11)

You need Endless Sky 0.11.4, which is free: either from [Steam](https://store.steampowered.com/app/404410/Endless_Sky/) or the official download from GitHub, [EndlessSky-win64-v0.11.4.zip](https://github.com/endless-sky/endless-sky/releases/tag/v0.11.4) extracted to any folder. The mod uses the game data of that copy and does not change it.

1. Download the latest version: [EndlessSky_Access_1.0.18.zip](https://github.com/hasajaza/endless-sky/raw/downloads/EndlessSky_Access_1.0.18.zip). All downloads are listed on the [downloads page](https://github.com/hasajaza/endless-sky/tree/downloads).
2. Extract the zip and run `Install.bat`. With the GitHub version, the easiest is to extract the mod into the game folder first. The installer finds the game (a GitHub copy around the installer, then Steam, then GitHub copies in Downloads, Desktop, Documents and C:\Games), installs the mod to `%LOCALAPPDATA%\Endless Sky Accessible`, and creates desktop and Start menu shortcuts. You can also name the game folder: `Install.bat "C:\path\to\EndlessSky-win64-v0.11.4"`.
3. Start "Endless Sky Accessible" from the desktop shortcut.

Your pilots and settings are shared with the normal game, in `%APPDATA%\endless-sky`.

New to the game? Read [docs/GETTING_STARTED.txt](docs/GETTING_STARTED.txt). All keys are in [docs/KEYBOARD.md](docs/KEYBOARD.md).

## Documentation

- [Getting started](docs/GETTING_STARTED.txt): your first trip, step by step
- [Keyboard reference](docs/KEYBOARD.md)
- [Features](docs/FEATURES.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md), including graphics driver crashes and hangs
- [Known limitations](docs/KNOWN_LIMITATIONS.md)
- [Changelog](docs/CHANGELOG.md)
- [Modified files](docs/MODIFIED_FILES.md) and [building](docs/BUILDING.md)
- [Testing report](docs/TESTING_REPORT.md)

## Source code

This repository is a fork of Endless Sky. The `accessibility` branch is the official game at tag v0.11.4 plus the mod:

- `source/accessibility/`: the accessibility layer (speech, key handling, one reader per screen, flight monitor, audio cues).
- Small hooks in about 30 original files, listed in [docs/MODIFIED_FILES.md](docs/MODIFIED_FILES.md).
- `tests/unit/src/accessibility/`: unit tests.
- `accessibility/build-scripts/`: scripts used to cross-compile the Windows build with MinGW-w64 and to run the game tests.
- `accessibility/installer/`: the Windows install and uninstall scripts.

To see every change compared with the official game: `git diff v0.11.4 accessibility`.

## Licence

Endless Sky and this mod are licensed under the GNU General Public License version 3 or later (see `license.txt` and `copyright`). The Windows release bundles Universal Speech and third-party libraries under their own licences, included in the release zip.
