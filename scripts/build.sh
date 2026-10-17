#!/usr/bin/env bash
# Cross-compiles WheresMyBuddy with llvm-mingw and stages dist/:
#   dist/d3dcompiler_47.dll             FH2 Mod Platform (next to azimuth.exe)
#   dist/fh2-mods/WheresMyBuddy.dll     the mod
#   dist/README.txt, dist/LICENSE.txt
#
#   $ scripts/build.sh                          (built-in app ID)
#   $ WMB_CLIENT_ID=<azure app id> scripts/build.sh
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
if [ ! -f "$root/platform/CMakeLists.txt" ]; then
    git -C "$root" submodule update --init platform
fi
for d in /opt/llvm-mingw/bin /usr/local/llvm-mingw/bin; do [ -d "$d" ] && PATH=$d:$PATH; done
if [ ! -f "$root/third_party/nlohmann/nlohmann/json.hpp" ]; then
    mkdir -p "$root/third_party/nlohmann/nlohmann"
    curl -fsSL "https://raw.githubusercontent.com/nlohmann/json/${NLOHMANN_JSON_VERSION:-v3.11.3}/single_include/nlohmann/json.hpp" \
        -o "$root/third_party/nlohmann/nlohmann/json.hpp"
fi
id_arg=()
[ -n "${WMB_CLIENT_ID:-}" ] && id_arg=(-DWMB_CLIENT_ID="$WMB_CLIENT_ID")
cmake -S "$root" -B "$root/build" -DCMAKE_TOOLCHAIN_FILE="$root/cmake/mingw-w64-x86_64.cmake" \
      -DCMAKE_BUILD_TYPE=Release "${id_arg[@]}"
cmake --build "$root/build" -j "${JOBS:-$(nproc)}"
rm -rf "$root/dist"
mkdir -p "$root/dist/fh2-mods"
cp "$root/build/platform/d3dcompiler_47.dll" "$root/dist/"
cp "$root/build/WheresMyBuddy.dll" "$root/dist/fh2-mods/"
cp "$root/scripts/dist-readme.txt" "$root/dist/README.txt"
cp "$root/LICENSE" "$root/dist/LICENSE.txt"
(cd "$root/dist" && find . -type f -printf '  %P\n')
