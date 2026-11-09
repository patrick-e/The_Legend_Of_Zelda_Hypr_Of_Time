#!/usr/bin/env bash
# ============================================================
# navi-say.sh — Sends a random Navi quote (PT-BR or EN)
# Uses notify-send (and optionally cowsay in terminal with --cowsay)
# ============================================================

SCRIPTS_DIR="$HOME/.config/hypr/scripts"
LANG_FILE="$SCRIPTS_DIR/.navi_lang"
NAVI_LANG="pt"

if [[ -f "$LANG_FILE" ]]; then
    NAVI_LANG="$(tr -d '[:space:]' < "$LANG_FILE" | tr '[:upper:]' '[:lower:]')"
elif [[ "${LANG:-}" != pt* ]]; then
    NAVI_LANG="en"
fi

if [[ "$NAVI_LANG" == "en" && -f "$SCRIPTS_DIR/navi_frases_en.txt" ]]; then
    QUOTES_FILE="$SCRIPTS_DIR/navi_frases_en.txt"
    DEFAULT_QUOTE="\"Hey! Listen! May the Triforce of Wisdom guide your session today!\""
else
    QUOTES_FILE="$SCRIPTS_DIR/navi_frases.txt"
    DEFAULT_QUOTE="\"Hey! Listen! Que a Triforce da Sabedoria guie a sua sessão de hoje!\""
fi

RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
STATE_FILE="$RUNTIME_DIR/navi_seen_count"

if [[ "${1:-}" == "--startup" ]]; then
    # Wait for swaync and waybar to start in the session
    for _ in {1..15}; do
        if pgrep -x swaync >/dev/null && pgrep -x waybar >/dev/null; then
            break
        fi
        sleep 1
    done
    sleep 1.5
fi

# Pick a random quote (ignoring empty lines)
if [[ -f "$QUOTES_FILE" ]]; then
    QUOTE=$(grep -v '^[[:space:]]*$' "$QUOTES_FILE" | shuf -n 1)
else
    QUOTE="$DEFAULT_QUOTE"
fi

# Sync seen count before sending so the Waybar Navi icon always blinks
CURRENT_COUNT=$(swaync-client -c -sw 2>/dev/null || echo 0)
if [[ "$CURRENT_COUNT" =~ ^[0-9]+$ ]]; then
    echo "$CURRENT_COUNT" > "$STATE_FILE"
fi

# Send notification with title "Navi"
notify-send -a "Navi" "Navi" "$QUOTE"

# Optional terminal cowsay output
if [[ "${1:-}" == "--cowsay" ]] && command -v cowsay >/dev/null 2>&1; then
    cowsay -W 55 "$QUOTE"
fi
