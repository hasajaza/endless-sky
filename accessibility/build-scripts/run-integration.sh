#!/bin/bash
# Run every Endless Sky integration test headless with the given binary.
ES=$1; RES=${2:-/home/claude/es-git}; OUT=${3:-/tmp/itest}
rm -rf $OUT; mkdir -p $OUT
pass=0; fail=0
$ES --config /home/claude/es-git/tests/integration/config -r $RES --tests 2>/dev/null > $OUT/list.txt
while IFS= read -r t; do
  [ -z "$t" ] && continue
  d="$OUT/cfg/$(echo "$t" | tr ' /' '__')"; mkdir -p "$d"; cp -r /home/claude/es-git/tests/integration/config/. "$d"
  if timeout 180 $ES --config "$d" --resources $RES --test "$t" > "$d.log" 2>&1; then pass=$((pass+1)); else fail=$((fail+1)); echo "FAIL: $t"; fi
done < $OUT/list.txt
echo "passed $pass failed $fail"
