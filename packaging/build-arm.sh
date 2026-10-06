#!/usr/bin/env bash
set -euo pipefail

recipe_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
output_dir=${1:?Usage: bash packaging/build-arm.sh OUTPUT_DIRECTORY}
mkdir -p -- "$output_dir"
output_dir=$(realpath -- "$output_dir")
image=portalview-qt-arm64:latest

docker image inspect omarchy-qt-arm64:latest >/dev/null
docker build --platform linux/arm64 -t "$image" -f "$recipe_dir/Dockerfile.arm64" "$recipe_dir"
docker run --rm --platform linux/arm64 \
    --user "$(id -u):$(id -g)" \
    --mount "type=bind,src=$recipe_dir/PKGBUILD,dst=/recipe/PKGBUILD,readonly" \
    --mount "type=bind,src=$output_dir,dst=/work" \
    --workdir /work -e SRCDEST=/work -e PKGDEST=/work \
    "$image" /usr/bin/bash -euc '
        test "$(uname -m)" = aarch64
        mkdir -p arm-build
        cp /recipe/PKGBUILD arm-build/PKGBUILD
        cd arm-build
        printf "source /etc/makepkg.conf\nPKGEXT=\047.pkg.tar.zst\047\n" > makepkg.conf
        makepkg --config "$PWD/makepkg.conf" --cleanbuild --force
    '
