#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}"

echo "============================================================"
echo "  The Legend of Zelda: Hypr of Time — Installer"
echo "============================================================"

# 1. Create target directories
mkdir -p \
    "$CONFIG_DIR/hypr/assets" \
    "$CONFIG_DIR/hypr/plugins" \
    "$CONFIG_DIR/hypr/scripts" \
    "$CONFIG_DIR/wofi/assets" \
    "$CONFIG_DIR/swaync/icons" \
    "$CONFIG_DIR/waybar/icons" \
    "$CONFIG_DIR/waybar/scripts"

# 2. Copy Hyprland assets, scripts, and configs (backing up existing ones)
echo "[1/5] Installing Hyprland Zelda border assets & scripts..."
cp -f "$REPO_DIR/hypr/assets/"*.png "$CONFIG_DIR/hypr/assets/"
cp -f "$REPO_DIR/hypr/scripts/"* "$CONFIG_DIR/hypr/scripts/"
chmod +x "$CONFIG_DIR/hypr/scripts/"*.sh

for cfg in hyprland.lua hyprland.conf; do
    if [ -f "$CONFIG_DIR/hypr/$cfg" ] && [ ! -f "$CONFIG_DIR/hypr/${cfg}.pre-zelda.bak" ]; then
        cp "$CONFIG_DIR/hypr/$cfg" "$CONFIG_DIR/hypr/${cfg}.pre-zelda.bak"
    fi
    sed "s|__HOME__|$HOME|g" "$REPO_DIR/hypr/$cfg" > "$CONFIG_DIR/hypr/$cfg"
done

# 3. Compile patched imgborders Hyprland plugin
echo "[2/5] Compiling patched Hyprland imgborders plugin..."
if pkg-config --exists hyprland; then
    g++ -shared -fPIC --no-gnu-unique -O2 -std=c++23 \
        "$REPO_DIR/hypr/plugins/imgborders/main.cpp" \
        "$REPO_DIR/hypr/plugins/imgborders/ImgBorder.cpp" \
        $(pkg-config --cflags --libs hyprland libdrm pixman-1 pangocairo) \
        -o "$CONFIG_DIR/hypr/plugins/libimgborders.so"
    echo "      -> Built $CONFIG_DIR/hypr/plugins/libimgborders.so"
else
    echo "      [!] Warning: hyprland headers not found via pkg-config; skipping libimgborders.so build."
fi

# 4. Install Wofi Ocarina of Time 3D theme & compile hover/scroll hook
echo "[3/5] Installing Wofi Ocarina of Time 3D launcher theme..."
cp -f "$REPO_DIR/wofi/assets/"*.png "$CONFIG_DIR/wofi/assets/"
cp -f "$REPO_DIR/wofi/libwofi_hover.c" "$CONFIG_DIR/wofi/libwofi_hover.c"
sed "s|__HOME__|$HOME|g" "$REPO_DIR/wofi/config" > "$CONFIG_DIR/wofi/config"
sed "s|__HOME__|$HOME|g" "$REPO_DIR/wofi/style.css" > "$CONFIG_DIR/wofi/style.css"

if pkg-config --exists gtk+-3.0; then
    gcc -shared -fPIC -O2 "$CONFIG_DIR/wofi/libwofi_hover.c" \
        -o "$CONFIG_DIR/wofi/libwofi_hover.so" \
        $(pkg-config --cflags --libs gtk+-3.0) -ldl
    echo "      -> Built $CONFIG_DIR/wofi/libwofi_hover.so"
else
    echo "      [!] Warning: gtk+-3.0 headers not found; skipping libwofi_hover.so build."
fi

# 5. Install SwayNC & Waybar Navi Notification System
echo "[4/5] Installing SwayNC Navi Dialogue notification theme..."
cp -f "$REPO_DIR/swaync/icons/"* "$CONFIG_DIR/swaync/icons/"
sed "s|__HOME__|$HOME|g" "$REPO_DIR/swaync/config.json" > "$CONFIG_DIR/swaync/config.json"
sed "s|__HOME__|$HOME|g" "$REPO_DIR/swaync/style.css" > "$CONFIG_DIR/swaync/style.css"

echo "[5/5] Installing Waybar Navi status indicator..."
cp -f "$REPO_DIR/waybar/icons/"* "$CONFIG_DIR/waybar/icons/"
cp -f "$REPO_DIR/waybar/scripts/"* "$CONFIG_DIR/waybar/scripts/"
chmod +x "$CONFIG_DIR/waybar/scripts/"*.sh
sed "s|__HOME__|$HOME|g" "$REPO_DIR/waybar/config.jsonc" > "$CONFIG_DIR/waybar/config.jsonc"
sed "s|__HOME__|$HOME|g" "$REPO_DIR/waybar/style.css" > "$CONFIG_DIR/waybar/style.css"

echo ""
echo "Done! Reloading services..."
hyprctl reload >/dev/null 2>&1 || true
swaync-client -R >/dev/null 2>&1 || true
swaync-client -rs >/dev/null 2>&1 || true
echo "✨ Hypr of Time installed successfully!"
