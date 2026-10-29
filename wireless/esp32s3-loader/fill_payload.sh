#!/bin/zsh
# Puts the files the loader sends into main/payload, where the build picks them up:
# CALCGPT, SOLVER and the C libraries from ../../send (the calculator build output), and the arTIfiCE v3 jailbreak from GitHub.
set -e
cd "${0:A:h}"
mkdir -p main/payload
cp ../../send/CALCGPT.8xp ../../send/SOLVER.8xp ../../send/clibs.8xg main/payload/
tmp=$(mktemp -d)
curl -sfL -o "$tmp/artifice.zip" https://github.com/YvanTT/arTIfiCE/releases/download/v3/arTIfiCE_v3_CE.zip
unzip -q -j "$tmp/artifice.zip" "arTIfiCE_v3_CE/arTIfiCE.8xp" "arTIfiCE_v3_CE/INEQUVAR.8xv" -d main/payload
rm -r "$tmp"
ls -l main/payload
