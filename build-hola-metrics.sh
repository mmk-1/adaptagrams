#!/usr/bin/env bash
# Build adaptagrams cola libraries (if needed) and the hola_metrics runner.
#
# Usage:
#   ./build-hola-metrics.sh              # install deps, build cola + hola_metrics
#   ./build-hola-metrics.sh --no-apt     # skip apt install (deps must already exist)
#   SKIP_APT=1 ./build-hola-metrics.sh   # same as --no-apt
#
set -euo pipefail

ADAPTAGRAMS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COLA_DIR="$ADAPTAGRAMS_ROOT/cola"
HOLA_SRC="$ADAPTAGRAMS_ROOT/hola_metrics.cpp"
BIN_PATH="$ADAPTAGRAMS_ROOT/hola_metrics"

APT_PACKAGES=(build-essential autoconf automake libtool pkg-config)

COLA_LIB_MARKER="$COLA_DIR/libdialect/.libs/libdialect.so"
COLA_LIBS=(
  "$COLA_DIR/libvpsc/.libs/libvpsc.so"
  "$COLA_DIR/libcola/.libs/libcola.so"
  "$COLA_DIR/libavoid/.libs/libavoid.so"
  "$COLA_LIB_MARKER"
)

SKIP_APT=0
for arg in "$@"; do
  case "$arg" in
    --no-apt) SKIP_APT=1 ;;
    -h|--help)
      sed -n '2,8p' "$0"
      exit 0
      ;;
    *)
      echo "error: unknown argument: $arg" >&2
      exit 1
      ;;
  esac
done
if [[ "${SKIP_APT:-}" == "1" ]]; then
  SKIP_APT=1
fi

log() {
  echo "==> $*"
}

nproc_safe() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
  else
    echo 2
  fi
}

apt_package_installed() {
  local pkg="$1"
  dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q 'install ok installed'
}

command_deps_ok() {
  command -v autoreconf >/dev/null 2>&1 \
    && command -v libtoolize >/dev/null 2>&1 \
    && command -v g++ >/dev/null 2>&1 \
    && command -v make >/dev/null 2>&1
}

ensure_apt_packages() {
  if [[ "$SKIP_APT" == "1" ]]; then
    return 0
  fi
  if ! command -v apt-get >/dev/null 2>&1; then
    return 0
  fi

  local missing=()
  for pkg in "${APT_PACKAGES[@]}"; do
    if ! apt_package_installed "$pkg"; then
      missing+=("$pkg")
    fi
  done

  if [[ ${#missing[@]} -eq 0 ]]; then
    log "apt packages already installed"
    return 0
  fi

  log "installing apt packages: ${missing[*]}"
  if [[ "$(id -u)" -eq 0 ]]; then
    apt-get update -qq
    DEBIAN_FRONTEND=noninteractive apt-get install -y "${missing[@]}"
  elif command -v sudo >/dev/null 2>&1; then
    sudo apt-get update -qq
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y "${missing[@]}"
  else
    echo "error: missing packages (${missing[*]}) and neither root nor sudo available." >&2
    echo "Run: apt-get install -y ${missing[*]}" >&2
    exit 1
  fi
}

ensure_build_deps() {
  ensure_apt_packages
  if command_deps_ok; then
    log "build tools present"
    return 0
  fi
  echo "error: missing build tools after apt install attempt." >&2
  echo "Required: g++, make, autoreconf, libtoolize" >&2
  echo "On Debian/Ubuntu: apt-get install -y ${APT_PACKAGES[*]}" >&2
  exit 1
}

cola_libs_built() {
  local lib
  for lib in "${COLA_LIBS[@]}"; do
    if [[ ! -f "$lib" ]]; then
      return 1
    fi
  done
  return 0
}

cola_configured() {
  [[ -f "$COLA_DIR/configure" && -f "$COLA_DIR/Makefile" ]]
}

configure_cola() {
  cd "$COLA_DIR"
  if [[ ! -f configure ]]; then
    log "running autoreconf in cola/"
    mkdir -p m4
    autoreconf --install --verbose
  fi
  if [[ ! -f Makefile ]]; then
    log "running ./configure in cola/"
    ./configure
  fi
}

build_cola() {
  ensure_build_deps
  configure_cola
  log "building cola libraries (may take a few minutes on first run)"
  make -C "$COLA_DIR" -j"$(nproc_safe)"
}

ensure_cola() {
  if cola_libs_built; then
    log "cola libraries already built — skipping"
    return 0
  fi
  build_cola
  if ! cola_libs_built; then
    echo "error: cola build finished but libraries are still missing." >&2
    exit 1
  fi
}

newest_cola_lib_time() {
  local newest=0
  local lib mtime
  for lib in "${COLA_LIBS[@]}"; do
    if [[ -f "$lib" ]]; then
      mtime=$(stat -c '%Y' "$lib")
      if (( mtime > newest )); then
        newest=$mtime
      fi
    fi
  done
  echo "$newest"
}

hola_needs_rebuild() {
  if [[ ! -x "$BIN_PATH" && ! -f "$BIN_PATH" ]]; then
    return 0
  fi
  if [[ "$HOLA_SRC" -nt "$BIN_PATH" ]]; then
    return 0
  fi
  local bin_time cola_time
  bin_time=$(stat -c '%Y' "$BIN_PATH")
  cola_time=$(newest_cola_lib_time)
  if (( cola_time > bin_time )); then
    return 0
  fi
  return 1
}

build_hola() {
  log "linking hola_metrics"
  make -C "$ADAPTAGRAMS_ROOT" hola
}

ensure_hola() {
  if hola_needs_rebuild; then
    build_hola
  else
    log "hola_metrics already up to date — skipping"
  fi
}

main() {
  log "ADAPTAGRAMS_ROOT: $ADAPTAGRAMS_ROOT"
  ensure_cola
  ensure_hola
  log "done: $BIN_PATH"
}

main "$@"
