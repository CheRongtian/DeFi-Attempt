#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
FRONTEND_DIR="$PROJECT_ROOT/frontend"
ENV_FILE=${1:-"$PROJECT_ROOT/.env.local"}
HOST=${DLP_FRONTEND_HOST:-127.0.0.1}
PORT=${DLP_FRONTEND_PORT:-4173}

"$PROJECT_ROOT/scripts/configure-frontend.sh" "$ENV_FILE"

cd "$FRONTEND_DIR"
npm run build

printf 'Frontend available at http://%s:%s\n' "$HOST" "$PORT"
exec "$FRONTEND_DIR/node_modules/.bin/vite" preview --host "$HOST" --port "$PORT"
