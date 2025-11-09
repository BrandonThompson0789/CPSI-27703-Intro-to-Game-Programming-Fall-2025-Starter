#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

print_header() {
  echo "========================================"
  echo " SDL2 + Box2D + tinyxml2 Linux Setup"
  echo "========================================"
  echo
}

abort() {
  echo "[ERROR] $*" >&2
  exit 1
}

need_root_helper() {
  if [[ "$EUID" -eq 0 ]]; then
    "$@"
  else
    if command -v sudo >/dev/null 2>&1; then
      sudo "$@"
    else
      abort "This operation requires elevated privileges and sudo was not found."
    fi
  fi
}

detect_package_manager() {
  if command -v apt-get >/dev/null 2>&1; then
    echo "apt-get"
  elif command -v dnf >/dev/null 2>&1; then
    echo "dnf"
  elif command -v yum >/dev/null 2>&1; then
    echo "yum"
  elif command -v zypper >/dev/null 2>&1; then
    echo "zypper"
  elif command -v pacman >/dev/null 2>&1; then
    echo "pacman"
  elif command -v apk >/dev/null 2>&1; then
    echo "apk"
  elif command -v brew >/dev/null 2>&1; then
    echo "brew"
  else
    echo ""
  fi
}

install_dependencies() {
  local manager="$1"
  echo "[INFO] Installing development dependencies via $manager..."

  case "$manager" in
    apt-get)
      need_root_helper apt-get update
      need_root_helper apt-get install -y \
        build-essential git cmake ninja-build clang lld pkg-config zip unzip curl python3
      ;;
    dnf)
      need_root_helper dnf install -y \
        @development-tools git cmake ninja-build clang lld gcc-c++ make pkgconfig zip unzip curl python3
      ;;
    yum)
      need_root_helper yum groupinstall -y "Development Tools"
      need_root_helper yum install -y \
        git cmake ninja-build clang lld gcc-c++ make pkgconfig zip unzip curl python3
      ;;
    zypper)
      need_root_helper zypper install -y \
        git cmake ninja clang lld gcc gcc-c++ make pkg-config zip unzip curl python3
      ;;
    pacman)
      need_root_helper pacman -Syu --needed --noconfirm \
        base-devel git cmake ninja clang lld pkgconf zip unzip curl python
      ;;
    apk)
      need_root_helper apk add --no-cache \
        build-base git cmake ninja clang lld pkgconf zip unzip curl python3
      ;;
    brew)
      brew update
      brew install git cmake ninja llvm pkg-config zip unzip curl python
      ;;
    *)
      abort "Could not identify a supported package manager. Please install git, CMake, Ninja, clang (or gcc), make, pkg-config, curl, zip, unzip, and Python 3 manually."
      ;;
  esac
}

ensure_dependencies() {
  local required_commands=(
    git cmake ninja curl pkg-config
  )

  local have_all=true
  for cmd in "${required_commands[@]}"; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
      have_all=false
      break
    fi
  done

  local cxx_compiler=""
  if command -v clang++ >/dev/null 2>&1; then
    cxx_compiler="clang++"
  elif command -v g++ >/dev/null 2>&1; then
    cxx_compiler="g++"
  else
    have_all=false
  fi

  if ! "$have_all"; then
    local manager
    manager="$(detect_package_manager)"
    if [[ -z "$manager" ]]; then
      abort "Missing required build tools and package manager could not be detected."
    fi
    install_dependencies "$manager"
  fi

  for cmd in "${required_commands[@]}"; do
    command -v "$cmd" >/dev/null 2>&1 || abort "Command '$cmd' is still missing after installation."
  done

  if command -v clang >/dev/null 2>&1 && command -v clang++ >/dev/null 2>&1; then
    C_COMPILER="clang"
    CXX_COMPILER="clang++"
  elif command -v gcc >/dev/null 2>&1 && command -v g++ >/dev/null 2>&1; then
    C_COMPILER="gcc"
    CXX_COMPILER="g++"
  else
    abort "Neither clang++ nor g++ is available."
  fi
}

ensure_vcpkg_submodule() {
  if [[ ! -d "$ROOT_DIR/external/vcpkg/.git" ]]; then
    echo "[INFO] Initializing vcpkg submodule..."
    git submodule update --init --recursive external/vcpkg
  else
    echo "[SUCCESS] vcpkg submodule already initialized."
  fi
}

bootstrap_vcpkg() {
  local vcpkg_bin="$ROOT_DIR/external/vcpkg/vcpkg"
  if [[ -x "$vcpkg_bin" ]]; then
    echo "[SUCCESS] vcpkg already bootstrapped."
    return
  fi

  echo "[INFO] Bootstrapping vcpkg..."
  (cd "$ROOT_DIR/external/vcpkg" && ./bootstrap-vcpkg.sh -disableMetrics)

  if [[ ! -x "$vcpkg_bin" ]]; then
    abort "Failed to bootstrap vcpkg."
  fi
}

detect_triplet() {
  local machine
  machine="$(uname -m)"
  case "$machine" in
    x86_64|amd64)
      echo "x64-linux"
      ;;
    aarch64|arm64)
      echo "arm64-linux"
      ;;
    armv7l|armv8l|armv6l)
      echo "arm-neon-linux"
      ;;
    *)
      echo "x64-linux"
      echo "[WARNING] Unrecognized architecture '$machine'. Defaulting to x64-linux." >&2
      ;;
  esac
}

configure_and_build() {
  local triplet="$1"
  local configure_args=(
    --preset linux-clang-debug
    "-DVCPKG_TARGET_TRIPLET=$triplet"
  )

  if [[ "$C_COMPILER" == "clang" ]]; then
    configure_args+=("-DCMAKE_C_COMPILER=clang" "-DCMAKE_CXX_COMPILER=clang++")
  elif [[ "$C_COMPILER" == "gcc" ]]; then
    configure_args+=("-DCMAKE_C_COMPILER=gcc" "-DCMAKE_CXX_COMPILER=g++")
  fi

  echo "[INFO] Configuring project with CMake..."
  cmake "${configure_args[@]}"

  echo "[INFO] Building project..."
  cmake --build "$ROOT_DIR/build/linux-debug"

  local executable="$ROOT_DIR/build/linux-debug/demo"
  if [[ ! -x "$executable" ]]; then
    abort "Expected executable not found at $executable"
  fi
}

print_summary() {
  local triplet="$1"
  cat <<EOF
[SUCCESS] Setup completed successfully!

Next steps:
  - Open VS Code and use "File > Open Folder..." on this repository.
  - Select the "Linux Clang + Ninja (Debug)" CMake preset if prompted.
  - Press F5 to build and debug, or use "Run > Start Debugging".

Artifacts:
  - Build directory: $ROOT_DIR/build/linux-debug
  - Executable:      $ROOT_DIR/build/linux-debug/demo
  - vcpkg triplet:   $triplet
EOF
}

print_header

if [[ ! -f "$ROOT_DIR/CMakeLists.txt" ]]; then
  abort "CMakeLists.txt not found. Please run this script from the project root."
fi

cd "$ROOT_DIR"

echo "[INFO] Checking prerequisites..."
ensure_dependencies
ensure_vcpkg_submodule
bootstrap_vcpkg

TRIPLET="${VCPKG_TARGET_TRIPLET:-$(detect_triplet)}"
echo "[INFO] Using vcpkg triplet: $TRIPLET"

configure_and_build "$TRIPLET"
print_summary "$TRIPLET"

