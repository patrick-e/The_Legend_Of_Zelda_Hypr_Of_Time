#!/usr/bin/env python3
"""
Waybar Sound Extension — Zelda Ocarina Tapestry Theme
- Abre como uma extensão real da Waybar ancorada no topo direito (via GtkLayerShell, não como janela comum).
- Segue exatamente o tema de tapeçaria da Waybar (tapestry-bg.png + tapestry-border.png).
- Mantém os 13 ícones originais do arco de Zelda (com fundo transparente sobre a tapeçaria e opções em cima no hover).
- Cifra em 3 camadas: Papel -> Wallpaper Semitransparente (Capa do Spotify se estiver tocando, ou Ícone REAL do App Aberto com MAIS SOM) -> Partitura (Clave de Sol original + 5 linhas).
- Carrega os ícones REAIS dos aplicativos abertos no sistema (.desktop / Gtk.IconTheme).
"""

import base64
import glob
import io
import json
import os
import shutil
import subprocess
import threading
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer
from PIL import Image, ImageDraw, ImageFilter

import gi

gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
gi.require_version("GtkLayerShell", "0.1")
gi.require_version("WebKit2", "4.1")
from gi.repository import Gtk, Gdk, GtkLayerShell, WebKit2  # noqa: E402

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ASSETS_PATH = os.path.join(SCRIPT_DIR, "zelda_sound_assets", "assets.json")
PID_FILE = "/tmp/zelda_pavucontrol.pid"

with open(ASSETS_PATH, "r", encoding="utf-8") as f:
    ASSETS = json.load(f)

try:
    _ov_im = Image.open(io.BytesIO(base64.b64decode(ASSETS["overlay"]))).convert("RGBA")
    _ov_px = _ov_im.load()
    for _y in range(70, 92):
        for _x in list(range(14, 28)) + list(range(244, 260)):
            _ov_px[_x, _y] = (0, 0, 0, 0)
    _buf = io.BytesIO()
    _ov_im.save(_buf, format="PNG")
    ASSETS["overlay"] = base64.b64encode(_buf.getvalue()).decode("ascii")

    # Build clean arc: 5 unique top icons (no duplicate functions) + 6 original lower medallions
    _base_im = Image.open(io.BytesIO(base64.b64decode(ASSETS["base"]))).convert("RGBA")
    _w, _h = _base_im.size
    _icons_canvas = Image.new("RGBA", (_w, _h), (0, 0, 0, 0))

    # 5 unique top option icons placed at the 5 central upper-arc positions:
    _top_five_icons = [
        ((180, 96), (219, 83)),   # Settings (Triforce)
        ((219, 83), (258, 76)),   # Recording (Crest)
        ((258, 76), (298, 74)),   # Playback (Sun)
        ((298, 74), (338, 76)),   # Output Device (Storms)
        ((338, 76), (377, 83)),   # Input Device (Epona)
    ]
    # 6 original lower medallion icons in their exact original state:
    _lower_six_medallions = [
        ((199, 137), (199, 137)), # Slot 1: Forest Medallion
        ((238, 124), (238, 124)), # Slot 2: Fire Medallion
        ((278, 118), (278, 118)), # Slot 3: Water Medallion
        ((318, 118), (318, 118)), # Slot 4: Spirit Medallion
        ((357, 124), (357, 124)), # Slot 5: Shadow Medallion
        ((396, 137), (396, 137)), # Slot 6: Light Medallion
    ]
    _r = 14
    _mask = Image.new("L", (_r * 2, _r * 2), 0)
    _mdraw = ImageDraw.Draw(_mask)
    _mdraw.ellipse((0, 0, _r * 2 - 1, _r * 2 - 1), fill=255)

    for (sx, sy), (dx, dy) in _top_five_icons + _lower_six_medallions:
        _crop = _base_im.crop((sx - _r, sy - _r, sx + _r, sy + _r))
        _icons_canvas.paste(_crop, (dx - _r, dy - _r), _mask)

    _ibuf = io.BytesIO()
    _icons_canvas.save(_ibuf, format="PNG")
    ASSETS["icons_only"] = base64.b64encode(_ibuf.getvalue()).decode("ascii")
except Exception:
    pass

PW, PH = 274, 99
VIRTUAL_APP_VOLUMES = {}
ICON_B64_CACHE = {}


def run_cmd(args, timeout=1.5):
    try:
        res = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
        return res.stdout.strip()
    except Exception:
        return ""


