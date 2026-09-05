#!/usr/bin/env bash
set -euo pipefail
prototype_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
prototype_build="$prototype_root/build-macro-motion-prototype"
cmake -S "$prototype_root" -B "$prototype_build" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DLUMA_BUILD_MOTION_PROTOTYPE=ON -DLUMA_BUILD_PREVIEW=OFF -DBUILD_TESTING=OFF
cmake --build "$prototype_build" --target luma-motion-prototype -j 4
exec "$prototype_build/tools/macro-motion-prototype/luma-motion-prototype" "$@"
