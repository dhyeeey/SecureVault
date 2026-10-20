#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build}"
EXE="$BUILD/appSecureVault"

if [[ ! -x "$EXE" ]]; then
  echo "Build the project first (expected $EXE)" >&2
  exit 1
fi

if command -v windeployqt >/dev/null 2>&1; then
  echo "This script is for Linux. Use packaging/windows-deploy.ps1 on Windows." >&2
fi

if command -v linuxdeployqt >/dev/null 2>&1; then
  linuxdeployqt "$EXE" -qmldir="$ROOT" -appimage
  echo "AppImage created next to $EXE"
  exit 0
fi

echo "Install linuxdeployqt (or cqtdeployer) and re-run."
echo "Until then you can run from the build tree if Qt is on LD_LIBRARY_PATH:"
echo "  export LD_LIBRARY_PATH=\"\$QTDIR/lib:\${LD_LIBRARY_PATH:-}\""
echo "  $EXE"
