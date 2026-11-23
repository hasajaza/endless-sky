#!/bin/bash
# Copy the executable and every non-system DLL it needs (recursively) into $2.
EXE=$1; DEST=$2
mkdir -p "$DEST"
cp "$EXE" "$DEST/"
SEARCH="/opt/mw/bin /usr/lib/gcc/x86_64-w64-mingw32/13-posix /usr/x86_64-w64-mingw32/lib"
queue=("$EXE"); seen=""
while [ ${#queue[@]} -gt 0 ]; do
  f="${queue[0]}"; queue=("${queue[@]:1}")
  for dll in $(x86_64-w64-mingw32-objdump -p "$f" | awk '/DLL Name/{print $3}'); do
    case " $seen " in *" $dll "*) continue;; esac
    seen="$seen $dll"
    found=""
    for d in $SEARCH; do [ -f "$d/$dll" ] && found="$d/$dll" && break; done
    if [ -n "$found" ]; then cp "$found" "$DEST/"; queue+=("$found"); echo "bundled $dll"; fi
  done
done
