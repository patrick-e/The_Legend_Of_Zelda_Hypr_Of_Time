#!/usr/bin/env bash
# ============================================================
# navi-say.sh — Envia uma fala aleatória da Navi em português
# Usa notify-send (e opcionalmente cowsay no terminal com --cowsay)
# ============================================================

QUOTES_FILE="$HOME/.config/hypr/scripts/navi_frases.txt"
RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
STATE_FILE="$RUNTIME_DIR/navi_seen_count"

if [[ "$1" == "--startup" ]]; then
    # Aguarda o swaync e a waybar iniciarem na sessão
    for _ in {1..15}; do
        if pgrep -x swaync >/dev/null && pgrep -x waybar >/dev/null; then
            break
        fi
        sleep 1
    done
    sleep 1.5
fi

# Escolhe uma frase aleatória (ignorando linhas vazias)
if [[ -f "$QUOTES_FILE" ]]; then
    QUOTE=$(grep -v '^[[:space:]]*$' "$QUOTES_FILE" | shuf -n 1)
else
    QUOTE="\"Hey! Listen! Que a Triforce da Sabedoria guie a sua sessão de hoje!\""
fi

# Garante que o contador visto antes do envio esteja sincronizado
# para que o ícone da Navi na Waybar sempre pisque ao receber a mensagem
CURRENT_COUNT=$(swaync-client -c -sw 2>/dev/null || echo 0)
if [[ "$CURRENT_COUNT" =~ ^[0-9]+$ ]]; then
    echo "$CURRENT_COUNT" > "$STATE_FILE"
fi

# Envia para a central de notificações com o título "Navi"
notify-send -a "Navi" "Navi" "$QUOTE"

# Se chamado no terminal com --cowsay, também imprime no cowsay
if [[ "$1" == "--cowsay" ]] && command -v cowsay >/dev/null 2>&1; then
    cowsay -W 55 "$QUOTE"
fi
