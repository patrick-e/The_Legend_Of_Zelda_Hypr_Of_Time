#!/usr/bin/env bash

LANG_FILE="$HOME/.config/hypr/scripts/.navi_lang"
SUN_LANG="pt"
if [[ -f "$LANG_FILE" ]]; then
    SUN_LANG="$(tr -d '[:space:]' < "$LANG_FILE" | tr '[:upper:]' '[:lower:]')"
elif [[ "${LANG:-}" != pt* ]]; then
    SUN_LANG="en"
fi

hour=$(date +%-H)
time_str=$(date +%H:%M)

if (( hour >= 5 && hour <= 7 )); then
    cls="dawn"
    if [[ "$SUN_LANG" == "en" ]]; then
        label="🌅 Dawn (Half Sun / Half Moon) — $time_str"
    else
        label="🌅 Amanhecer (Meio Sol / Meia Lua) — $time_str"
    fi
elif (( hour >= 8 && hour <= 10 )); then
    cls="morning"
    if [[ "$SUN_LANG" == "en" ]]; then
        label="🌤️ Morning (Rising Sun) — $time_str"
    else
        label="🌤️ Manhã (Sol Crescente) — $time_str"
    fi
elif (( hour >= 11 && hour <= 14 )); then
    cls="day"
    if [[ "$SUN_LANG" == "en" ]]; then
        label="☀️ Midday (Full Sun) — $time_str"
    else
        label="☀️ Dia Pleno (Sol) — $time_str"
    fi
elif (( hour >= 15 && hour <= 16 )); then
    cls="afternoon"
    if [[ "$SUN_LANG" == "en" ]]; then
        label="⛅ Afternoon (Setting Sun) — $time_str"
    else
        label="⛅ Tarde (Sol Poente) — $time_str"
    fi
elif (( hour >= 17 && hour <= 19 )); then
    cls="dusk"
    if [[ "$SUN_LANG" == "en" ]]; then
        label="🌇 Dusk (Half Moon / Half Sun) — $time_str"
    else
        label="🌇 Entardecer (Meia Lua / Meio Sol) — $time_str"
    fi
else
    cls="night"
    if [[ "$SUN_LANG" == "en" ]]; then
        label="🌙 Night (Moon) — $time_str"
    else
        label="🌙 Noite (Lua) — $time_str"
    fi
fi

printf '{"text": " ", "alt": "%s", "class": "%s", "tooltip": "%s"}\n' "$cls" "$cls" "$label"
