# Endless Sky Accessible 1.0.23 - keyboard reference

This reference lists the game's own keys (unchanged) and the extra accessibility keys.
All game keys are the defaults of Endless Sky 0.11.4. If you have changed a key in
Preferences, Controls, the game uses your key, and the accessibility layer announces
your key, not the default. In the game, press Control+F2 at any time to load this list,
with your current bindings, into the review cursor.

No key needs a numeric keypad.

## 1. Accessibility keys (added by this mod)

These work on every screen unless noted. None of them replaces a game function.
They can be changed in the file "accessibility keys.txt" in your Endless Sky
configuration folder (see TROUBLESHOOTING.md).

### Speech and help

- F2: describe the current screen and the selected item, and say how many keys it has.
  Press F2 again to hear the keys one at a time ("3 of 12: B buys the selected ship").
  After the last key, F2 starts again from the description.
- Shift+F2: go back to the previous key.
- Control+F2: load the complete key reference (screen, accessibility and game keys)
  into the review cursor.
- Control+R: repeat the last announcement.
- Control+Page Up / Control+Page Down: move back / forward through the speech history
  (the last 300 announcements).
- Control+F12: switch speech output: automatic, then each available engine
  (for example NVDA, JAWS, SAPI 5). The choice is remembered.
- Control+F11: verbosity: minimal (alerts and important events only), normal, verbose
  (all game messages including chatter).
- Control+F10: audio cues on or off (beacon and damage sounds).
- Control+P: pause on arrival on or off. When on (the default), the game pauses each time
  you arrive in a new system; P continues.
- Control+F9: ship chatter on or off (messages from other ships; they stay in the message log).
- Control+Shift+O: motion announcements on or off. When on (the default), the compass
  direction is spoken while you turn with Left, Right or Down, the exact heading when you
  let go, "Full speed" when the ship reaches its top speed and "Stopped" when it stops.

F2 is an unmodified key. If you bind F2 to a game command in Preferences, the game
command wins and the accessibility help is disabled (you are told about this when the
game starts). Change the help key in "accessibility keys.txt" in that case.

### Information

- Control+L: location: system, government, date, planet you are landed on, or the
  nearest object with its bearing and distance.
- Control+S: flagship status, short: shields, hull, fuel (number of jumps), energy,
  heat and speed. Crew only when it is too low. The ship's name is in Control+F.
- Control+T: current target: name, type, government, attitude, bearing, distance,
  shields and hull, and every value the game's own target display shows (crew, energy,
  fuel, heat and weapon ranges only if you have the scanners the game requires),
  closing speed, whether it is in your weapons range, and whether it can be boarded.
- Control+D: destination: landing target, planned route, next jump and its direction,
  fuel needed for the whole route.
- Control+N: planets, stations and stars in this system with landing status,
  bearing and distance.
- Control+H: hostile ships in sight, nearest first.
- Control+F: your fleet (condition, position and formation of each ship), and other
  friendly ships in sight.
- Control+M: your active missions. Press again for the next mission.
- Control+K: credits, cargo, passengers and free bunks.
- Control+G: weapons, ammunition, selected secondary weapon, weapon ranges, energy,
  heat and turret mode.
- Control+O: speed first ("speed 120 of 369", "full speed" or "stopped"), then the
  direction of movement compared with where the nose points (forward, backward,
  drifting left or right) and the compass direction of travel, then where the ship
  faces: compass direction and heading in degrees (0 is north, 90 east).
- Control+E: recent game messages. Press again for older ones.
- Control+B: audio beacon on or off. The beacon pings from the direction of your
  target ship, landing target, asteroid or hyperspace destination. It is panned left
  or right relative to your ship's nose, and its pitch is high when the object is
  ahead and low when it is behind. It pings faster when you are close or lined up.

### Review cursor

The review cursor reads lists that are built from the game's data for the current
screen: nearby objects in flight, systems on the map, commodities, shop items,
missions, dialogue history, settings and so on. Moving the review cursor never
changes the game by itself.

Every review key has two bindings. The function-key set exists because some screen
readers (JAWS in particular) keep Control with the arrow keys for themselves, so those
presses never reach the game. Use whichever set works with your screen reader.

