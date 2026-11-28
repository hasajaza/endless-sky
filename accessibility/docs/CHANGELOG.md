# Changelog

## 1.0.26 (2026-11-28)

- Logbook: Control+Down now reads the entries of the chosen page (date and text, or name
  and description) instead of the system names drawn on the logbook's map. Up / Down
  announce the page with its number of entries and the first entry.
- Player info review: the pilot details are read line by line ("net worth: 0 credits")
  instead of being mixed with the fleet table's column names.
- Outfitter and shipyard maps: Up / Down say the chosen outfit or ship, its price and the
  known systems that sell it, nearest first, instead of "Map colouring changed". Left
  and Right say whether each system sells it.
- Flight: the first time Down is pressed, the mod explains that it turns the ship
  against its motion (it is not a brake) unless the ship has reverse thrusters.

## 1.0.25 (2026-11-27)

- Aim cues for combat (Control+Shift+B, on by default): with an enemy ship targeted near
  weapons range, a tick panned to the side to turn toward, faster as you line up, and a
  bright chirp while the forward guns are lined up, leading a moving target.
- Player info: K, Shift+P, A and C (parking) and V (save order) are listed in F2 and their
  results are spoken.
- Getting started: new "Fighting" section.

## 1.0.24 (2026-11-27)

- Control+Shift+A: select no escorts, so orders go to the whole fleet. Followed by
  Control and a digit, it empties (disbands) that group.
- Ship info screen by keyboard: each ship is announced with its cargo. Tab and Shift+Tab
  choose a cargo item or a jettisonable outfit, then the game's C or J jettisons just
  that item (in space). W and Shift+W go through the weapon hardpoints; Space picks up a
  weapon and Space on another hardpoint of the same kind swaps them (while landed), the
  keyboard version of dragging with the mouse. While landed, the screen explains that
  the cargo is kept for the whole fleet.

## 1.0.23 (2026-11-27)

- Escort groups by keyboard: Control+Shift+N adds the escort targeted with Shift+N to the
  selection (or removes it), so several escorts can be selected without a mouse. The
  game's group keys now speak: a digit says which ships the group selected (or that it
  is empty and how to make it), Control+digit says which ships were saved, Shift+digit
  says the new selection. Control+F says each ship's groups and which ships are selected
  for orders.
- Getting started: how to make and use escort groups.

## 1.0.22 (2026-11-27)

- Speed and heading, from player reports: Control+O now starts with the speed
  ("speed 120 of 369", "full speed" or "stopped"), then the direction of movement and
  travel, then the compass heading and degrees. Control+S is shorter and in order of
  need: shields, hull, fuel, energy, heat and speed (the ship's name is in Control+F,
  crew only when too low).
- Motion announcements (Control+Shift+O to turn off): the compass direction while you
  turn with Left, Right or Down, the exact heading when you let go, "Full speed" when
  the ship reaches top speed and "Stopped" when it stops (for example after Shift+Down).
- Getting started: new section on flying with escorts and giving fleet orders.

## 1.0.21 (2026-11-26)

