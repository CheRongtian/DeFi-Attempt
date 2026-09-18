#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SOURCE_DIR="$PROJECT_ROOT/tools/metal-option-pricer"
BUILD_DIR="$PROJECT_ROOT/build/metal-option-pricer"
HOST=${DLP_OPTION_PRICER_HOST:-127.0.0.1}
PORT=${DLP_OPTION_PRICER_PORT:-18081}
MODE=${1:-run}

if [[ $MODE == run || $MODE == build ]]
then
    cmake -S "$SOURCE_DIR" -B "$BUILD_DIR"
    cmake --build "$BUILD_DIR" --parallel
fi

if [[ $MODE == build ]]
then
    exit 0
fi

if [[ $MODE != run && $MODE != serve ]]
then
    printf 'Usage: %s [run|build|serve]\n' "$0" >&2
    exit 2
fi

exec "$BUILD_DIR/dlp_metal_option_pricer" serve "$HOST" "$PORT"
