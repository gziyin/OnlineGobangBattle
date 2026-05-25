#!/usr/bin/env bash
set -euo pipefail

TARGET_RELEASE="${1:-}"
APP_ROOT="${APP_ROOT:-/srv/online-gobang}"
RELEASES_DIR="$APP_ROOT/releases"
CURRENT_LINK="$APP_ROOT/current"

if [[ ! -d "$RELEASES_DIR" ]]; then
  echo "[rollback] ERROR: releases directory not found: $RELEASES_DIR"
  exit 1
fi

if [[ -n "$TARGET_RELEASE" ]]; then
  TARGET_DIR="$RELEASES_DIR/$TARGET_RELEASE"
  if [[ ! -d "$TARGET_DIR" ]]; then
    echo "[rollback] ERROR: target release not found: $TARGET_DIR"
    exit 1
  fi
else
  mapfile -t RELEASES < <(ls -1dt "$RELEASES_DIR"/* 2>/dev/null || true)
  if (( ${#RELEASES[@]} < 2 )); then
    echo "[rollback] ERROR: at least 2 releases are required for auto rollback"
    exit 1
  fi
  TARGET_DIR="${RELEASES[1]}"
fi

ln -sfn "$TARGET_DIR" "$CURRENT_LINK"

echo "[rollback] switched current -> $TARGET_DIR"
echo "[rollback] current -> $(readlink -f "$CURRENT_LINK")"