- Fleet formations: Shift+G (the game's own key) now always speaks, at every verbosity
  level, with the formation name, a short description of its shape and how to go on.
  Control+F says which formation each escort is flying in.

## 1.0.20 (2026-11-26)

- Map trade prices: the mod gave prices for every system, including systems with no
  inhabited planet, where nothing can be bought or sold. The game itself only shows prices
  for inhabited systems. Uninhabited systems now say "no trading here, uninhabited", also
  when you change the trade good with Up and Down.

## 1.0.19 (2026-11-25)

- The audio cues were working, but it was not clear when to expect them. Control+B now
  says when there is no target yet (the beacon stays silent until you choose one with N,
  R, V, L or a route on the map), and "Beacon silent: no target" is said once when the
  target goes away. Control+F10 says which sounds exist (shield and hull hits, the
  weapon-range chime and the beacon) and warns if the sound system could not start.

## 1.0.18 (2026-11-24)

Fixes for reports from players on audiogames.net:

- The content now comes first and the position after it: conversation responses
  ("Sounds good! 1 of 2"), F2 keys, shop values, the ship list, boarding loot, the job
  board ("Rush delivery to Ingot, job 3 of 25") and Control+M.
- Missions say where every remaining stopover and waypoint is (planet, system, number of
  jumps and direction), and the destination's jumps and direction.
- Deadlines: jobs on offer always said "0 days left". The days left are now counted from
  the date, and the estimated trip length is given ("the trip takes about 4 days"), with a
  warning when it is too late.
- Jobs on offer say how they fit your accepted missions: in the same system, on the way to
  your mission's destination, or how many jumps from it.
- Job board: Control+Space read the first job of the list instead of the selected one; it
  now reads the selected mission.

## 1.0.17 (2026-11-23)

- Works with the free official download of Endless Sky from GitHub
  (EndlessSky-win64-v0.11.4.zip), not only with Steam. The installer finds an extracted
  copy by itself: in the folder around the installer (easiest: extract the mod into the
  game folder), then Steam, then in Downloads, Desktop, Documents and C:\Games. A folder
  can also be given directly: Install.bat "C:\path\to\EndlessSky-win64-v0.11.4".
- README: updated requirements and installation; removed the outdated testing request.

## 1.0.16 (2026-11-23)

- The arrival hint for a mission planet now says "Press L once: the autopilot flies there
  and lands", without the confusing "press it again to choose another planet". The
  mission planet is already the landing target when you arrive.

## 1.0.15 (2026-11-23)

- Map: after choosing a destination with Left or Right, Up and Down (trade goods) also
  say the good's price in that destination, so you can go through all goods for one
  system without moving away from it. When the price there is not known (a system you
  have not visited and no map covers), it says so instead of leaving it out.

## 1.0.14 (2026-11-23)

- Deleting a pilot (D in Load and save): the pilot's name is filled in for you, so you
  only press Enter to delete or Escape to cancel. Typing the name by ear often failed:
  capitals must match, and a space typed at the end of the first name when the pilot was
  created becomes part of the name.

## 1.0.13 (2026-11-23)

- Pause on arrival: the game pauses each time you arrive in a new system, so you can
  listen to the messages and decide what to do. The hint then ends with "Game paused.
  Press P to continue." The map (M) and the information commands work while paused.
  Control+P turns this on or off; it is on by default.
- When there is no fuel left, the hint no longer says "Press J"; it says to land and
  refuel first.
- Routes that need more fuel than you have say to refuel on an inhabited planet on the
  way.
- Outfitter: rows without a value (such as "Cannot be plundered while installed") are
  read as plain sentences; once you have listened to the description with Space, it is
  no longer repeated for every item you move to (the same for ships).
- Map: the systems of mission stopovers (such as Caph for "Moving House") are named, as
  the mission text names them, instead of "unexplored system".

## 1.0.12 (2026-11-22)

- The warning for pressing J without a route now always comes (it is triggered by the
  game's "Engaging autopilot to jump" message), and is skipped when there is no fuel,
  since the game then does not jump.
- Control+F9 turns ship chatter on or off. Chatter stays in the message log.
- Long game help messages that repeat on every arrival ("Press M to bring up your
  map...") are spoken at most twice per session; they stay in the message log.
- Trading: Up at the top and Down at the bottom say "Top of the list" or "End of the
  list" once, instead of repeating the same commodity.
- Trading: an overfilled cargo hold is reported as such; "you can buy" is never negative.
- Map: when a destination has no known route, the closest system you can reach in
  that direction is suggested, and Enter then plans a route to it.
- Attack alerts say "Under attack by ..." without repeating "hostile" and "attacking".
- "Fuel for 0 jumps only" is now "No fuel left for jumping".

## 1.0.11 (2026-11-21)

- Planning a route on the map while landed now says: close the map, press D to take
  off, then J in space. (On a planet, J opens the job board.)
- Arriving in the last system of the route says "End of your route" with the mission
  planet to land on, and not to press J there.
- Pressing J without a planned route (the game then jumps to whichever linked system
  the ship faces) gives a warning naming that system, says that an arrow key cancels,
  and how many jumps of fuel are left.
- Take-off and arrival hints say when fuel is left for fewer than 2 jumps.

## 1.0.10 (2026-11-21)

- Map: Left and Right no longer wrap around. Left from the first destination used to
  jump to the last one (often a far unexplored system); now it says "First destination"
  and stays. Right at the end says "Last destination". The first press of either key
  starts at the first destination.
- Shipyard and outfitter: A reads all values of the selection at once.

## 1.0.9 (2026-11-21)

- Outfitter: Space reads the selected outfit's values one at a time, Shift+Space goes
  back, and moving to another outfit reads the same value for it ("No range" if it has
  none). "/ second" is read as "per second".
- Maps in the outfitter no longer say "0 installed": they are described as maps that add
  nearby systems to the star map, and buying one says so.
- Galaxy map: Up and Down announce the chosen trade good and its price here; Left and
  Right then include its price in each known system.

## 1.0.8 (2026-11-21)

- Shipyard: Space reads the selected ship's values one at a time ("2 of 13, Hull: 600.
  Your Star Barge: 1,000."), Shift+Space goes back. Moving to another ship repeats the
  value you were on, so ships can be compared value by value. The announcement when
  moving between ships is short again: name, category, price and position.

## 1.0.7 (2026-11-20)

- Map: Left and Right arrows choose a destination (mission destinations first, then
  reachable systems, nearest first) and Enter plans the route. Opening the map says
  which system you are in and where your missions go.
- After taking off or arriving in a system, a hint says what to do next: "Your mission
  destination New Greenland is in this system. Press L to land", "Next jump: Sol. Press J
  to jump", or how to plan a route on the map. Control+D includes the same hint.
- New guide: docs/GETTING_STARTED.txt, a step-by-step walk through the first trip.

## 1.0.6 (2026-11-20)

- F2 again covers all the keys of a screen, but one at a time: the first press describes
  the screen and says how many keys there are, each further press reads the next key
  ("3 of 12: ..."), and Shift+F2 goes back. It starts over when the screen changes.
- The full key reference moved from Shift+F2 to Control+F2. An existing "accessibility
  keys.txt" that still has Shift+F2 for it is updated automatically.

## 1.0.5 (2026-11-20)

- Shipyard: moving to a ship now announces its category, price, place in the list and
  its main numbers (shields, hull, top speed, cargo, crew and bunks, fuel, free outfit
  space, gun ports, turret mounts), so ships can be compared with the arrow keys.
- Shipyard and outfitter: Space reads the full specifications, installed outfits and
  the description of the selected item.
- F2 now says only the main keys of the screen (at most five). Shift+F2 still lists all.
- Buying or selling that opens a dialog no longer says "Nothing changed"; the result is
  announced when the dialog closes.
- Typing in text fields says "cap" before capital letters, because pilot and ship names
  must be typed with the right capitals (for example when deleting a pilot). When the
  typed text is refused, the text field is read back.
- The fleet command (Control+F) says "No active ships" instead of an empty list.
- Troubleshooting: the "Reduce large graphics" preference for laptops whose graphics hang or crash.

## 1.0.4 (2026-11-20)

- Fixes "Entry point not found" when starting 1.0.3. The official OpenAL32.dll used in
  1.0.3 needs newer runtime libraries (libstdc++-6.dll, libwinpthread-1.dll) than the ones
  this mod ships, so Windows refused to start the game. The sound library is back to the
  mod's own OpenAL32.dll from 1.0.2. SDL2.dll and glew32.dll stay the official ones; every
  import of every file in the game folder was checked against the DLLs that provide it.

## 1.0.3 (2026-11-20)

- Uses the official Endless Sky 0.11.4 release's own SDL2.dll (2.32.10), glew32.dll (2.2.0)
  and OpenAL32.dll (OpenAL Soft 1.23.1), the window, OpenGL and sound libraries, instead of
  copies built for this mod. This is to find out whether the AMD driver crash after the
  first conversation is caused by those libraries. Nothing else changed.

## 1.0.2 (2026-11-20)

- Diagnostic release: if the game crashes, "crash report.txt" is written to
  %APPDATA%\endless-sky with the screen that was open and the call stack, so the cause can
  be found. Nothing else changed.

## 1.0.1 (2026-11-20)

- Main menu: Up and Down arrows now move through the menu options (each option is spoken
  with its letter and position), Enter selects, Home and End jump. Previously the arrows
  only changed the credits scrolling speed, which made a tick sound and said nothing.
- Planet screen: the same arrow and Enter navigation for the planet's services.
- Review cursor: second key set on function keys (F5/F6 items, F7/F8 lists, Shift+F5/F6
  first/last, F4 details, F9 activate), because JAWS keeps Control+arrows for itself.
- "accessibility keys.txt" is rewritten at start-up so it always lists every command,
  including the new alternate keys (your own changes are kept).
- The "Endless Sky, <screen>" announcement on window focus is no longer repeated during
  loading or when focus bounces several times within 3 seconds.

## 1.0.0 (2026-11-20)

First release, based on Endless Sky 0.11.4.

- Central speech manager with Universal Speech (screen readers and SAPI 5), worker
  thread, priorities, duplicate suppression, history, repeat, engine switching,
  verbosity levels, braille output, speech log.
- Accessibility key bindings using Control and F2 only, configurable, with conflict
  detection against the game's bindings; Control chords never leak into flight controls.
- Generic screen reader for any panel: text from the game's renderer and interface
  buttons with their keys.
- Readers for: main menu, load/save, new pilot scenarios, preferences (with keyboard
  volume control), gamerules, dialogs, conversations (with name entry), hails, planet,
  spaceport, trading, bank, hiring, job board, shipyard, outfitter, galaxy map (all modes),
  player info, ship info, message log, logbook, boarding.
- Flight monitor: targets, shields and hull thresholds, disabled flagship, fuel, hyperspace,
  attackers, incoming missiles, weapons range, kills and losses, secondary weapons,
  ammunition, pause, fast forward.
- Information commands: location, status, target, destination, nearby, hostiles, fleet,
  missions, cargo, weapons, motion, messages.
- Review cursor with flight radar lists, map system browser, shop catalogues, commodity
  table, mission lists, conversation history and key reference.
- Synthesised audio beacon and cues through the game's OpenAL context.
- Optional resources-path.txt so the accessible build runs from its own folder against an
  existing installation.
- Installer and uninstaller scripts.
- Unit tests for the accessibility formatting, text and capture logic.
