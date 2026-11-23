#!/bin/bash
# Usage: drive.sh <logfile> <keys-script>
# Runs the Linux build under Xvfb and replays a key script (lines: "sleep N" or xdotool key args).
LOG=$1; SCRIPT=$2; CFG=${CFG:-/tmp/escfg}
export DISPLAY=:57
Xvfb :57 -screen 0 1280x800x24 >/dev/null 2>&1 &
XPID=$!
sleep 1
cd /home/claude/build-linux
ES_ACCESS_STDOUT=1 ./endless-sky -r /home/claude/es-git -c $CFG > $LOG 2>&1 &
GPID=$!
sleep ${STARTWAIT:-12}
WID=$(xdotool search --name "Endless Sky" | head -1)
xdotool windowfocus $WID 2>/dev/null
while read -r line; do
  [ -z "$line" ] && continue
  case "$line" in
    sleep*) sleep ${line#sleep } ;;
    type*) xdotool type --delay 120 "${line#type }" ;;
    x\ *) xdotool ${line#x } ;;
    *) xdotool key --delay 150 $line ;;
  esac
  sleep 0.4
done < $SCRIPT
sleep 1
kill $GPID 2>/dev/null; sleep 1; kill -9 $GPID 2>/dev/null
kill $XPID 2>/dev/null
