#!/usr/bin/env bash
# Cross-compiles WheresMyBuddy with llvm-mingw and stages dist/, laid out to
# extract straight into the Azimuth folder (the FH2ML Manager's Install mod
# takes the same zip):
#   dist/mods/wheresmybuddy/mod.json            what FH2ML reads (version from CMakeLists.txt)
#   dist/mods/wheresmybuddy/WheresMyBuddy.dll   the mod
#   dist/mods/wheresmybuddy/README.txt, LICENSE.txt
#
#   $ scripts/build.sh                          (built-in app ID)
#   $ WMB_CLIENT_ID=<azure app id> scripts/build.sh
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
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
pkg="$root/dist/mods/wheresmybuddy"
mkdir -p "$pkg"
cp "$root/build/mod.json" "$root/build/WheresMyBuddy.dll" "$pkg/"
cp "$root/scripts/dist-readme.txt" "$pkg/README.txt"
cp "$root/LICENSE" "$pkg/LICENSE.txt"
(cd "$root/dist" && find . -type f -printf '  %P\n')
