#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OS_NAME="$(uname -s)"
ARCH_INPUT="${1:-}"

case "$OS_NAME" in
  MINGW*|MSYS*|CYGWIN*)
    TRIPLET="x64-mingw-dynamic"
    DIST_SUBDIR="windows"
    EXECUTABLE_NAME="demo.exe"
    ;;
  Darwin)
    TRIPLET="x64-osx"
    DIST_SUBDIR="macos"
    EXECUTABLE_NAME="demo"
    ;;
  *)
    TRIPLET="x64-linux"
    DIST_SUBDIR="linux"
    EXECUTABLE_NAME="demo"
    ;;
esac

if [[ -n "${VCPKG_TARGET_TRIPLET:-}" ]]; then
  TRIPLET="$VCPKG_TARGET_TRIPLET"
fi

if [[ -n "$ARCH_INPUT" ]]; then
  if [[ "$ARCH_INPUT" == *"-"* ]]; then
    TRIPLET="$ARCH_INPUT"
  else
    case "$DIST_SUBDIR" in
      windows)
        if [[ "$ARCH_INPUT" == "x86" ]]; then
          TRIPLET="x86-mingw-dynamic"
        elif [[ "$ARCH_INPUT" == "x64" ]]; then
          TRIPLET="x64-mingw-dynamic"
        fi
        ;;
      linux)
        if [[ "$ARCH_INPUT" == "x86" ]]; then
          TRIPLET="x86-linux"
        elif [[ "$ARCH_INPUT" == "x64" ]]; then
          TRIPLET="x64-linux"
        fi
        ;;
      macos)
        if [[ "$ARCH_INPUT" == "arm64" ]]; then
          TRIPLET="arm64-osx"
        elif [[ "$ARCH_INPUT" == "x64" || "$ARCH_INPUT" == "x86_64" ]]; then
          TRIPLET="x64-osx"
        fi
        ;;
    esac
  fi
fi

ARCH_TAG="${TRIPLET%%-*}"
if [[ -z "$ARCH_TAG" ]]; then
  ARCH_TAG="x64"
fi

BUILD_DIR="$ROOT_DIR/build/release-${DIST_SUBDIR}-${TRIPLET}"
DIST_ROOT="$ROOT_DIR/dist/${DIST_SUBDIR}-${ARCH_TAG}"
PACKAGE_STAGE="$DIST_ROOT/demo-release"
PACKAGE_ARCHIVE="$DIST_ROOT/demo-${DIST_SUBDIR}-${ARCH_TAG}-release.zip"
TOOLCHAIN="$ROOT_DIR/external/vcpkg/scripts/buildsystems/vcpkg.cmake"

declare -a GENERATOR_ARGS=()
if [[ -n "${CMAKE_GENERATOR:-}" ]]; then
  GENERATOR_ARGS=(-G "${CMAKE_GENERATOR}")
elif command -v ninja >/dev/null 2>&1; then
  GENERATOR_ARGS=(-G Ninja)
fi

cmake -E remove_directory "$BUILD_DIR"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" "${GENERATOR_ARGS[@]}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DVCPKG_TARGET_TRIPLET="$TRIPLET"

cmake --build "$BUILD_DIR"

EXECUTABLE_PATH="$BUILD_DIR/$EXECUTABLE_NAME"
if [[ ! -f "$EXECUTABLE_PATH" ]]; then
  echo "ERROR: Expected executable not found at $EXECUTABLE_PATH" >&2
  exit 1
fi

cmake -E make_directory "$DIST_ROOT"
cmake -E remove_directory "$PACKAGE_STAGE"
cmake -E make_directory "$PACKAGE_STAGE"

cmake -E copy "$EXECUTABLE_PATH" "$PACKAGE_STAGE/$EXECUTABLE_NAME"
cmake -E copy_directory "$ROOT_DIR/assets" "$PACKAGE_STAGE/assets"
cmake -E copy_if_different "$ROOT_DIR/README.md" "$PACKAGE_STAGE/README.md"

find "$BUILD_DIR" -maxdepth 1 -type f \( -name "*.dll" -o -name "*.so*" -o -name "*.dylib" \) \
  -exec cmake -E copy_if_different {} "$PACKAGE_STAGE" \;

if [[ "$DIST_SUBDIR" == "windows" ]]; then
  find "$BUILD_DIR" -maxdepth 1 -type f -name "*.pdb" \
    -exec cmake -E copy_if_different {} "$PACKAGE_STAGE" \;
fi

if [[ -f "$PACKAGE_ARCHIVE" ]]; then
  rm -f "$PACKAGE_ARCHIVE"
fi

if [[ "$DIST_SUBDIR" == "windows" ]] && command -v powershell.exe >/dev/null 2>&1; then
  powershell.exe -NoLogo -NoProfile -Command \
    "Compress-Archive -Path \"${PACKAGE_STAGE//\//\\}\*\" -DestinationPath \"${PACKAGE_ARCHIVE//\//\\}\" -Force"
elif command -v zip >/dev/null 2>&1; then
  (cd "$PACKAGE_STAGE" && zip -r "$PACKAGE_ARCHIVE" .)
elif command -v python3 >/dev/null 2>&1; then
  python3 - <<PY
import os, zipfile
stage = r"$PACKAGE_STAGE"
archive = r"$PACKAGE_ARCHIVE"
with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    for root, _, files in os.walk(stage):
        for name in files:
            abs_path = os.path.join(root, name)
            rel_path = os.path.relpath(abs_path, stage)
            zf.write(abs_path, rel_path)
PY
else
  echo "ERROR: No supported zip utility found (powershell, zip, or python3)." >&2
  exit 1
fi

echo "Release package created at $PACKAGE_ARCHIVE"

