# Troubleshooting

## Files and folders

- Game program: %LOCALAPPDATA%\Endless Sky Accessible (default install folder).
- Game data location: the file resources-path.txt in the install folder contains the
  path of your Endless Sky folder. Edit it with Notepad if you move the game.
- Configuration folder (shared with the normal game): %APPDATA%\endless-sky
  - accessibility log.txt: everything that was spoken in the last session, with times
    and priorities. Overwritten at each start.
  - accessibility.txt: verbosity, audio cues, cue volume, SAPI fallback, speech engine.
  - accessibility keys.txt: the accessibility key bindings (created on first start).
  - errors.txt: the game's own error log.
  - saves, preferences.txt, keys.txt: the game's own files.

## The game hangs while loading, or crashes after a few minutes

This happens on some laptops with integrated graphics (for example AMD Athlon or Ryzen
with Radeon graphics), with this mod and with the Steam version alike. Fixes, in order:

1. In Preferences, set "Reduce large graphics" to "all" (press P on the main menu, then
   move to that setting and press Enter until it says "all"). This loads large pictures
   at reduced size and fixed a loading hang on an AMD Athlon Silver 3050U.
2. Update the graphics driver from the manufacturer (AMD's "Auto-Detect and Install"
   tool, or the laptop maker's support site).
3. As a last resort, install the software rendering add-on, which draws with the
   processor instead of the graphics driver (slower).

If the game cannot be started at all to change the preference, close it and edit
preferences.txt in %APPDATA%\endless-sky with Notepad: change or add the line
"Reduce large graphics" 2

## No speech at all

1. Check that UniversalSpeech.dll and nvdaControllerClient.dll are in the install
   folder next to Endless Sky.exe.
2. Open accessibility log.txt. If it contains announcements, the game is talking but the
   speech engine is not: press Control+F12 to switch engines; the new engine announces
   itself.
3. If no screen reader is running, Windows SAPI 5 is used. Make sure "sapi fallback 1"
   is in accessibility.txt.
4. If the log is empty or missing, the game did not start: check errors.txt.

## The game says it cannot find its resources or data

The install folder must contain resources-path.txt with the path of the Endless Sky
folder (the one with credits.txt, data, images and sounds). Run Install.bat again, or
edit the file. The path must be on one line, without quotes.

## Data errors after a Steam update

This build is Endless Sky 0.11.4. If Steam updates the game, the data may contain things
0.11.4 does not understand. errors.txt will list them. Options: use Steam's betas or
properties to stay on 0.11.4, or wait for an updated accessible build (the source patch in
the source folder can be applied to a newer version by a developer).

## Too much or too little speech

- Control+F11 cycles verbosity: minimal, normal, verbose.
- Control+F10 turns the audio cues off.
- In accessibility.txt, "cue volume" sets the beacon and cue volume (0 to 1).

## A key does nothing

- The game's own keys are those in Preferences, Controls. F2 tells you the keys for the
  current screen.
- If you bind F2 (or any unmodified accessibility key) to a game command, the game command
  wins. You are told at start-up. Change "context help" in accessibility keys.txt, for
  example to: "context help" ctrl "F1"
- On AZERTY keyboards the slash key for the message log needs Shift; rebind
  "View message log" in Preferences, Controls if needed.

## Control+arrow keys do nothing

Some screen readers (JAWS in particular) use Control with the arrow keys for their own
commands, so the game never receives them. Use the function-key alternatives: F6 next item,
F5 previous item, F8 next list, F7 previous list, F4 details, F9 activate.

## Rebinding accessibility keys

accessibility keys.txt has one line per command:
  "status" ctrl "s"
Review commands also have a second line ending in "alternate", for example:
  "review next alternate" "F6"
Modifiers are ctrl, shift and alt; the key name is an SDL key name such as "F2",
"Page Up", "Return", "Up", "s". Use "none" to disable a command. Restart the game after
editing.

## Restoring the original game

Run Uninstall.bat, or delete the install folder and the two shortcuts. The Steam game and
your saves are untouched.
