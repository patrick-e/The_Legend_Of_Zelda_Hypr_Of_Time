As seen in 𝕽𝖎𝖛𝖊𝖓𝖉𝖊𝖑𝖑, the winning submission to Hyprland's 4th ricing competition: https://codeberg.org/zacoons/rivendell-hyprdots

# Installing

See https://wiki.hypr.land/Plugins/Using-Plugins/

## Using hyprpm

```sh
hyprpm update
hyprpm add https://codeberg.org/zacoons/imgborders
hyprpm enable imgborders
```

## Using NixOS flake

```nix
{
  inputs = {
    hyprland.url = "github:hyprwm/Hyprland";

    imgborders = {
      url = "git+https://codeberg.org/zacoons/imgborders.git";
      inputs.hyprland.follows = "hyprland";
    };
  };

  outputs = { self, nixpkgs, hyprland, imgborders, ... }: {
    nixosConfigurations.HOSTNAME = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        hyprland.nixosModules.default
        {
          programs.hyprland = {
            enable = true;
            package = hyprland.packages.x86_64-linux.hyprland;
          };
        }
      ];
    };
  };
}
```

# Config'ing

```lua
function is_plugin_loaded(name)
	for _,v in pairs(hl.get_loaded_plugins()) do
		if v.name == name then return true end
	end
	return false
end

if is_plugin_loaded("imgborders") then
	hl.config({
		plugin = {
			imgborders = {
				image = "~/path/to/file",
				sizes = 1,2,3,4, -- top, right, bottom, left
				insets = 1,2,3,4, -- top, right, bottom, left
				scale = 1.2,
				smooth = true,
				blur = false,
			},
		},
	})
end
```

|property|type|description|default|
|-|-|-|-|
`image` | str | Path to a PNG image, or empty to disable imgborders. | `""`
`sizes` | css_gaps | Number of pixels to take from each edge of the image. | `0`
`insets` | css_gaps | Amount to inset each side into (or away from) the window. | `0`
`scale` | float | Scale the borders by some amount. | `1`
`smooth` | bool | Whether the image should be smoothed or pixelated. | `true`
`blur` | bool | Whether it should blur the background or not. | `false`

## Window rules

`imgborders:noimgborders` - Disables image borders.

# Contributing

```sh
git clone https://codeberg.org/zacoons/imgborders
```

## Debugging

For a quickstart, just run `./debug.sh`.

For more information, see https://wiki.hypr.land/Plugins/Development/Getting-Started/

Good luck!