def build_desktop_icon_map():
    """Mapeia classes/nomes de aplicativos para o campo Icon= dos arquivos .desktop."""
    mapping = {
        "brave": "brave-desktop",
        "brave-browser": "brave-desktop",
        "firefox": "firefox",
        "discord": "discord",
        "vesktop": "discord",
        "steam": "steam",
        "spotify": "spotify",
        "code": "code",
    }
    search_dirs = [
        "/usr/share/applications/*.desktop",
        os.path.expanduser("~/.local/share/applications/*.desktop"),
    ]
    for pattern in search_dirs:
        for path in glob.glob(pattern):
            try:
                base = os.path.basename(path).replace(".desktop", "").lower()
                icon_val = None
                name_val = None
                wmclass_val = None
                with open(path, "r", encoding="utf-8", errors="ignore") as f:
                    for line in f:
                        line = line.strip()
                        if line.startswith("Icon=") and not icon_val:
                            icon_val = line.split("=", 1)[1].strip()
                        elif line.startswith("Name=") and not name_val:
                            name_val = line.split("=", 1)[1].strip().lower()
                        elif line.startswith("StartupWMClass=") and not wmclass_val:
                            wmclass_val = line.split("=", 1)[1].strip().lower()
                if icon_val:
                    mapping[base] = icon_val
                    if name_val:
                        mapping[name_val] = icon_val
                    if wmclass_val:
                        mapping[wmclass_val] = icon_val
            except Exception:
                pass
    return mapping


DESKTOP_ICON_MAP = build_desktop_icon_map()


def resolve_icon_to_b64(app_key):
    """Resolve o ícone real do aplicativo aberto para PNG base64."""
    key = (app_key or "").lower().strip()
    if key in ICON_B64_CACHE:
        return ICON_B64_CACHE[key]

    candidates = [
        DESKTOP_ICON_MAP.get(key),
        DESKTOP_ICON_MAP.get(key.replace("-browser", "")),
        key,
        f"{key}-desktop",
        key.replace("-browser", ""),
    ]
    for k, v in DESKTOP_ICON_MAP.items():
        if key and (key in k or k in key):
            candidates.append(v)

    theme = Gtk.IconTheme.get_default()
    icon_path = None

    for cand in candidates:
        if not cand:
            continue
        if os.path.isfile(cand):
            icon_path = cand
            break
        info = theme.lookup_icon(cand, 128, 0)
        if info and info.get_filename():
            icon_path = info.get_filename()
            break
        for sz in ["256x256", "128x128", "64x64", "48x48"]:
            p = f"/usr/share/icons/hicolor/{sz}/apps/{cand}.png"
            if os.path.isfile(p):
                icon_path = p
                break
        if icon_path:
            break

    if not icon_path:
        info = theme.lookup_icon("application-x-executable", 64, 0) or theme.lookup_icon("audio-speakers", 64, 0)
        if info:
            icon_path = info.get_filename()

    if icon_path and os.path.isfile(icon_path):
        try:
            im = Image.open(icon_path).convert("RGBA")
            im = im.resize((96, 96), Image.Resampling.LANCZOS)
            buf = io.BytesIO()
            im.save(buf, format="PNG")
            b64 = base64.b64encode(buf.getvalue()).decode("ascii")
            ICON_B64_CACHE[key] = b64
            return b64
        except Exception:
            pass

    return ""


def make_feathered_wallpaper_b64(img_path_or_url):
    try:
        if img_path_or_url.startswith("file://"):
            local_path = urllib.parse.unquote(img_path_or_url[7:])
            im = Image.open(local_path).convert("RGB")
        elif img_path_or_url.startswith("http://") or img_path_or_url.startswith("https://"):
            req = urllib.request.Request(img_path_or_url, headers={"User-Agent": "Mozilla/5.0"})
            with urllib.request.urlopen(req, timeout=1.5) as resp:
                im = Image.open(io.BytesIO(resp.read())).convert("RGB")
        else:
            return ASSETS["default_spotify_wp"]

        wp = im.resize((PW, PH), Image.Resampling.LANCZOS).convert("RGBA")
        mask = Image.new("L", (PW, PH), 0)
        mdraw = ImageDraw.Draw(mask)
        mdraw.rounded_rectangle((5, 4, PW - 6, PH - 5), radius=8, fill=105)
        mask = mask.filter(ImageFilter.GaussianBlur(radius=4))
        wp.putalpha(mask)

        buf = io.BytesIO()
        wp.save(buf, format="PNG")
        return base64.b64encode(buf.getvalue()).decode("ascii")
    except Exception:
        return ASSETS["default_spotify_wp"]


