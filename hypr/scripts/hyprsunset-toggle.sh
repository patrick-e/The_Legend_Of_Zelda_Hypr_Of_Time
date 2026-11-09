#!/usr/bin/env bash

STATE_FILE="/tmp/hypr_shader_state"
SHADER_PATH="$HOME/.config/hypr/shaders/blue-light-filter.glsl"

if [ -f "$STATE_FILE" ] && [ "$(cat "$STATE_FILE")" = "on" ]; then
    hyprctl eval 'hl.config({ decoration = { screen_shader = "" } })'
    echo "off" > "$STATE_FILE"
    notify-send -u low -h string:x-canonical-private-synchronous:nightmode -i display "Modo Noturno" "Desativado"
else
    hyprctl eval "hl.config({ decoration = { screen_shader = '$SHADER_PATH' } })"
    echo "on" > "$STATE_FILE"
    notify-send -u low -h string:x-canonical-private-synchronous:nightmode -i display "Modo Noturno" "Ativado (~3000K)"
fi
