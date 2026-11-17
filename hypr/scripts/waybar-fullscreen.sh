#!/bin/bash

# Sincroniza o estado da Waybar com base na aba (workspace) ativa atual:
# - Se a aba ativa tiver alguma janela em fullscreen (hasfullscreen == true), fecha a Waybar.
# - Se a aba ativa NÃO tiver janela em fullscreen (hasfullscreen == false), abre a Waybar.
sync_waybar() {
    local has_fs
    has_fs=$(hyprctl activeworkspace -j 2>/dev/null | jq -r '.hasfullscreen')

    if [[ "$has_fs" == "true" ]]; then
        if pgrep -x waybar >/dev/null; then
            killall -q waybar
            for _ in {1..15}; do
                pgrep -x waybar >/dev/null || break
                sleep 0.02
            done
        fi
    elif [[ "$has_fs" == "false" ]]; then
        if ! pgrep -x waybar >/dev/null; then
            hyprctl eval 'hl.exec_cmd("waybar")' >/dev/null 2>&1
            for _ in {1..15}; do
                pgrep -x waybar >/dev/null && break
                sleep 0.02
            done
        fi
    fi
}

# Garante o estado correto ao iniciar o script
sync_waybar

# Escuta os eventos do Hyprland em tempo real (troca de aba, fullscreen, fechar/mover janela, etc.)
socat -U - "UNIX-CONNECT:$XDG_RUNTIME_DIR/hypr/$HYPRLAND_INSTANCE_SIGNATURE/.socket2.sock" | while read -r line; do
    case "$line" in
        fullscreen\>\>*|workspace\>\>*|focusedmon\>\>*|closewindow\>\>*|movewindow\>\>*|openwindow\>\>*|activespecial\>\>*)
            sync_waybar
            ;;
    esac
done
