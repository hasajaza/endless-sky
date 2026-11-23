# Known limitations

Verification
- The Windows build has been compiled but never run on Windows. Universal Speech, NVDA,
  JAWS and SAPI output have not been heard in testing; only the log/standard-output
  backend was exercised (on Linux). The audio cues could not be heard in testing because
  the test machine had no sound device.
- Wine could not run in the build environment, so not even a Windows smoke test was
  possible.

Version
- The build matches Endless Sky 0.11.4 data. Other data versions may produce errors.

Flight
- Steering is still the game's own real-time control. The mod reports bearings, distances,
  drift and threats, plays a beacon, and relies on the game's autopilot (land, jump, auto
  steer, stop, automatic firing) for precision. Dogfighting without automatic firing will
  remain hard.
- The beacon uses stereo panning plus pitch for front and back; true 3D audio (HRTF) is
  not used.
- Ship distances are rounded unless you have a range finder (the game shows exact ranges
  only with one).
- Fleet orders are spoken through the game's own messages; the mod does not list which
  order each escort is currently following.

Screens
- Some screens rely partly on the generic screen-text review (for example parts of
  player info, the logbook and the bank table). Their content is complete but less
  structured.
- Planet descriptions are read in full when landing; press Control to interrupt.
- The game's first-time help dialogs mention the mouse; keyboard equivalents exist and
  are listed by F2.
- In the shipyard, outfitter, trading and hiring screens, some Control combinations belong
  to the game (quantity x20), so the matching accessibility commands are unavailable there.
- Plugin screens that add new panel types are read with the generic screen-text reader.

Text input
- Typing echo announces each character. Pasting with Control+V announces the new text.
