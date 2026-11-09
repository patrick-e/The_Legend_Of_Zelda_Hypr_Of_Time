
#!/bin/bash

# Garante que o waybar esteja rodando ao iniciar o script
if ! pgrep -x waybar >/dev/null; then
    hyprctl eval 'hl.exec_cmd("waybar")'
fi

# Escuta a tomada de eventos do Hyprland em tempo real
socat -U - UNIX-CONNECT:$XDG_RUNTIME_DIR/hypr/$HYPRLAND_INSTANCE_SIGNATURE/.socket2.sock | while read -r line; do
    # Se a janela entrar em fullscreen
    if [[ "$line" == "fullscreen>>1" ]]; then
        killall waybar
    # Se a janela sair do fullscreen
    elif [[ "$line" == "fullscreen>>0" ]]; then
        if ! pgrep -x waybar >/dev/null; then
            hyprctl eval 'hl.exec_cmd("waybar")'
        fi
    fi
done