- Control+Down or F6 / Control+Up or F5: next / previous item.
- Control+Right or F8 / Control+Left or F7: next / previous list (group).
- Control+Home or Shift+F5: refresh the list and go to the first item.
- Control+End or Shift+F6: last item.
- Control+Space or F4: read the current item in detail.
- Control+Enter or F9: activate the item: the same as clicking it with the mouse (for
  example select a ship or planet as target, plan a route to a system, choose a
  commodity, select a shop item, press a button). The action happens when you
  release Control, so that the game does not see Control as "buy 20".

In flight, while Control is held, the keys used by these chords (for example Up or L)
do not act as flight controls, so Control+L never makes you land.

### Where the game itself uses Control

In trading and hiring, Control plus Enter, Space, plus, minus, Backspace or Delete
buys, sells, hires or fires 20 at a time, exactly as in the original game. In the
shipyard and outfitter, Control plus B, S, R, U, I or C acts on 20 items. On those
screens these Control keys go to the game and the matching accessibility command is
not available there.

## 2. Game keys (unchanged originals)

### Flight

- Up arrow: forward thrust. Left / Right arrows: turn. Down arrow: reverse thrust,
  or turn to face backwards if you have no reverse thrusters.
- Shift+Down: stop the ship (autopilot brakes).
- E (hold): auto steer toward your target.
- A: afterburner.
- Tab: fire primary weapons. Q: fire secondary weapon. W: select the next secondary
  weapon.
- L: land. Press again within a second to switch to the next landable planet.
  If you selected a landing target with the review cursor, L lands there.
- J: hyperspace jump (hold to let escorts prepare). Shift+J: fleet jump.
- N: select the next ship. Shift+N: cycle through your own escorts.
  R: nearest hostile ship. V: nearest asteroid.
- B: board the selected disabled ship (press again to cycle). T: hail the selected
  ship or planet. S (hold): scan the selected ship.
- D: deploy or recall fighters. C: cloak.
- P: pause. Caps Lock: fast forward. F11: full screen. F3: performance display.
- Plus / minus: zoom.
- Escort groups (each change is spoken):
  - Shift+N: target your next escort. This also selects it alone, so fleet orders then
    go only to that ship. Pressing Shift+N after your last escort clears the selection,
    and orders go to the whole fleet again.
  - Control+Shift+N (mod key): add the escort you targeted to the selection, or remove
    it if it is already there. Repeat Shift+N and Control+Shift+N to select several.
  - Control+digit: save the selected escorts as that group (with nothing selected, it
    empties the group).
  - Digit (0 to 9): select that group. Fleet orders then go to its ships.
  - Shift+digit: add a group to the selection, or remove it if all its ships are
    selected already.
  - Control+F says which groups each ship is in and which ships are selected.
- Fleet orders: F fight my target, G gather around me, H hold position,
  Y toggle hold fire, U toggle escort ammunition use, Z harvest flotsam.
- Shift+G: formation. Each press puts your selected escorts (or all escorts if none are
  selected) into the next of the 14 formations and says its name and shape. After the
  last one, the next press turns formation flying off. Formations only arrange where the
  escorts fly; they do not change speed, shields or weapons.
- M: map. I: player info. Slash (/): message log. F1: help. Escape: main menu.

### Planet

- Up / Down arrows (added): move through the planet's options; Enter selects the
  option. Home and End go to the first and last option.
- T trading, J job board, B bank, P spaceport, S shipyard, O outfitter,
  H hire crew, I player info, M map, D depart. (Only services that exist on
  the planet are offered; F2 tells you which.)

### Trading

- Up / Down: choose a commodity (announced with price, price level, amount held,
  profit per ton and how many you can afford).
- Plus, Enter or Space: buy one ton. Minus, Backspace or Delete: sell one ton.
  Shift x5, Control x20, Alt x500.
- U or Shift+B: buy as much as possible. E or Shift+S: sell all commodities.
- F or Shift+O: sell outfits in cargo. N or Shift+M: sell minerals. R: store outfits.

### Job board and missions map

- Left arrow: available jobs. Right arrow: your accepted missions.
- Up / Down: choose a mission (announced with destination, deadline, cargo,
  passengers, payment and description).
- A: accept. Shift+A: abort the selected accepted mission.

### Shipyard and outfitter

- Arrow keys: move through the items. Each ship is announced with its name, category,
  price and position in the list (such as 2 of 3).
