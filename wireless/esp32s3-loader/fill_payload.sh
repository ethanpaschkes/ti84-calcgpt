#!/bin/zsh
# Puts the files the loader sends into main/payload, where the build picks them up:
#   CALC, CALCGPT, SOLVER and the C libraries from ../../send (the calculator build output; run tools/check_send.py --fix first),
#   arTIfiCE v2.1 (OS 5.5-5.8.4) and v3 (OS 5.8.5) from GitHub,
#   TI's Inequalz app (.8ek), which arTIfiCE v3 needs: from $INEQUALZ, or the calculator backup in ../../backup.
set -e
cd "${0:A:h}"
mkdir -p main/payload
cp ../../send/CALC.8xp ../../send/CALCGPT.8xp ../../send/SOLVER.8xp ../../send/clibs.8xg main/payload/
tmp=$(mktemp -d)
curl -sfL -o "$tmp/artifice.zip" https://github.com/YvanTT/arTIfiCE/releases/download/v3/arTIfiCE_v3_CE.zip
unzip -q -o -j "$tmp/artifice.zip" "arTIfiCE_v3_CE/arTIfiCE.8xp" "arTIfiCE_v3_CE/INEQUVAR.8xv" -d main/payload
curl -sfL -o main/payload/arTIfiCE2.8xp https://github.com/YvanTT/arTIfiCE/releases/download/v2.1/arTIfiCE_v2.1.8xp
rm -r "$tmp"
inequalz=${INEQUALZ:-$(ls ../../backup/*/Inequalz.8ek 2>/dev/null | head -1)}
if [[ -z "$inequalz" ]]; then
  echo "Inequalz.8ek not found: download it from TI (education.ti.com, TI-84 Plus CE apps) and run INEQUALZ=/path/Inequalz.8ek $0" >&2
  exit 1
fi
cp "$inequalz" main/payload/Inequalz.8ek
ls -l main/payload
