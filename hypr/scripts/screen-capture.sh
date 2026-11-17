#!/usr/bin/env bash
# Script unificado de Captura de Tela (Print) e Gravação de Vídeo para Hyprland

export PATH="$HOME/.local/bin:$PATH"

SHOT_DIR="$HOME/Imagens/Capturas de tela"
REC_DIR="$HOME/Vídeos/Gravações"
STATE_FILE="/tmp/hypr_recording_file"
CSS_FILE="$HOME/.config/hypr/scripts/capture-menu.css"

mkdir -p "$SHOT_DIR" "$REC_DIR"

WF_RECORDER="$(command -v wf-recorder)"

is_recording() {
    pgrep -x "wf-recorder" >/dev/null 2>&1
}

stop_recording() {
    if is_recording; then
        pkill -INT -x "wf-recorder"
        # Aguarda o wf-recorder finalizar o arquivo MP4
        for _ in {1..30}; do
            is_recording || break
            sleep 0.1
        done

        SAVED_FILE=""
        if [ -f "$STATE_FILE" ]; then
            SAVED_FILE="$(cat "$STATE_FILE")"
            rm -f "$STATE_FILE"
        fi

        if [ -n "$SAVED_FILE" ] && [ -f "$SAVED_FILE" ]; then
            if command -v wl-copy >/dev/null 2>&1; then
                printf "file://%s\r\n" "$SAVED_FILE" | wl-copy --type text/uri-list
            fi
            notify-send -u normal -i video-x-generic "Gravação Finalizada" "Salvo em: $(basename "$SAVED_FILE")\n(Copiado para a área de transferência)"
        else
            notify-send -u normal -i video-x-generic "Gravação Finalizada" "Vídeo salvo em ~/Vídeos/Gravações"
        fi
        exit 0
    fi
}

# Arredonda largura e altura do slurp para números pares (necessário para h264 yuv420p)
select_even_geometry() {
    local raw_geom
    raw_geom="$(slurp)" || return 1
    [ -z "$raw_geom" ] && return 1

    local pos size x y w h
    pos="${raw_geom% *}"
    size="${raw_geom#* }"
    x="${pos%,*}"
    y="${pos#*,}"
    w="${size%x*}"
    h="${size#*x}"

    w=$(( w - (w % 2) ))
    h=$(( h - (h % 2) ))
    [ "$w" -lt 2 ] && w=2
    [ "$h" -lt 2 ] && h=2

    echo "${x},${y} ${w}x${h}"
}

start_recording() {
    local mode="$1"       # region | output
    local audio_mode="$2" # desktop | mic | none

    if [ -z "$WF_RECORDER" ]; then
        notify-send -u critical "Erro na Gravação" "wf-recorder não encontrado!"
        exit 1
    fi

    local geom_args=()
    if [ "$mode" = "region" ]; then
        local geom
        geom="$(select_even_geometry)" || exit 0
        geom_args=(-g "$geom")
    fi

    local audio_args=()
    case "$audio_mode" in
        desktop)
            local sink_monitor
            sink_monitor="$(pactl get-default-sink).monitor"
            audio_args=(--audio="$sink_monitor")
            ;;
        mic)
            local default_mic
            default_mic="$(pactl get-default-source)"
            audio_args=(--audio="$default_mic")
            ;;
        none)
            audio_args=()
            ;;
    esac

    local timestamp
    timestamp="$(date +'%Y-%m-%d_%H-%M-%S')"
    local out_file="$REC_DIR/gravacao_${timestamp}.mp4"
    echo "$out_file" > "$STATE_FILE"

    local audio_label="Áudio do Sistema"
    [ "$audio_mode" = "mic" ] && audio_label="Microfone"
    [ "$audio_mode" = "none" ] && audio_label="Sem Áudio"

    notify-send -u low -t 2500 -i media-record "Gravação Iniciada ($audio_label)" "Pressione Ctrl+Print ou Super+Print para parar."

    "$WF_RECORDER" "${geom_args[@]}" "${audio_args[@]}" \
        -c libx264 -p preset=ultrafast -p crf=21 -x yuv420p \
        -y -f "$out_file" >/tmp/wf-recorder.log 2>&1 &
}

show_menu() {
    # Fecha o wofi se já estiver aberto
    if pgrep -x "wofi" >/dev/null 2>&1; then
        pkill -x "wofi"
        exit 0
    fi

    local options=""
    local height=360

    if is_recording; then
        options+="⏹️  Parar Gravação em Andamento\n"
        height=410
    fi

    options+="📸  Print — Selecionar Área (Print)\n"
    options+="🖥️  Print — Tela Inteira (Shift+Print)\n"
    options+="🪟  Print — Janela Ativa\n"
    options+="🎥  Gravar Área — Áudio do Sistema (Ctrl+Print)\n"
    options+="🎬  Gravar Tela Inteira — Áudio do Sistema (Ctrl+Shift+Print)\n"
    options+="🎙️  Gravar Área — Microfone\n"
    options+="🔇  Gravar Área — Sem Áudio"

    local choice
    choice="$(printf "%b" "$options" | wofi --dmenu \
        --style "$CSS_FILE" \
        --width 500 \
        --height "$height" \
        --location center \
        --prompt "Capturar ou Gravar..." \
        --insensitive \
        --hide-scroll \
        --no-actions)"

    [ -z "$choice" ] && exit 0

    # Aguarda o menu fechar antes de capturar
    sleep 0.15

    case "$choice" in
        *"Parar Gravação"*)
            stop_recording
            ;;
        *"Print — Selecionar Área"*)
            hyprshot -m region -o "$SHOT_DIR"
            ;;
        *"Print — Tela Inteira"*)
            hyprshot -m output -o "$SHOT_DIR"
            ;;
        *"Print — Janela Ativa"*)
            hyprshot -m window -o "$SHOT_DIR"
            ;;
        *"Gravar Área — Áudio do Sistema"*)
            stop_recording
            start_recording region desktop
            ;;
        *"Gravar Tela Inteira — Áudio do Sistema"*)
            stop_recording
            start_recording output desktop
            ;;
        *"Gravar Área — Microfone"*)
            stop_recording
            start_recording region mic
            ;;
        *"Gravar Área — Sem Áudio"*)
            stop_recording
            start_recording region none
            ;;
    esac
}

case "${1:-menu}" in
    menu)
        show_menu
        ;;
    shot-region)
        hyprshot -m region -o "$SHOT_DIR"
        ;;
    shot-output)
        hyprshot -m output -o "$SHOT_DIR"
        ;;
    shot-window)
        hyprshot -m window -o "$SHOT_DIR"
        ;;
    record-region)
        stop_recording
        start_recording region desktop
        ;;
    record-output)
        stop_recording
        start_recording output desktop
        ;;
    record-region-mic)
        stop_recording
        start_recording region mic
        ;;
    record-region-noaudio)
        stop_recording
        start_recording region none
        ;;
    stop)
        stop_recording
        ;;
    *)
        echo "Uso: $0 {menu|shot-region|shot-output|shot-window|record-region|record-output|record-region-mic|record-region-noaudio|stop}"
        exit 1
        ;;
esac
