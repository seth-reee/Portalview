#!/usr/bin/env bash
# Stage a trusted, architecture-matching Arch FreeRDP 3.31.1 package.
set -euo pipefail
archive=$(realpath "${1:?Usage: install-private-freerdp.sh PACKAGE [DESTINATION]}")
destination=${2:-${XDG_DATA_HOME:-$HOME/.local/share}/portalview/freerdp-3.31.1}
metadata=$(bsdtar -xOf "$archive" .PKGINFO)
[[ "$metadata" == *'pkgname = freerdp'* && "$metadata" == *'pkgver = 2:3.31.1-1'* ]] || {
  echo 'Expected the FreeRDP 3.31.1-1 Arch package' >&2; exit 1;
}
[[ "$metadata" == *"arch = $(uname -m)"* ]] || { echo 'Package architecture does not match this machine' >&2; exit 1; }
[[ ! -e "$destination" ]] || { echo "Destination already exists: $destination" >&2; exit 1; }
staging=$(mktemp -d)
trap 'rm -rf "$staging"' EXIT
bsdtar -xf "$archive" -C "$staging"
mkdir -p "$staging/runtime/bin" "$staging/runtime/lib" "$staging/runtime/licenses"
cp "$staging/usr/bin/xfreerdp3" "$staging/runtime/bin/"
for library in libfreerdp-client3 libfreerdp3 libwinpr3; do
  cp -a "$staging/usr/lib/$library.so"* "$staging/runtime/lib/"
done
# Arch installs the Apache text through the system SPDX license package.
cp /usr/share/licenses/spdx/Apache-2.0.txt "$staging/runtime/licenses/FreeRDP-Apache-2.0.txt"
printf '%s\n' "$metadata" > "$staging/runtime/licenses/package-info.txt"
sha256sum "$archive" > "$staging/runtime/licenses/package-sha256.txt"
LD_LIBRARY_PATH="$staging/runtime/lib" "$staging/runtime/bin/xfreerdp3" /version
mkdir -p "$(dirname "$destination")"
mv "$staging/runtime" "$destination"
printf 'Installed private runtime: %s\n' "$destination"
