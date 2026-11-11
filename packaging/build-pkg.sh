#!/bin/bash
# Builds a macOS installer package for Lucky Reference.
# Usage: build-pkg.sh <version> <path to Lucky Reference.vst3> <output .pkg>
set -euo pipefail

version="$1"
vst3="$2"
output="$3"
here="$(cd "$(dirname "$0")" && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

mkdir -p "$work/vst3/Library/Audio/Plug-Ins/VST3" "$work/packages"
ditto "$vst3" "$work/vst3/Library/Audio/Plug-Ins/VST3/Lucky Reference.vst3"

pkgbuild --analyze --root "$work/vst3" "$work/vst3.plist" > /dev/null
plutil -replace 0.BundleIsRelocatable -bool NO "$work/vst3.plist"

pkgbuild --quiet --root "$work/vst3" --component-plist "$work/vst3.plist" \
  --identifier com.luckylabs.luckyreference.vst3 --version "$version" --install-location / \
  "$work/packages/LuckyReference-VST3.pkg"

sed "s/__VERSION__/$version/g" "$here/distribution.xml" > "$work/distribution.xml"
productbuild --quiet --distribution "$work/distribution.xml" --package-path "$work/packages" \
  --resources "$here/pkg-resources" "$output"

echo "built $output"
