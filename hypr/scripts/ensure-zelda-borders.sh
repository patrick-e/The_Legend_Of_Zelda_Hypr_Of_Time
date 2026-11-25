#!/usr/bin/env bash
# Auto-recompila o plugin libzelda_borders.so caso o Hyprland / hyprgraphics tenha sido atualizado
PLUGIN_SO="$HOME/.config/hypr/plugins/libzelda_borders.so"
SRC_DIR="$HOME/.config/hypr/plugins/imgborders"

if hyprctl plugin list 2>/dev/null | grep -q "imgborders"; then
    exit 0
fi

if [ -d "$SRC_DIR" ] && pkg-config --exists hyprland; then
    g++ -shared -fPIC --no-gnu-unique -O2 -std=c++23 -DVERSION=\"3.0.0-zelda\" \
        "$SRC_DIR/main.cpp" \
        "$SRC_DIR/ImgBorder.cpp" \
        "$SRC_DIR/ImgBorderPassElement.cpp" \
        "$SRC_DIR/ImgUtils.cpp" \
        $(pkg-config --cflags --libs hyprland libdrm pixman-1 pangocairo) \
        -o "${PLUGIN_SO}.tmp" && \
    cp -f "${PLUGIN_SO}.tmp" "$HOME/.config/hypr/plugins/libimgborders.so" && \
    mv -f "${PLUGIN_SO}.tmp" "$PLUGIN_SO" && \
    hyprctl plugin load "$PLUGIN_SO" >/dev/null 2>&1 && \
    hyprctl reload >/dev/null 2>&1
fi
