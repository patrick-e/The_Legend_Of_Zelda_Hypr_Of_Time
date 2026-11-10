-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
-- HYPRLAND LUA CONFIG (Migrated from hyprland.conf)     --
-- https://wiki.hypr.land/configuring/                   --
-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

------------------
---- MONITORS ----
------------------

-- See https://wiki.hypr.land/configuring/core/monitors/
hl.monitor({
    output   = "",
    mode     = "preferred",
    position = "auto",
    scale    = "auto",
})


---------------------
---- MY PROGRAMS ----
---------------------

local terminal    = "kitty"
local fileManager = "dolphin"
local menu        = "hyprlauncher"


-------------------
---- AUTOSTART ----
-------------------

-- See https://wiki.hypr.land/configuring/core/autostart/
hl.on("hyprland.start", function ()
    hl.exec_cmd("swaync")
    hl.exec_cmd("hypridle")
    hl.exec_cmd("hyprsunset")
    hl.exec_cmd("gnome-keyring-daemon --start --components=secrets")
    hl.exec_cmd("bash -lc 'sleep 2; __HOME__/.config/hypr/scripts/waybar-fullscreen.sh >> /tmp/waybar-script.log 2>&1'")
    hl.exec_cmd("__HOME__/.config/hypr/scripts/navi-say.sh --startup")
end)


-------------------------------
---- ENVIRONMENT VARIABLES ----
-------------------------------

-- See https://wiki.hypr.land/configuring/core/environment-variables/
hl.env("QT_QPA_PLATFORMTHEME", "qt5ct")
-- hl.env("GSK_RENDERER", "gl")
hl.env("ELECTRON_OZONE_PLATFORM_HINT", "auto")

hl.env("XCURSOR_SIZE", "24")
hl.env("HYPRCURSOR_SIZE", "24")

-- Wayland & Toolkit Compatibility (Prevents visual glitches & forces native rendering)
hl.env("GDK_BACKEND", "wayland,x11,*")
hl.env("QT_QPA_PLATFORM", "wayland;xcb")
hl.env("SDL_VIDEODRIVER", "wayland,x11")
hl.env("CLUTTER_BACKEND", "wayland")

-- Java AWT WM Nonreparenting (Fixes gray/blank screens in Java apps like SKlauncher/Minecraft)
hl.env("_JAVA_AWT_WM_NONREPARENTING", "1")


-----------------------
----- PERMISSIONS -----
-----------------------

-- See https://wiki.hypr.land/configuring/core/advanced-configuration/permissions/
-- hl.config({
--   ecosystem = {
--     enforce_permissions = true,
--   },
-- })

-- hl.permission({ binary = "/usr/(bin|local/bin)/grim", type = "screencopy", mode = "allow" })
-- hl.permission({ binary = "/usr/(lib|libexec|lib64)/xdg-desktop-portal-hyprland", type = "screencopy", mode = "allow" })
-- hl.permission({ binary = "/usr/(bin|local/bin)/hyprpm", type = "plugin", mode = "allow" })


-----------------------
---- LOOK AND FEEL ----
-----------------------

hl.plugin.load("__HOME__/.config/hypr/plugins/libzelda_borders.so")

-- Refer to https://wiki.hypr.land/configuring/core/config-options/
hl.config({
    plugin = {
        imgborders = {
            -- Use zelda_border_ornate.png (com arabesco central) ou zelda_border.png (apenas cantos e linha dupla)
            image  = "__HOME__/.config/hypr/assets/zelda_border_ornate.png",
            sizes  = 74,
            insets = 64,
            scale  = 1.0,
            smooth = true,
            blur   = false,
        },
    },

    general = {
        gaps_in  = 8,
        gaps_out = 20,

        border_size = 0,

        col = {
            active_border   = { colors = {"rgba(f6f4e0ee)", "rgba(d8cfa8ee)"}, angle = 45 },
            inactive_border = "rgba(595959aa)",
        },

        resize_on_border = true,
        allow_tearing    = false,

        layout = "dwindle",
    },

    decoration = {
        rounding       = 0,
        rounding_power = 2,

        active_opacity     = 1.0,
        inactive_opacity   = 0.68,
        fullscreen_opacity = 1.0,

        shadow = {
            enabled      = true,
            range        = 15,
            render_power = 4,
            color        = "rgba(0,0,0,0.5)",
        },

        blur = {
            enabled        = true,
            size           = 7,
            passes         = 3,
            vibrancy       = 0.1696,
            ignore_opacity = true,
            xray           = false,
            popups         = true,
        },
    },

    xwayland = {
        force_zero_scaling = true,
    },

    animations = {
        enabled = true,
    },

    dwindle = {
        preserve_split = true,
    },

    master = {
        new_status = "master",
    },

    scrolling = {
        fullscreen_on_one_column = true,
    },

    misc = {
        force_default_wallpaper = -1,
        disable_hyprland_logo   = false,
        focus_on_activate       = true,
    },

    cursor = {
        no_warps = true,
    },

    layers = {
        allow_new_top_layers_over_existing_fullscreen = true,
    },
})

