#!/usr/bin/env bash
set -euo pipefail
output_dir=$(realpath "${1:?Usage: build-ci.sh OUTPUT_DIRECTORY}")
: "${PACKAGE_VERSION:?}" "${PACKAGE_SYSTEM:?}"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
tar -xf "$output_dir/portalview-$PACKAGE_VERSION.tar.gz" -C "$work"
cd "$work/Portalview-$PACKAGE_VERSION"
nix build --no-update-lock-file ".#packages.$PACKAGE_SYSTEM.portalview" --out-link "$work/result" -L
store_path=$(readlink -f "$work/result")
"$store_path/lib/portalview/freerdp-3.31.1/bin/xfreerdp3" /version | grep -F '3.31.1'
name="portalview-$PACKAGE_VERSION-nix-$PACKAGE_SYSTEM"
printf '%s\n' "$store_path" > "$output_dir/$name.store-path"
# Export the full runtime closure, including Qt and the pinned FreeRDP client.
mapfile -t closure < <(nix-store --query --requisites "$store_path")
nix-store --export "${closure[@]}" | xz -T2 > "$output_dir/$name.nar.xz"
cd "$output_dir"
sha256sum "$name.nar.xz" "$name.store-path" > "SHA256SUMS-nix-$PACKAGE_SYSTEM"