def get_live_state():
    # 1. Check Spotify via playerctl
    spotify_playing = False
    spotify_title = "Song of Storms"
    spotify_wp = ASSETS["default_spotify_wp"]

    players_raw = run_cmd(["playerctl", "-l"])
    players = [p.strip() for p in players_raw.splitlines() if p.strip()]
    spotify_player = next((p for p in players if "spotify" in p.lower()), None)

    if spotify_player:
        st = run_cmd(["playerctl", "-p", spotify_player, "status"])
        if st.lower() == "playing":
            spotify_playing = True
            t = run_cmd(["playerctl", "-p", spotify_player, "metadata", "xesam:title"])
            art = run_cmd(["playerctl", "-p", spotify_player, "metadata", "mpris:artUrl"])
            if t:
                spotify_title = t
            if art:
                spotify_wp = make_feathered_wallpaper_b64(art)

    # 2. Gather Playback: ONLY open apps with sound in real time (sink-inputs + MPRIS audio players)
    playback_items = []
    seen_keys = set()

    raw_si = run_cmd(["pactl", "-f", "json", "list", "sink-inputs"])
    if raw_si:
        try:
            for item in json.loads(raw_si):
                idx = item.get("index")
                props = item.get("properties", {})
                app_name = (
                    props.get("application.name")
                    or props.get("application.process.binary")
                    or props.get("node.name")
                    or f"App {idx}"
                )
                if "pavucontrol" in app_name.lower() or "zelda" in app_name.lower():
                    continue
                binary = props.get("application.process.binary") or app_name
                vol_dict = item.get("volume", {})
                first_ch = next(iter(vol_dict.values()), {})
                pct_str = first_ch.get("value_percent", "80%").replace("%", "").strip()
                pct = int(pct_str) if pct_str.isdigit() else 80
                muted = bool(item.get("mute", False))
                icon_b64 = resolve_icon_to_b64(binary) or resolve_icon_to_b64(app_name)
                seen_keys.add(app_name.lower())
                seen_keys.add(binary.lower())
                playback_items.append({
                    "id": f"si:{idx}",
                    "name": app_name[:18],
                    "volume": 0 if muted else min(100, pct),
                    "iconB64": icon_b64,
                })
        except Exception:
            pass

    # Also check active MPRIS media players that have audio loaded
    for p in players:
        p_base = p.split(".")[0].lower()
        if p_base in seen_keys or any(p_base in k for k in seen_keys):
            continue
        st = run_cmd(["playerctl", "-p", p, "status"]).lower()
        if st in ("playing", "paused"):
            seen_keys.add(p_base)
            icon_b64 = resolve_icon_to_b64(p_base)
            vol_raw = run_cmd(["playerctl", "-p", p, "volume"])
            try:
                pct = int(round(float(vol_raw) * 100)) if vol_raw else VIRTUAL_APP_VOLUMES.get(p_base, 80)
            except Exception:
                pct = VIRTUAL_APP_VOLUMES.get(p_base, 80)
            playback_items.append({
                "id": f"app:{p_base}",
                "name": p_base.capitalize()[:18],
                "volume": min(100, max(0, pct)),
                "iconB64": icon_b64,
            })

    # 3. Recording (source-outputs)
    recording_items = []
    mic_icon_b64 = resolve_icon_to_b64("audio-input-microphone")
    raw_so = run_cmd(["pactl", "-f", "json", "list", "source-outputs"])
    if raw_so:
        try:
            for item in json.loads(raw_so):
                idx = item.get("index")
                props = item.get("properties", {})
                name = props.get("application.name") or props.get("node.name") or f"Rec {idx}"
                if "pavucontrol" in name.lower() or "peak" in name.lower():
                    continue
                vol_dict = item.get("volume", {})
                first_ch = next(iter(vol_dict.values()), {})
                pct_str = first_ch.get("value_percent", "80%").replace("%", "").strip()
                pct = int(pct_str) if pct_str.isdigit() else 80
                recording_items.append({
                    "id": f"so:{idx}",
                    "name": name[:20],
                    "volume": min(100, pct),
                    "iconB64": resolve_icon_to_b64(name) or mic_icon_b64,
                })
        except Exception:
            pass
    if not recording_items:
        recording_items.append({
            "id": "so:none",
            "name": "Sem gravação ativa",
            "volume": 0,
            "iconB64": mic_icon_b64,
        })

    # 4. Output Devices (sinks)
    output_items = []
    spk_icon_b64 = resolve_icon_to_b64("audio-speakers")
    raw_sinks = run_cmd(["pactl", "-f", "json", "list", "sinks"])
    if raw_sinks:
        try:
            for item in json.loads(raw_sinks):
                idx = item.get("index")
                desc = item.get("description") or item.get("name") or f"Sink {idx}"
                if "[" in desc and "]" in desc:
                    short_desc = desc.split("[")[-1].replace("]", "").strip()
                    if len(short_desc) < 4:
                        short_desc = desc[:22]
                else:
                    short_desc = desc[:22]
                vol_dict = item.get("volume", {})
                first_ch = next(iter(vol_dict.values()), {})
                pct_str = first_ch.get("value_percent", "80%").replace("%", "").strip()
                pct = int(pct_str) if pct_str.isdigit() else 80
                output_items.append({
                    "id": f"sink:{idx}",
                    "name": short_desc,
                    "fullName": desc,
                    "volume": min(100, pct),
                    "iconB64": spk_icon_b64,
                })
        except Exception:
            pass

    # 5. Input Devices (sources)
    input_items = []
    raw_sources = run_cmd(["pactl", "-f", "json", "list", "sources"])
    if raw_sources:
        try:
            for item in json.loads(raw_sources):
                idx = item.get("index")
                name_raw = item.get("name") or ""
                if ".monitor" in name_raw:
                    continue
                desc = item.get("description") or name_raw or f"Mic {idx}"
                vol_dict = item.get("volume", {})
                first_ch = next(iter(vol_dict.values()), {})
                pct_str = first_ch.get("value_percent", "80%").replace("%", "").strip()
                pct = int(pct_str) if pct_str.isdigit() else 80
                input_items.append({
                    "id": f"source:{idx}",
                    "name": desc[:22],
                    "fullName": desc,
                    "volume": min(100, pct),
                    "iconB64": mic_icon_b64,
                })
        except Exception:
            pass

    # 6. Settings (Cards)
    settings_items = []
    raw_cards = run_cmd(["pactl", "-f", "json", "list", "cards"])
    if raw_cards:
        try:
            for item in json.loads(raw_cards):
                idx = item.get("index")
                props = item.get("properties", {})
                desc = props.get("device.description") or item.get("name") or f"Card {idx}"
                active_prof = item.get("active_profile") or "default"
                settings_items.append({
                    "id": f"card:{idx}",
                    "name": f"{desc[:16]} ({active_prof[:14]})",
                    "fullName": f"{desc} — {active_prof}",
                    "volume": 100,
                    "iconB64": spk_icon_b64,
                })
        except Exception:
            pass

    return {
        "spotify": {
            "playing": spotify_playing,
            "title": spotify_title,
            "wallpaperB64": spotify_wp,
        },
        "tabs": {
            "playback": playback_items,
            "recording": recording_items,
            "output": output_items,
            "input": input_items,
            "settings": settings_items,
        },
    }


