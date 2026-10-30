#!/bin/bash
set -euo pipefail

SRC="$(cd "$(dirname "$0")" && pwd)"
SD="${1:-}"

if [ -z "$SD" ]; then
    for candidate in "/media/$USER/GB300" "/media/$USER/SF2000" "/media/$USER/SDCARD"; do
        if [ -d "$candidate" ]; then
            SD="$candidate"
            break
        fi
    done
fi

if [ -z "$SD" ] || [ ! -d "$SD" ]; then
    echo "Usage: $0 /path/to/sd-card" >&2
    exit 1
fi

if ! mountpoint -q "$SD"; then
    echo "ERROR: $SD is not a mounted SD card. Insert the card and mount it first." >&2
    exit 1
fi

DEVICE="${DEVICE:-}"
if [ -z "$DEVICE" ]; then
    case "$SD" in
        *GB300*|*gb300*) DEVICE="GB300V2" ;;
        *SF2000*|*sf2000*) DEVICE="SF2000" ;;
        *) DEVICE="GB300V2" ;;
    esac
fi

if [ "$DEVICE" = "SF2000" ]; then
    CORE="$SRC/dist/DDLC_SF2000.sf2k"
    [ -f "$CORE" ] || CORE="$SRC/build/core_87000000_sf2000"
else
    CORE="$SRC/dist/DDLC_GB300V2.gb3k"
    [ -f "$CORE" ] || CORE="$SRC/build/core_87000000_gb300v2"
fi

if [ ! -f "$CORE" ]; then
    echo "Core not found. Run 'make gb300-core' or 'make sf2000-core' first." >&2
    exit 1
fi
if [ ! -f "$SRC/game/main.lua" ]; then
    echo "Game data not found in $SRC/game" >&2
    exit 1
fi

echo "==> Installing DDLC ($DEVICE) to $SD"

mkdir -p "$SD/system/Deimos/cores"
mkdir -p "$SD/cores/ddlc"
mkdir -p "$SD/ROMS/ddlc"
mkdir -p "$SD/configs/custom_cores/ddlc"
mkdir -p "$SD/bios"

cp -f "$CORE" "$SD/system/Deimos/cores/ddlc.sf2k"
cp -f "$CORE" "$SD/cores/ddlc/core_87000000"
cp -f "$CORE" "$SD/ROMS/ddlc/ddlc.sf2k"

echo "==> Copying game data (existing saves are kept)"
cp -a "$SRC/game/." "$SD/ROMS/ddlc/"

printf 'ddlc;ddlc;DDLC.gba' > "$SD/ROMS/ddlc/DDLC.gba"
printf 'ddlc;ddlc;DDLC.gba' > "$SD/ROMS/DDLC.gba"
printf 'DDLC.gba|ddlc\n' > "$SD/configs/custom_cores/ddlc/games.opt"
printf 'DDLC.gba|ddlc\n' > "$SD/ROMS/ddlc/games.opt"

if [ -f "$SRC/dist/DDLC.rgb565" ]; then
    cp -f "$SRC/dist/DDLC.rgb565" "$SD/ROMS/ddlc/DDLC.rgb565"
    cp -f "$SRC/dist/DDLC.rgb565" "$SD/ROMS/DDLC.rgb565"
fi
if [ -f "$SRC/dist/DDLC.png" ]; then
    cp -f "$SRC/dist/DDLC.png" "$SD/ROMS/ddlc/DDLC.png"
fi

touch "$SD/bios/enable_xlog.txt"
sync

echo "==> Done."
echo "    Core: $SD/system/Deimos/cores/ddlc.sf2k"
echo "    Game: $SD/ROMS/ddlc/DDLC.gba"
echo "    Data: $SD/ROMS/ddlc/"
echo "    Launch: FrogUI -> ROMS/ddlc/DDLC.gba"
