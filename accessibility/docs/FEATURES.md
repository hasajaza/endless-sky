# Features

Everything below uses the game's own data and rules. The mod reads what a sighted
player can see on screen and never decides anything for you.

## Architecture in one paragraph

A central speech manager (AccessSpeech) owns all output: it runs Universal Speech on its
own thread, so the game never waits for a speech engine; it applies priorities (critical
alerts interrupt, focus changes and requested information interrupt earlier focus speech,
important events queue ahead of background chatter, background messages are dropped when
too many pile up), filters duplicates repeated within 2.5 seconds, keeps a 300-entry
history, and writes a log. Each game screen has a reader (menus, dialogs, conversations,
planet services, shops, map, missions, information screens, boarding, flight) that
announces the screen when it opens, reacts to the game's own keys, and fills the review
cursor. A flight monitor runs inside the game's main step while the physics thread is
idle, so it can safely read ship positions.

## Speech

- Universal Speech: NVDA, JAWS, other supported screen readers, SAPI 5 fallback.
- Control+F12 switches engines; the choice is saved.
- Priority categories: critical, important, focus, requested, background.
- Repeat (Control+R) and history (Control+Page Up / Page Down).
- Three verbosity levels (Control+F11).
- Braille: focus and requested messages are also sent to the braille display when the
  screen reader supports it.
- Announcement when the game window regains focus.

## Every screen

- Opening a screen announces its name, what is selected and its options with their keys
  (taken from the game's interface definitions, so they match exactly what is drawn).
- F2 describes the screen; each further press reads one of its keys, Shift+F2 goes back.
- The review cursor lists the screen's items; the default for any screen is the text the
  game draws (from the game's text renderer, not OCR) plus its buttons, so even screens
  without a dedicated reader can be read and their buttons pressed.

## Main menu, pilots, preferences

- Main menu with pilot summary (name, ship, location, date, credits).
- Load and save screen: pilots and snapshots with date, credits, location, flagship and
  play time.
- New pilot scenarios with full descriptions; name entry with letter-by-letter echo.
- Preferences: controls (with current key and conflicts), settings with their current
  values, plugins, and keyboard volume control on the audio page.
- Gamerules: rules and presets with their values.

## Dialogs and conversations

- Dialog text, text fields (with typing echo), buttons and the selected button.
- Mission offers announce accept and decline keys.
- Conversations: new text is read automatically, followed by the responses; Up and Down
  read each response and say if it is unavailable; Page Up and Page Down re-read
  paragraphs; the review cursor holds the whole conversation and the responses.
- Hails: the ship or planet's reply is read whenever it changes.

## Planets and services

- Landing announces the planet, its system, the available services with their keys and
  the description.
- Spaceport description and news.
- Trading: price, price level, amount held, profit or loss per ton, how much you can
  afford; each purchase or sale reports quantity, cost, cargo and credits; failures say
  why (no money, hold full, nothing to sell). Review list of all commodities.
- Bank: credits, net worth, credit score, each debt with interest, term and payment,
  mortgage qualification, result of paying.
- Hire crew: crew, required crew, bunks, how many can be hired or fired, salaries.
- Job board: available and accepted missions with destination, deadline, cargo,
  passengers, payment, description, and whether you have the space to accept.

## Shipyard and outfitter

- The selected item with price; for outfits also the number installed on the selected
  ship, in cargo and in storage, its category and any license cost.
- The selected ship of yours with free outfit, weapon and engine space.
- Results of buying, selling, installing, uninstalling, storing.
- Review lists: every category of the store, your ships here, and the full
  specifications of the selection (all attributes, energy and heat table, outfits,
  sale value, description).

## Galaxy map

- Selected system: name, government, visited or not, jumps away, direction, missions
  there, planets and their services (only what the map shows you), restrictions.
- Route: jumps, destination, fuel needed against fuel available.
- Review lists: the route, reachable systems nearest first, mission destinations, all
  known systems alphabetically, and the map's own text. Control+Enter plans a route
  exactly as clicking the system does.
- Commodity colouring and planet card selection announced.
- Unexplored systems are never named or described beyond what the map shows.

## Flight

Automatic announcements:
- New target (ship, landing target, asteroid, hyperspace destination) with bearing and
  distance.
- Shields at half, 25 percent and critical; hull at half, 25 percent and critical;
  flagship disabled; low fuel for a jump; entering hyperspace.
- Hostile ships that start attacking you (grouped, rate limited) and incoming guided
  missiles.
- Target in or out of weapons range (with a sound).
- Target disabled or destroyed; your escorts disabled or destroyed.
- Secondary weapon changes and running out of ammunition; pause; fast forward.
- Every game message (landing, jumping, autopilot, scans, fines, hails...) according
  to the verbosity level.

Commands: location, status, target, destination, nearby objects, hostiles, fleet,
missions, cargo, weapons, motion, messages, beacon (see KEYBOARD.md).

Review lists: planets and stations, hostile ships, other ships, disabled ships, your
fleet, minable asteroids, flotsam, hyperspace links, each with live bearing and distance.
Control+Enter selects the object the same way a left click does (planets must be
landable, asteroids need an asteroid scanner, exactly as with the mouse).

Bearings are relative to your ship's nose: "dead ahead", "15 degrees right",
"at 4 o'clock", "directly behind". Ship distances are exact if you have a range
finder (as on the game's display) and rounded otherwise.

Audio cues (synthesised, played through the game's own audio): beacon, shield hit,
hull hit, target in range, end of a list.

## Combat, fleet, mining, boarding

- Combat information through Control+T, Control+H, Control+G and automatic alerts.
  The game's "Automatic firing" preference and auto steer (E) are the original aiming
  aids; the mod does not aim or fire for you.
- Fleet: Control+F lists your ships; the game's fleet keys issue orders and the game's
  messages confirm them; escorts can be selected from the "Your fleet" review list.
- Mining: V selects the nearest asteroid; the "Minable asteroids" list gives the 20
  nearest; collection messages are spoken.
- Boarding: the ship, each item of loot (value, size, whether you can take it and why
  not), capture odds for attacking and defending, and every result message.

## Information screens

- Player info: pilot, credits, net worth, ships with condition; full screen text in the
  review cursor (combat rating, licenses, salaries...).
- Ship info: each ship's condition, cargo and outfit space; review lists of attributes,
  energy and heat, installed outfits and cargo.
- Message log: read message by message with the arrow keys.
- Logbook: selected date or category and its entries.
