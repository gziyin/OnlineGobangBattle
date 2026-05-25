#!/usr/bin/env bash
set -euo pipefail

GIT_REF="${1:-main}"
APP_ROOT="${APP_ROOT:-/srv/online-gobang}"
REPO_DIR="${REPO_DIR:-$APP_ROOT/repo}"
RELEASES_DIR="$APP_ROOT/releases"
CURRENT_LINK="$APP_ROOT/current"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
RELEASE_DIR="$RELEASES_DIR/$TIMESTAMP"

echo "[deploy] ref=$GIT_REF"
echo "[deploy] app_root=$APP_ROOT"

mkdir -p "$RELEASES_DIR"

if [[ ! -d "$REPO_DIR/.git" ]]; then
  echo "[deploy] ERROR: REPO_DIR is not a git repository: $REPO_DIR"
  echo "[deploy] hint: export REPO_DIR=/path/to/your/repo"
  exit 1
fi

echo "[deploy] update repository"
git -C "$REPO_DIR" fetch --all --tags
git -C "$REPO_DIR" checkout "$GIT_REF"
git -C "$REPO_DIR" pull --ff-only origin "$GIT_REF" || true

echo "[deploy] create release from repo working tree"
mkdir -p "$RELEASE_DIR"
rsync -a --delete \
  --exclude '.git' \
  --exclude 'source/build' \
  --exclude 'source/logs' \
  --exclude 'logs' \
  "$REPO_DIR/" "$RELEASE_DIR/"

echo "[deploy] build and test"
cmake -S "$RELEASE_DIR/source" -B "$RELEASE_DIR/source/build"
cmake --build "$RELEASE_DIR/source/build" -j
ctest --test-dir "$RELEASE_DIR/source/build" --output-on-failure

echo "[deploy] switch current symlink"
ln -sfn "$RELEASE_DIR" "$CURRENT_LINK"

echo "[deploy] done: $RELEASE_DIR"
echo "[deploy] current -> $(readlink -f "$CURRENT_LINK")"
