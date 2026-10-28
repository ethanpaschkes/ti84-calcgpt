#!/bin/bash
# Check the model-written questions against an app.   ./run_check.sh calcgpt|triggpt
set -e
APP="$1"; HERE="$(cd "$(dirname "$0")" && pwd)"; SRC="$HERE/../src"
case "$APP" in
  calcgpt) KB=kb_gas.c; APPC=app_calcgpt.c ;;
  triggpt) KB=kb_trig.c; APPC=app_triggpt.c ;;
  *) echo "usage: $0 calcgpt|triggpt"; exit 1 ;;
esac
cd "$HERE"
clang -w -o check_$APP check_paraphrases.c "$SRC/kb.c" "$SRC/kb_general.c" "$SRC/$KB" "$SRC/$APPC" "$SRC/eliza.c" "$SRC/calc.c" "$SRC/tri.c" -I "$SRC" -lm
if [ "$APP" = calcgpt ]; then FILE=paraphrases-calcgpt.tsv; else
  # general answers were tested with the CalcGPT run; TrigGPT adds its own
  cat paraphrases-calcgpt.tsv paraphrases-triggpt.tsv > .combined.tsv 2>/dev/null; FILE=.combined.tsv; fi
./check_$APP "$FILE" | tee "report-$APP.txt"
