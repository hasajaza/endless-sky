# Modified original source files

Base: Endless Sky v0.11.4 (git tag v0.11.4, commit 406661ebc4920816d02ac66e7e89e18554cfe0c8).
All accessibility code lives in source/accessibility (new). The original files below
received only small hooks (about 230 lines in total). The full diff is
source/endless-sky-accessibility-1.0.16.patch.

## Hooks into the game loop

- source/main.cpp: Accessibility::Init / Quit; every SDL event goes to
  Accessibility::HandleEvent first (accessibility commands are consumed there);
  Accessibility::AfterKey after the game handles a key; Accessibility::Step after the
  panels step (normal and integration-test loops).
- source/MainPanel.cpp: Accessibility::FlightStep while the engine's calculation thread
  is idle (between Engine::Step and Engine::Go).
- source/UI.cpp: marks which panel is being drawn, for the screen-text capture.
- source/Messages.cpp: every displayed game message is passed to the speech layer.
- source/Command.cpp: in Command::ReadKeyboard, a key that is part of a held
  accessibility Control chord does not act as a flight control.

## Text capture (labels and generic screen review)

- source/text/Font.cpp: records drawn text (the full text, not the truncated copy).
- source/text/WrappedText.cpp/.h: records wrapped paragraphs as one block; accessor for
  the original text.
- source/Interface.cpp/.h: interface buttons report their label, key and state.
- source/TextArea.h: accessor for the displayed text.
- source/ConversationPanel.h: accessor for a paragraph's text.
- source/BookEntry.h: friend AccessInspector, to read the text of logbook entries.

## Panel identification (the game is built without RTTI)

- source/Panel.h: virtual AccessibilityId() and friend AccessInspector.
- One line ES_ACCESSIBLE_PANEL(...) at the end of each of these classes (identifies the
  panel and lets the accessibility layer read its state): MenuPanel, MainPanel,
  LoadPanel, PreferencesPanel, StartConditionsPanel, ConversationPanel, DialogPanel,
  ShipNameDialogPanel, OptionalInputDialogPanel, PlanetPanel, TradingPanel, BankPanel,
  HiringPanel, SpaceportPanel, ShopPanel, ShipyardPanel, OutfitterPanel, MissionPanel,
  MapPanel, MapDetailPanel, MapShipyardPanel, MapOutfitterPanel, MapSalesPanel,
  PlayerInfoPanel, ShipInfoPanel, LogbookPanel, MessageLogPanel, BoardingPanel,
  HailPanel, GamerulesPanel, GameLoadingPanel.
- friend AccessInspector in: Engine.h, AI.h, ItemInfoDisplay.h, ShipInfoDisplay.h,
  OutfitInfoDisplay.h.

## Installation support

- source/Files.cpp: if no resources are found next to the executable, read
  resources-path.txt (UTF-8, one line) next to it.

## Build and tests

- source/CMakeLists.txt: lists the new files.
- tests/CMakeLists.txt, tests/unit/src/accessibility/test_accessibility.cpp: unit tests.

No gameplay code (physics, AI, combat, economy, missions, saving) was changed.

## New files (source/accessibility)

- Accessibility.h/.cpp: entry points, reader lifecycle, key dispatch, review cursor,
  information commands, settings.
- AccessSpeech.h/.cpp: speech manager (Universal Speech on a worker thread, priorities,
  history, log).
- AccessKeys.h/.cpp: accessibility key bindings, conflict detection, chord reservation.
- AccessCapture.h/.cpp: screen text and button capture per panel.
- AccessAudio.h/.cpp: synthesised OpenAL beacon and cues.
- AccessFlight.h/.cpp: flight monitor, information commands, radar review lists.
- AccessFormat.h/.cpp: bearings, distances, numbers and lists for speech.
- AccessInspector.h: read access to private game state.
- AccessReader.h, AccessReaders.h/.cpp, AccessContext.h, AccessPanelId.h: reader
  framework and generic/flight readers.
- ReadersMenu.cpp, ReadersDialog.cpp, ReadersPlanet.cpp, ReadersShop.cpp,
  ReadersMap.cpp, ReadersInfo.cpp: screen readers.
