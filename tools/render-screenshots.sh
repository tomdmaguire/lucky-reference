#!/bin/sh
# Renders every promo-site screenshot in docs/assets (and the Lucky Labs card) in the mode it needs.
# Build LuckyReferenceSnapshot first. reference.png must show the radar ripples, so it uses "playing 1.3" (the settle time of the original hero), not "reference".
set -eu
cd "$(dirname "$0")/.."
snapshot=build/LuckyReferenceSnapshot

"$snapshot" docs/assets/reference.png playing 1.3
"$snapshot" docs/assets/library.png closest
"$snapshot" docs/assets/loaded.png loaded

if [ -d ../lucky-labs/docs/assets ]; then
    cp docs/assets/reference.png ../lucky-labs/docs/assets/lucky-reference.png
    echo "copied reference.png to ../lucky-labs/docs/assets/lucky-reference.png"
fi
