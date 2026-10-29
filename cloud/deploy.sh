#!/bin/sh
# Deploys the CalcGPT cloud server to Fly.io (see README.md first). The whole project is about 500 MB, so this builds
# from a small temporary folder with only the files the server needs. It never copies .env or devices.json.
# Run from anywhere:  sh cloud/deploy.sh
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/cloud" "$STAGE/calcgpt/mac"
cp "$ROOT/cloud/server.py" "$ROOT/cloud/add_device.py" "$ROOT/cloud/Dockerfile" "$STAGE/cloud/"
if [ -f "$ROOT/cloud/models.json" ]; then cp "$ROOT/cloud/models.json" "$STAGE/cloud/"; fi
cp "$ROOT/calcgpt/mac/calcgptd.py" "$ROOT/calcgpt/mac/models.json" "$STAGE/calcgpt/mac/"
# --ha=false: exactly one machine, because devices.json is on one volume
fly deploy "$STAGE" --config "$ROOT/cloud/fly.toml" --dockerfile "$STAGE/cloud/Dockerfile" --ha=false "$@"
