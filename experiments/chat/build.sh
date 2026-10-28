#!/bin/bash
# Build one CalcGPT app from the shared source:  ./build.sh calcgpt   or   ./build.sh triggpt
# The finished .8xp is copied to ../send/.
set -e
APP="$1"
case "$APP" in
  calcgpt) NAME=CALCGPT; DESC="CalcGPT chat"; KB="kb_gas.c" ;;
  triggpt) NAME=TRIGGPT; DESC="TrigGPT chat"; KB="kb_trig.c" ;;
  *) echo "usage: $0 calcgpt|triggpt"; exit 1 ;;
esac
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/../build-$APP"
rm -rf "$OUT" && mkdir -p "$OUT/src"
cp "$HERE"/src/{main,eliza,calc,kb,kb_general,tri,solver}.c "$HERE"/src/{eliza,calc,kb,tri,solver}.h "$OUT/src/"
cp "$HERE/src/$KB" "$OUT/src/"
cp "$HERE/src/app_$APP.c" "$OUT/src/app.c"
cp "$HERE/icon.png" "$OUT/"
sed -e "s/^NAME = .*/NAME = $NAME/" -e "s/^DESCRIPTION = .*/DESCRIPTION = \"$DESC\"/" "$HERE/makefile" > "$OUT/makefile"
export CEDEV="${CEDEV:-$HOME/CEdev}" PATH="$HOME/CEdev/bin:$PATH"
(cd "$OUT" && make 2>&1 | grep -v -E "GNU-stack|deprecated")
mkdir -p "$HERE/../send" && cp "$OUT/bin/$NAME.8xp" "$HERE/../send/"
