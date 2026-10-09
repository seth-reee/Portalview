#!/usr/bin/env bash
set -euo pipefail
output_dir=$(realpath -- "${1:?Usage: bash packaging/verify-arm.sh OUTPUT_DIRECTORY}")
docker run --rm --platform linux/arm64 \
    --user "$(id -u):$(id -g)" \
    --mount "type=bind,src=$output_dir,dst=/work" --workdir /work \
    portalview-qt-arm64:latest /usr/bin/bash -euc '
        mkdir -p verify-aarch64/runtime
        chmod 700 verify-aarch64/runtime
        bsdtar -xf portalview-0.1.2-1-aarch64.pkg.tar.zst -C verify-aarch64
        readelf -h verify-aarch64/usr/bin/portalview | grep Machine
        set +e
        QT_QPA_PLATFORM=offscreen \
        XDG_RUNTIME_DIR=/work/verify-aarch64/runtime \
        XDG_DATA_HOME=/work/verify-aarch64/data \
        XDG_CONFIG_HOME=/work/verify-aarch64/config \
            timeout 15 verify-aarch64/usr/bin/portalview > verify-aarch64/smoke.log 2>&1
        smoke_status=$?
        set -e
        cat verify-aarch64/smoke.log
        test "$smoke_status" -eq 124
        test ! -s verify-aarch64/smoke.log
        printf "ARM package offscreen smoke test passed.\n"
    '
