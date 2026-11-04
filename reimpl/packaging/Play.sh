#!/bin/sh
set -eu
app=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$app/stellar7" --play "$@"