-- Curves and animations
hl.curve("fluid",          { type = "bezier", points = { {0.15, 0.85}, {0.25, 1}    } })
hl.curve("snappy",         { type = "bezier", points = { {0.3, 1},     {0.4, 1}     } })
hl.curve("easeOutQuint",   { type = "bezier", points = { {0.23, 1},    {0.32, 1}    } })
hl.curve("easeInOutCubic", { type = "bezier", points = { {0.65, 0.05}, {0.36, 1}    } })
hl.curve("linear",         { type = "bezier", points = { {0, 0},       {1, 1}       } })
hl.curve("almostLinear",   { type = "bezier", points = { {0.5, 0.5},   {0.75, 1}    } })
hl.curve("quick",          { type = "bezier", points = { {0.15, 0},    {0.1, 1}     } })

hl.animation({ leaf = "global",           enabled = true, speed = 10,   bezier = "default" })
hl.animation({ leaf = "border",           enabled = true, speed = 5.39, bezier = "easeOutQuint" })
hl.animation({ leaf = "windows",          enabled = true, speed = 4.79, bezier = "easeOutQuint" })
hl.animation({ leaf = "windowsIn",        enabled = true, speed = 4.1,  bezier = "easeOutQuint", style = "popin 87%" })
hl.animation({ leaf = "windowsOut",       enabled = true, speed = 1.49, bezier = "linear",       style = "popin 87%" })
hl.animation({ leaf = "fadeIn",           enabled = true, speed = 1.73, bezier = "almostLinear" })
hl.animation({ leaf = "fadeOut",          enabled = true, speed = 1.46, bezier = "almostLinear" })
hl.animation({ leaf = "fade",             enabled = true, speed = 3.03, bezier = "quick" })
hl.animation({ leaf = "layers",           enabled = true, speed = 3.81, bezier = "easeOutQuint" })
hl.animation({ leaf = "layersIn",         enabled = true, speed = 4,    bezier = "easeOutQuint", style = "fade" })
hl.animation({ leaf = "layersOut",        enabled = true, speed = 1.5,  bezier = "linear",       style = "fade" })
hl.animation({ leaf = "fadeLayersIn",     enabled = true, speed = 1.79, bezier = "almostLinear" })
hl.animation({ leaf = "fadeLayersOut",    enabled = true, speed = 1.39, bezier = "almostLinear" })
hl.animation({ leaf = "workspaces",       enabled = true, speed = 1.94, bezier = "almostLinear", style = "fade" })
hl.animation({ leaf = "workspacesIn",     enabled = true, speed = 1.21, bezier = "almostLinear", style = "fade" })
hl.animation({ leaf = "workspacesOut",    enabled = true, speed = 1.94, bezier = "almostLinear", style = "fade" })
hl.animation({ leaf = "specialWorkspace", enabled = true, speed = 4,    bezier = "fluid",        style = "slidefadevert -35%" })
hl.animation({ leaf = "zoomFactor",       enabled = true, speed = 7,    bezier = "quick" })


---------------
---- INPUT ----
---------------

hl.config({
    input = {
        kb_layout  = "br",
        kb_variant = "",
        kb_model   = "",
        kb_options = "",
        kb_rules   = "",

        follow_mouse = 1,

        sensitivity = 0, -- -1.0 - 1.0, 0 means no modification.

        touchpad = {
            natural_scroll = false,
        },
    },
})

hl.gesture({
    fingers   = 3,
    direction = "horizontal",
    action    = "workspace",
})

hl.device({
    name        = "epic-mouse-v1",
    sensitivity = -0.5,
})


---------------------
---- KEYBINDINGS ----
---------------------

local mainMod = "SUPER" -- Sets "Windows" key as main modifier

hl.bind("ALT + CTRL + T",     hl.dsp.exec_cmd(terminal))
hl.bind(mainMod .. " + Escape", hl.dsp.window.close())
hl.bind(mainMod .. " + M",    hl.dsp.exec_cmd("command -v hyprshutdown >/dev/null 2>&1 && hyprshutdown || hyprctl dispatch 'hl.dsp.exit()'"))
hl.bind(mainMod .. " + E",    hl.dsp.exec_cmd(fileManager))
hl.bind(mainMod .. " + V",    hl.dsp.window.float({ action = "toggle" }))
hl.bind(mainMod .. " + R",    hl.dsp.exec_cmd(menu))
hl.bind(mainMod .. " + P",    hl.dsp.window.pseudo())
hl.bind(mainMod .. " + J",    hl.dsp.layout("togglesplit")) -- dwindle
hl.bind(mainMod .. " + Space", hl.dsp.exec_cmd("env LD_PRELOAD=__HOME__/.config/wofi/libwofi_hover.so wofi --show drun"))
hl.bind(mainMod .. " + F",    hl.dsp.window.fullscreen({ mode = "maximized" }))
hl.bind(mainMod .. " + N",    hl.dsp.exec_cmd("~/.config/hypr/scripts/hyprsunset-toggle.sh")) -- Toggle modo noturno (hyprsunset)