def set_system_volume(item_id, val):
    val = max(0, min(100, int(val)))
    if item_id.startswith("si:"):
        idx = item_id.split(":", 1)[1]
        run_cmd(["pactl", "set-sink-input-mute", idx, "0"])
        run_cmd(["pactl", "set-sink-input-volume", idx, f"{val}%"])
    elif item_id.startswith("so:"):
        idx = item_id.split(":", 1)[1]
        if idx.isdigit():
            run_cmd(["pactl", "set-source-output-volume", idx, f"{val}%"])
    elif item_id.startswith("sink:"):
        idx = item_id.split(":", 1)[1]
        run_cmd(["pactl", "set-sink-mute", idx, "0"])
        run_cmd(["pactl", "set-sink-volume", idx, f"{val}%"])
    elif item_id.startswith("source:"):
        idx = item_id.split(":", 1)[1]
        run_cmd(["pactl", "set-source-mute", idx, "0"])
        run_cmd(["pactl", "set-source-volume", idx, f"{val}%"])
    elif item_id.startswith("app:"):
        app_key = item_id.split(":", 1)[1]
        VIRTUAL_APP_VOLUMES[app_key] = val
        run_cmd(["playerctl", "-p", app_key, "volume", str(round(val / 100.0, 2))])


HTML_PAGE = """<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8" />
  <title>Waybar Zelda Sound Extension</title>
  <style>
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      user-select: none;
      font-family: "CodeNewRoman Nerd Font Propo", "Noto Serif", Georgia, serif;
    }
    html, body {
      width: 100%;
      height: 100%;
      overflow: hidden;
      background: transparent;
    }
    /* Painel com o EXATO tema de Tapeçaria da Waybar (tapestry-bg.png + tapestry-border.png) */
    #tapestryPanel {
      position: relative;
      width: 100vw;
      height: 100vh;
      background-color: #211813;
      background-repeat: repeat;
      border-style: solid;
      border-width: 6px;
      border-image-slice: 24 24 24 24;
      border-image-width: 6px;
      border-image-repeat: repeat;
      border-radius: 2px;
      box-shadow: inset 0px 0px 14px rgba(0, 0, 0, 0.75);
      overflow: hidden;
    }
    @keyframes ringPulse {
      0%, 100% {
        box-shadow: 0 0 8px 2px rgba(255, 255, 255, 0.9), inset 0 0 6px rgba(255, 255, 255, 0.7);
      }
      50% {
        box-shadow: 0 0 14px 4px rgba(255, 255, 255, 1), inset 0 0 10px rgba(255, 255, 255, 0.9);
      }
    }
    .zelda-ring {
      border: 2.5px solid #ffffff !important;
      animation: ringPulse 2s infinite ease-in-out;
    }
    .icon-hotspot {
      position: absolute;
      transform: translate(-50%, -50%);
      width: 5.6%;
      aspect-ratio: 1 / 1;
      border-radius: 9999px;
      cursor: pointer;
      transition: all 0.15s ease;
      border: 2px solid transparent;
    }
    .icon-hotspot:hover {
      border-color: rgba(246, 214, 118, 0.9);
    }
    /* Tooltip idêntico ao da Waybar (tapestry-bg + borda #9e4a2f) aparecendo EM CIMA do ícone */
    .icon-tooltip {
      pointer-events: none;
      position: absolute;
      bottom: 100%;
      left: 50%;
      transform: translateX(-50%);
      margin-bottom: 6px;
      white-space: nowrap;
      opacity: 0;
      transition: opacity 0.15s ease;
      padding: 3px 9px;
      border-radius: 2px;
      background-color: #211813;
      border: 2px solid #9e4a2f;
      box-shadow: 0px 2px 8px rgba(0, 0, 0, 0.85);
      color: #e8d8b8;
      font-size: 12px;
      z-index: 60;
    }
    .icon-hotspot:hover .icon-tooltip {
      opacity: 1;
    }
    .medallion-app-badge {
      width: 100%;
      height: 100%;
      border-radius: 50%;
      background: radial-gradient(circle, #1c1612 55%, #0c0a08 100%);
      border: 1.5px solid #6b4c2b;
      box-shadow: inset 0 1px 3px rgba(0,0,0,0.9), 0 2px 5px rgba(0,0,0,0.85);
      display: flex;
      align-items: center;
      justify-content: center;
      overflow: hidden;
    }
    .medallion-app-badge img {
      width: 76%;
      height: 76%;
      object-fit: contain;
      filter: drop-shadow(0 1px 2px rgba(0,0,0,0.85));
    }
    .bottom-bar {
      position: absolute;
      left: 8px;
      right: 8px;
      bottom: 6px;
      z-index: 40;
      display: flex;
      justify-content: center;
    }
    .bottom-pill-container {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      justify-content: center;
      gap: 6px;
      padding: 4px 10px;
      border-radius: 4px;
      background-color: rgba(24, 16, 12, 0.88);
      border: 1.5px solid #9e4a2f;
      box-shadow: 0 3px 10px rgba(0, 0, 0, 0.8);
    }
    .app-pill {
      display: flex;
      align-items: center;
      gap: 6px;
      padding: 3px 8px;
      border-radius: 4px;
      font-size: 12px;
      color: #dfd1b4;
      border: 1px solid transparent;
      transition: all 0.15s ease;
    }
    .app-pill.loudest {
      background: rgba(158, 74, 47, 0.32);
      color: #f6d676;
      border-color: #e8c478;
      font-weight: 600;
    }
    .app-real-icon {
      width: 18px;
      height: 18px;
      object-fit: contain;
      filter: drop-shadow(0 1px 2px rgba(0,0,0,0.8));
    }
    input[type="range"] {
      -webkit-appearance: none;
      appearance: none;
      width: 68px;
      height: 6px;
      background: rgba(255, 255, 255, 0.18);
      border-radius: 999px;
      outline: none;
      cursor: pointer;
      user-select: auto;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 13px;
      height: 13px;
      border-radius: 50%;
      background: #f6d676;
      border: 1.5px solid #211813;
      box-shadow: 0 1px 3px rgba(0,0,0,0.8);
      cursor: grab;
    }
    input[type="range"]:active::-webkit-slider-thumb {
      cursor: grabbing;
      transform: scale(1.15);
    }
  </style>
</head>
<body>
  <div id="tapestryPanel">

    <!-- 1. ÍCONES ORIGINAIS DO ZELDA (5 funções únicas no arco superior + 6 medalhões originais no arco inferior) -->
    <img id="iconsOnlyImg"
         style="pointer-events: none; position: absolute; inset: 0; width: 100%; height: 100%; object-fit: fill; z-index: 5; filter: drop-shadow(0 3px 6px rgba(0,0,0,0.85));" />

    <!-- 2. CIFRA / PARTITURA EM 3 CAMADAS (Papel -> Wallpaper do Spotify OU Ícone REAL do App com mais som -> Partitura Original) -->
    <div style="pointer-events: none; position: absolute; left: 20%; top: 51%; width: 60%; height: 32%; z-index: 10; overflow: hidden; box-shadow: 0 6px 18px rgba(0,0,0,0.85);">
      <!-- Camada 1: O Papel -->
      <img id="papelImg"
           style="position: absolute; inset: 0; width: 100%; height: 100%; object-fit: fill; z-index: 10;" />

      <!-- Camada 2 (ENTRE O PAPEL E A PARTITURA): Wallpaper da música do Spotify OU Ícone REAL do App Aberto com MAIS SOM -->
      <div id="middleLayer"
           style="position: absolute; inset: 0; z-index: 20; display: flex; align-items: center; justify-content: center;">
      </div>

      <!-- Camada 3: A Partitura (Clave de Sol Original + 5 Linhas + Ornamentos) -->
      <img id="overlayImg"
           style="position: absolute; inset: 0; width: 100%; height: 100%; object-fit: fill; z-index: 30;" />

      <!-- Título no topo da partitura -->
      <div style="position: absolute; left: 0; right: 0; top: 5%; z-index: 35; text-align: center; padding: 0 14px;">
        <span id="partituraTitle"
              style="font-size: 14px; font-weight: 600; color: #221306; text-shadow: 0 1px 1px rgba(240, 225, 190, 0.85);">
          Song of Storms
        </span>
      </div>
    </div>

    <!-- 3. HOTSPOTS DOS ÍCONES (5 funções no arco superior + 6 medalhões substituídos 1 a 1 pelos apps com som em tempo real) -->
    <div id="iconsContainer" style="position: absolute; inset: 0; z-index: 25;"></div>

    <!-- 4. BARRA INFERIOR COM OS APPS ABERTOS E SEUS ÍCONES REAIS -->
    <div class="bottom-bar">
      <div id="bottomList" class="bottom-pill-container"></div>
    </div>

  </div>

  <script>
    const ORIGINAL_ICONS = [
      { id: 'settings', label: 'Settings', x: 219 / 596 * 100, y: 83 / 335 * 100, tab: 'settings' },
      { id: 'recording', label: 'Recording', x: 258 / 596 * 100, y: 76 / 335 * 100, tab: 'recording' },
      { id: 'playback', label: 'Playback', x: 298 / 596 * 100, y: 74 / 335 * 100, tab: 'playback' },
      { id: 'output', label: 'Output Device', x: 338 / 596 * 100, y: 76 / 335 * 100, tab: 'output' },
      { id: 'input', label: 'Input Device', x: 377 / 596 * 100, y: 83 / 335 * 100, tab: 'input' }
    ];

    const MEDALLION_SLOTS = [
      { idx: 0, x: 199 / 596 * 100, y: 137 / 335 * 100 },
      { idx: 1, x: 238 / 596 * 100, y: 124 / 335 * 100 },
      { idx: 2, x: 278 / 596 * 100, y: 118 / 335 * 100 },
      { idx: 3, x: 318 / 596 * 100, y: 118 / 335 * 100 },
      { idx: 4, x: 357 / 596 * 100, y: 124 / 335 * 100 },
      { idx: 5, x: 396 / 596 * 100, y: 137 / 335 * 100 }
    ];

    let activeIconId = 'playback';
    let activeTab = 'playback';
    let currentState = null;
    let isDraggingSlider = false;
    let lastRenderedSignature = '';
    let lastMedallionSignature = '';
    let lastMiddleKey = '';
    let volumeTimers = {};

    window.addEventListener('mouseup', () => { isDraggingSlider = false; });
    window.addEventListener('pointerup', () => { isDraggingSlider = false; });

    function renderIcons() {
      const container = document.getElementById('iconsContainer');
      const playbackApps = (currentState && currentState.tabs && currentState.tabs.playback)
        ? currentState.tabs.playback
        : [];

      const topHtml = ORIGINAL_ICONS.map(icon => {
        const isActive = icon.id === activeIconId;
        return `
          <div class="icon-hotspot ${isActive ? 'zelda-ring' : ''}"
               style="left: ${icon.x}%; top: ${icon.y}%;"
               onclick="selectIcon('${icon.id}', '${icon.tab}')">
            <div class="icon-tooltip">${icon.label}</div>
          </div>
        `;
      }).join('');

      // Os 6 medalhões de baixo: cada app aberto com som em tempo real substitui 1 medalhão (1 app = 1 ícone, 2 apps = 2 ícones...).
      // Os slots sem app não recebem overlay e permanecem no estado original do medalhão!
      const medallionsHtml = MEDALLION_SLOTS.map((slot, i) => {
        const app = playbackApps[i];
        if (!app) {
          return '';
        }
        const imgHtml = app.iconB64
          ? `<img src="data:image/png;base64,${app.iconB64}" />`
          : `<span style="font-size:10px;color:#f6d676;font-weight:bold;">${app.name.slice(0, 2)}</span>`;
        return `
          <div class="icon-hotspot"
               style="left: ${slot.x}%; top: ${slot.y}%; width: 5.0%; aspect-ratio: 1 / 1;"
               onclick="selectIcon('playback', 'playback')">
            <div class="medallion-app-badge">
              ${imgHtml}
            </div>
            <div id="medallion-tip-${app.id}" class="icon-tooltip">${app.name} (${app.volume}%)</div>
          </div>
        `;
      }).join('');

      container.innerHTML = topHtml + medallionsHtml;
    }

    function renderParchment() {
      if (!currentState) return;
      const middle = document.getElementById('middleLayer');
      const titleEl = document.getElementById('partituraTitle');
      const items = currentState.tabs[activeTab] || [];

      if (currentState.spotify && currentState.spotify.playing) {
        const key = 'spotify:' + currentState.spotify.title;
        if (lastMiddleKey !== key) {
          middle.innerHTML = `<img src="data:image/png;base64,${currentState.spotify.wallpaperB64}" style="width:100%;height:100%;object-fit:cover;" />`;
          lastMiddleKey = key;
        }
        titleEl.textContent = currentState.spotify.title;
      } else if (items.length > 0) {
        const loudest = items.reduce((max, cur) => (cur.volume > max.volume ? cur : max), items[0]);
        const key = 'app:' + loudest.id;
        if (lastMiddleKey !== key) {
          if (loudest.iconB64) {
            middle.innerHTML = `<img src="data:image/png;base64,${loudest.iconB64}" style="width:68px;height:68px;object-fit:contain;opacity:0.34;filter:sepia(0.25);" />`;
          } else {
            middle.innerHTML = '';
          }
          lastMiddleKey = key;
        }
        titleEl.textContent = `${loudest.name} (${loudest.volume}%)`;
      } else {
        if (lastMiddleKey !== 'empty') {
          middle.innerHTML = '';
          lastMiddleKey = 'empty';
        }
        titleEl.textContent = 'Song of Storms';
      }
    }

    function updateHighlightAndLabels() {
      if (!currentState) return;
      const items = currentState.tabs[activeTab] || [];
      const loudestItem = items.length
        ? items.reduce((max, cur) => (cur.volume > max.volume ? cur : max), items[0])
        : null;

      items.forEach(item => {
        const pill = document.getElementById(`pill-${item.id}`);
        const pctEl = document.getElementById(`pct-${item.id}`);
        const sliderEl = document.getElementById(`slider-${item.id}`);
        if (pill) {
          if (loudestItem && item.id === loudestItem.id) {
            pill.classList.add('loudest');
          } else {
            pill.classList.remove('loudest');
          }
        }
        if (pctEl) {
          pctEl.textContent = `${item.volume}%`;
        }
        if (sliderEl && !isDraggingSlider) {
          sliderEl.value = item.volume;
        }
      });

      const playbackApps = (currentState.tabs && currentState.tabs.playback) || [];
      playbackApps.forEach(app => {
        const tipEl = document.getElementById(`medallion-tip-${app.id}`);
        if (tipEl) {
          tipEl.textContent = `${app.name} (${app.volume}%)`;
        }
      });
    }

    function renderUI(forceRebuild = false) {
      if (!currentState) return;
      renderParchment();

      const playbackApps = (currentState.tabs && currentState.tabs.playback) || [];
      const medSig = activeIconId + ':' + playbackApps.map(a => a.id + ':' + (a.iconB64 ? '1' : '0')).join('|');
      if (forceRebuild || medSig !== lastMedallionSignature) {
        lastMedallionSignature = medSig;
        renderIcons();
      }

      const items = currentState.tabs[activeTab] || [];
      const sig = activeTab + ':' + items.map(i => i.id + ':' + i.name).join('|');

      if (!forceRebuild && sig === lastRenderedSignature) {
        updateHighlightAndLabels();
        return;
      }

      lastRenderedSignature = sig;
      const bottom = document.getElementById('bottomList');
      if (items.length === 0) {
        bottom.style.display = 'none';
        bottom.innerHTML = '';
        return;
      }
      bottom.style.display = 'flex';
      const loudestItem = items.reduce((max, cur) => (cur.volume > max.volume ? cur : max), items[0]);

      bottom.innerHTML = items.map(item => {
        const isLoudest = loudestItem && item.id === loudestItem.id;
        const iconTag = item.iconB64
          ? `<img src="data:image/png;base64,${item.iconB64}" class="app-real-icon" />`
          : '';
        return `
          <div id="pill-${item.id}" class="app-pill ${isLoudest ? 'loudest' : ''}">
            ${iconTag}
            <span>${item.name}</span>
            <input id="slider-${item.id}"
                   type="range" min="0" max="100" value="${item.volume}"
                   onmousedown="isDraggingSlider = true"
                   onpointerdown="isDraggingSlider = true"
                   oninput="onSliderInput('${item.id}', this.value)"
                   onchange="isDraggingSlider = false; onSliderInput('${item.id}', this.value)" />
            <span id="pct-${item.id}" style="font-family: monospace; color: #f6d676; min-width: 30px; text-align: right;">${item.volume}%</span>
          </div>
        `;
      }).join('');
    }

    function selectIcon(iconId, tab) {
      activeIconId = iconId;
      activeTab = tab;
      lastMiddleKey = '';
      renderUI(true);
    }

    function onSliderInput(itemId, newVal) {
      if (!currentState) return;
      const items = currentState.tabs[activeTab] || [];
      const target = items.find(i => i.id === itemId);
      const parsed = parseInt(newVal, 10);
      if (target) {
        target.volume = parsed;
        renderParchment();
        updateHighlightAndLabels();
      }
      if (volumeTimers[itemId]) {
        clearTimeout(volumeTimers[itemId]);
      }
      volumeTimers[itemId] = setTimeout(() => {
        fetch(`/api/volume?id=${encodeURIComponent(itemId)}&val=${encodeURIComponent(parsed)}`).catch(() => {});
      }, 25);
    }

    async function loadInitialAssets() {
      const res = await fetch('/api/assets');
      const assets = await res.json();
      const panel = document.getElementById('tapestryPanel');
      panel.style.backgroundImage = `url('data:image/png;base64,${assets.tapestry_bg}')`;
      panel.style.borderImageSource = `url('data:image/png;base64,${assets.tapestry_border}')`;
      document.getElementById('iconsOnlyImg').src = 'data:image/png;base64,' + assets.icons_only;
      document.getElementById('papelImg').src = 'data:image/png;base64,' + assets.papel;
      document.getElementById('overlayImg').src = 'data:image/png;base64,' + assets.overlay;
    }

    async function pollState() {
      if (isDraggingSlider) return;
      try {
        const res = await fetch('/api/state');
        currentState = await res.json();
        renderUI(false);
      } catch (e) {}
    }

    renderIcons();
    loadInitialAssets().then(() => {
      pollState();
      setInterval(pollState, 1500);
    });
  </script>
</body>
</html>
"""


class RequestHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/":
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(HTML_PAGE.encode("utf-8"))
        elif parsed.path == "/api/assets":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(ASSETS).encode("utf-8"))
        elif parsed.path == "/api/state":
            st = get_live_state()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(st).encode("utf-8"))
        elif parsed.path == "/api/volume":
            qs = urllib.parse.parse_qs(parsed.query)
            item_id = qs.get("id", [""])[0]
            val = qs.get("val", ["80"])[0]
            set_system_volume(item_id, val)
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(b'{"ok":true}')
        else:
            self.send_response(404)
            self.end_headers()


class WaybarSoundExtension(Gtk.Window):
    def __init__(self, port):
        super().__init__()
        self.set_decorated(False)
        self.set_resizable(False)
        self.set_size_request(540, 310)

        # Configura como uma extensão real da Waybar (Layer Shell ancorado no topo direito abaixo da barra)
        GtkLayerShell.init_for_window(self)
        GtkLayerShell.set_namespace(self, "waybar-zelda-sound")
        GtkLayerShell.set_layer(self, GtkLayerShell.Layer.TOP)
        GtkLayerShell.set_anchor(self, GtkLayerShell.Edge.TOP, True)
        GtkLayerShell.set_anchor(self, GtkLayerShell.Edge.RIGHT, True)
        GtkLayerShell.set_margin(self, GtkLayerShell.Edge.TOP, 4)
        GtkLayerShell.set_margin(self, GtkLayerShell.Edge.RIGHT, 10)
        GtkLayerShell.set_keyboard_mode(self, GtkLayerShell.KeyboardMode.ON_DEMAND)

        screen = self.get_screen()
        visual = screen.get_rgba_visual()
        if visual:
            self.set_visual(visual)
        self.set_app_paintable(True)

        self.webview = WebKit2.WebView()
        rgba = Gdk.RGBA(0, 0, 0, 0)
        self.webview.set_background_color(rgba)
        self.add(self.webview)
        self.webview.load_uri(f"http://127.0.0.1:{port}/")

        self.connect("destroy", self.on_quit)
        self.connect("key-press-event", self.on_key)

    def on_key(self, widget, event):
        if event.keyval == Gdk.KEY_Escape:
            self.on_quit()

    def on_quit(self, *args):
        try:
            if os.path.exists(PID_FILE):
                os.remove(PID_FILE)
        except Exception:
            pass
        Gtk.main_quit()


def main():
    if os.path.exists(PID_FILE):
        try:
            with open(PID_FILE, "r") as f:
                old_pid = int(f.read().strip())
            os.kill(old_pid, 15)
            os.remove(PID_FILE)
            return
        except Exception:
            try:
                os.remove(PID_FILE)
            except Exception:
                pass

    with open(PID_FILE, "w") as f:
        f.write(str(os.getpid()))

    server = HTTPServer(("127.0.0.1", 0), RequestHandler)
    port = server.server_port
    t = threading.Thread(target=server.serve_forever, daemon=True)
    t.start()

    win = WaybarSoundExtension(port)
    win.show_all()
    Gtk.main()


if __name__ == "__main__":
    main()
