#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SQL_FILE="$SCRIPT_DIR/init_db.sql"
CONF_FILE="$PROJECT_ROOT/source/config/server.conf"

# --- defaults (overridable via env) ---
DB_HOST="${DB_HOST:-127.0.0.1}"
DB_PORT="${DB_PORT:-3306}"
DB_USER="${DB_USER:-root}"
DB_PASS="${DB_PASS:-}"

# --- parse server.conf if it exists ---
if [[ -f "$CONF_FILE" ]]; then
    while IFS='=' read -r key val; do
        key="$(echo "$key" | xargs)"
        val="$(echo "$val" | xargs | sed 's/^"//;s/"$//')"
        case "$key" in
            db_host)     DB_HOST="${DB_HOST:-$val}" ;;
            db_port)     DB_PORT="${DB_PORT:-$val}" ;;
            db_user)     DB_USER="${DB_USER:-$val}" ;;
            db_password) DB_PASS="${DB_PASS:-$val}" ;;
        esac
    done < "$CONF_FILE"
fi

# --- preflight checks ---
if ! command -v mysql &>/dev/null; then
    echo "[init_db] ERROR: mysql client not found. Install mysql or mysql-client."
    exit 1
fi

if ! mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" ${DB_PASS:+-p"$DB_PASS"} -e "SELECT 1" &>/dev/null; then
    echo "[init_db] ERROR: cannot connect to MySQL at $DB_HOST:$DB_PORT as $DB_USER"
    echo "[init_db] Check that MySQL is running and credentials in server.conf are correct."
    exit 1
fi

# --- execute ---
echo "[init_db] Connecting to $DB_HOST:$DB_PORT as $DB_USER ..."
mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" ${DB_PASS:+-p"$DB_PASS"} < "$SQL_FILE"
echo "[init_db] Database initialized successfully."