-- Move focus with mainMod + arrow keys
hl.bind(mainMod .. " + left",  hl.dsp.focus({ direction = "left" }))
hl.bind(mainMod .. " + right", hl.dsp.focus({ direction = "right" }))
hl.bind(mainMod .. " + up",    hl.dsp.focus({ direction = "up" }))
hl.bind(mainMod .. " + down",  hl.dsp.focus({ direction = "down" }))

-- Switch workspaces with mainMod + [0-9]
-- Move active window to a workspace with mainMod + SHIFT + [0-9]
for i = 1, 10 do
    local key = i % 10 -- 10 maps to key 0
    hl.bind(mainMod .. " + " .. key,         hl.dsp.focus({ workspace = i }))
    hl.bind(mainMod .. " + SHIFT + " .. key, hl.dsp.window.move({ workspace = i }))
end

-- Example special workspace (scratchpad)
hl.bind(mainMod .. " + S",         hl.dsp.workspace.toggle_special("magic"))
hl.bind(mainMod .. " + SHIFT + S", hl.dsp.window.move({ workspace = "special:magic" }))

-- Scroll through existing workspaces with mainMod + scroll
hl.bind(mainMod .. " + mouse_down", hl.dsp.focus({ workspace = "e+1" }))
hl.bind(mainMod .. " + mouse_up",   hl.dsp.focus({ workspace = "e-1" }))

-- Move/resize windows with mainMod + LMB/RMB and dragging
hl.bind(mainMod .. " + mouse:272", hl.dsp.window.drag(),   { mouse = true })
hl.bind(mainMod .. " + mouse:273", hl.dsp.window.resize(), { mouse = true })

-- Laptop multimedia keys for volume and LCD brightness
hl.bind("XF86AudioRaiseVolume",  hl.dsp.exec_cmd("wpctl set-volume -l 1 @DEFAULT_AUDIO_SINK@ 5%+"), { locked = true, repeating = true })
hl.bind("XF86AudioLowerVolume",  hl.dsp.exec_cmd("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-"),      { locked = true, repeating = true })
hl.bind("XF86AudioMute",         hl.dsp.exec_cmd("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle"),     { locked = true, repeating = true })
hl.bind("XF86AudioMicMute",      hl.dsp.exec_cmd("wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle"),   { locked = true, repeating = true })
hl.bind("XF86MonBrightnessUp",   hl.dsp.exec_cmd("brightnessctl -e4 -n2 set 5%+"),                  { locked = true, repeating = true })
hl.bind("XF86MonBrightnessDown", hl.dsp.exec_cmd("brightnessctl -e4 -n2 set 5%-"),                  { locked = true, repeating = true })

-- Requires playerctl
hl.bind("XF86AudioNext",  hl.dsp.exec_cmd("playerctl next"),       { locked = true })
hl.bind("XF86AudioPause", hl.dsp.exec_cmd("playerctl play-pause"), { locked = true })
hl.bind("XF86AudioPlay",  hl.dsp.exec_cmd("playerctl play-pause"), { locked = true })
hl.bind("XF86AudioPrev",  hl.dsp.exec_cmd("playerctl previous"),   { locked = true })

-- Print
hl.bind("SHIFT + Print", hl.dsp.exec_cmd("hyprshot -m output -o Imagens/Capturas\\ de\\ tela/"))
hl.bind("Print",         hl.dsp.exec_cmd("hyprshot -m region -o Imagens/Capturas\\ de\\ tela/"))


--------------------------------
---- WINDOWS AND WORKSPACES ----
--------------------------------

hl.window_rule({
    -- Ignore maximize requests from all apps. You'll probably like this.
    name  = "suppress-maximize-events",
    match = { class = ".*" },

    suppress_event = "maximize",
})

hl.window_rule({
    -- Fix some dragging issues with XWayland
    name  = "fix-xwayland-drags",
    match = {
        class      = "^$",
        title      = "^$",
        xwayland   = true,
        float      = true,
        fullscreen = false,
        pin        = false,
    },

    no_focus = true,
})

-- Hyprland-run windowrule
hl.window_rule({
    name  = "move-hyprland-run",
    match = { class = "hyprland-run" },

    move  = "20 monitor_h-120",
    float = true,
})

-- Dolphin Emulator windowrule
hl.window_rule({
    name  = "dolphin-emu",
    match = { class = "^(dolphin-emu)$" },

    allows_input   = true,
    suppress_event = "activate",
})

-- SwayNC layerrules
hl.layer_rule({
    name         = "swaync-control-center",
    match        = { namespace = "swaync-control-center" },
    blur         = true,
    ignore_alpha = 0.3,
})

hl.layer_rule({
    name         = "swaync-notification-window",
    match        = { namespace = "swaync-notification-window" },
    blur         = true,
    ignore_alpha = 0.3,
})

hl.layer_rule({
    name         = "wofi-zelda-blur",
    match        = { namespace = "wofi" },
    blur         = true,
    ignore_alpha = 0.01,
})