- Shipyard, Space: read the ship's values one at a time (1 of 13: shields, hull, max
  speed, acceleration, turning, cargo space, bunks and crew, fuel, mass, free outfit
  space, weapons, installed outfits, description). Each value is followed by the same
  value of the ship you fly now. Shift+Space goes back. When you move to another ship,
  the value you were listening to is read for that ship too, so you can compare ships
  value by value with the arrow keys.
- Outfitter, Space: read the outfit's values one at a time (mass, outfit space needed,
  range, damage per second and so on, then the description). Shift+Space goes back.
  When you move to another outfit, the value you were on is read for it too, or "No
  range" if it does not have one.
- A: read all values of the selected ship or outfit at once, without stopping.
- Maps in the outfitter (such as Local Map) are not installed on a ship. Buying one
  adds the systems it covers to your star map, with their planets and trade prices.
- Tab: switch between the store and your ships.
- Shipyard: B buy, S sell, R or U sell but keep the outfits in storage.
- Outfitter: B buy and install, S sell, I install from cargo or storage,
  U uninstall, C buy into cargo, R move to storage.
- F: search by name. K: park or unpark escorts. Digits: escort groups.
- L, D or Escape: leave.

### Bank

- Up / Down: choose a debt or the new mortgage row. Enter: pay extra or apply.
  A: pay everything you can.

### Hire crew

- H, plus, Enter or Space: hire. F, minus, Backspace or Delete: fire.

### Map

- Left / Right: choose a destination. Your mission destinations come first, then the
  systems you can reach, nearest first. The list does not wrap around: at the ends you
  hear "First destination" or "Last destination".
- Enter: plan the route to the chosen destination. Then close the map (M) and press J.

- Tab or J: add the next hyperspace link to the route (Shift extends the route).
  Backspace or Delete: remove the last jump.
- F: find a system or planet by name (a text dialog; letters are echoed as you type).
- Up / Down: choose a trade good (Food, Clothing, Medical...). You hear its price here.
  After that, Left and Right also say its price in each system you know, to find where
  to buy cheap and sell high. (With a planet card selected, Up / Down move between the
  planets instead.)
- C: send escorts to the selected system.
- P, T, S, O, I: ports, stars, shipyards, outfitters and missions modes.
- M, D or Escape: close.

### Conversations and dialogs

- Up / Down: choose a response. Enter or 1 to 9: select it.
- Page Up / Page Down (added): re-read earlier paragraphs. Home / End (added): first /
  latest paragraph.
- Name entry: type, Tab switches first and last name, Enter confirms.
- Dialogs: Enter or Space presses the selected button. Tab, Left and Right choose a
  button. Escape cancels. Mission offers: A accept, D decline. M shows the map.

### Boarding

- Up / Down, Page Up / Page Down, Home / End: choose loot. T: take it.
- C: try to capture the ship. During a capture: A attack, D defend.
- X or D: leave.

### Player info and ship info

- Up / Down: select ships. Shift extends the selection. Control+Shift+Up/Down move ships
  in the fleet order. S or Enter: ship info.
- Ship info: Left / Right (or P / N): previous / next ship. R rename, Shift+P or K park,
  Shift+D disown, L logbook, M map, I back to player info.

### Message log and logbook

- Message log (added speech): Up older, Down newer, Home oldest, End newest.
  I important only, C clear.
- Logbook: Up / Down choose a date or category; Control+Down reads the entries.

### Main menu, load panel, new pilot, preferences

- Main menu: Up / Down arrows (added) move through the options and Enter selects one;
  Home and End go to the first and last. The letter keys also work: E enter ship,
  N new pilot, L or M load and save, P preferences, G gamerules, Q quit. (In the
  original game Up and Down only changed the speed of the scrolling credits.)
- Load and save: Left pilots, Right saved games, Up / Down choose, L load,
  N new pilot, A add snapshot, D delete pilot, Shift+R delete snapshot, B back.
- New pilot: Up / Down choose a scenario, Enter start, G gamerules, B back.
- Preferences: C controls, S settings, P plugins, A audio. Up / Down choose,
  Enter changes (on Controls, then press the new key), X or Delete clears a key,
  N / R change page, B back.
- Audio page (added): Up / Down choose a volume, Left / Right change it by 10 percent.
