# 🗡️ The Legend of Zelda: Hypr of Time

Tema completo para **Hyprland**, **Wofi**, **SwayNC** e **Waybar** inspirado na interface de **The Legend of Zelda: Ocarina of Time 3D** e **Breath of the Wild**.

---

## ✨ O que está incluso

### 1. 🖼️ Bordas Ornamentadas de Janela (`hypr/`)
- Texturas 9-slice em alta resolução (`zelda_border_ornate.png` e `zelda_border.png`) com arabescos nos 4 cantos, linhas duplas contínuas em marfim/ouro envelhecido e **um único ornamento centralizado** no meio de cada lateral da janela.
- Versão customizada do plugin [`imgborders`](hypr/plugins/imgborders/ImgBorder.cpp) com mapeamento UV centralizado (`0.5 - span * 0.5` até `0.5 + span * 0.5`) para garantir que o ornamento fique sempre no centro exato de qualquer janela, independentemente do tamanho.
- Suporte tanto para configuração em Lua (`hyprland.lua`) quanto tradicional (`hyprland.conf`).

### 2. 🎒 Menu de Aplicativos Wofi — *Ocarina of Time 3D Item Screen* (`wofi/`)
- Layout em coluna vertical centralizada com fundo escuro translúcido e `blur` de camada inteira no Hyprland.
- Slots quadrados (`72x72`) em bronze/marrom escuro com ornamentos nos cantos (`slot_normal.png`).
- Moldura branca brilhante no item selecionado com setas indicadoras verticais (`▲` / `▼`), ocultando automaticamente a seta superior no primeiro item (`slot_selected_first.png`) e a seta inferior no último (`slot_selected_last.png`).
- Nome do aplicativo exibido **fora do quadrado** (à direita, sobre um sublinhado dourado) exclusivamente quando o item está selecionado.
- **Interação nativa com mouse e sub-ações (`libwofi_hover.c`):**
  - Seleção instantânea ao passar o mouse (`hover`) e abertura com clique único.
  - Seta lateral expansível à esquerda dos aplicativos que possuem ações secundárias (ex.: *Nova janela*, *Nova janela anônima* no navegador).
  - Trava de rolagem horizontal (`GTK_POLICY_NEVER` + `hadjustment = 0`) para manter a coluna de ícones perfeitamente fixa durante buscas ou capturas de tela.

### 3. 🧚‍♀️ Sistema de Notificações da Navi (`swaync/` + `waybar/` + `hypr/scripts/`)
- **Caixa de Diálogo da Navi (`swaync/`):** Notificações estilizadas como a caixa de diálogo de *Ocarina of Time 3D*, com fundo azul-noturno translúcido, borda brilhante azul-fada, crachá de nome no topo e a clássica seta azul apontando para baixo (`navi-arrow.png`).
- **Indicador Interativo na Waybar (`waybar/`):** Ícone da Navi na barra superior (`navi-idle.png`) que brilha em estado de alerta (`navi-alert.png`) ao receber novas notificações ou passar o mouse.
- **Falas da Navi (`hypr/scripts/navi-say.sh` & `navi_frases.txt`):** Ao iniciar a sessão no Hyprland, a Navi cumprimenta você com dicas e referências de *Ocarina of Time* combinadas com o universo Linux/Hyprland.

---

## 📦 Dependências

No Arch Linux (e derivados):

```bash
sudo pacman -S hyprland wofi swaync waybar libnotify socat python gcc pkgconf gtk3
```

*(Opcional para a fonte temática usada no CSS)*:
- `ttf-codenewroman-nerd` (AUR) ou qualquer Nerd Font de sua preferência.

---

## 🚀 Instalação Rápida

Clone o repositório e execute o instalador automático (ele substitui os caminhos para o seu `$HOME`, compila o `libimgborders.so` e o `libwofi_hover.so`, e faz backup da sua configuração atual do Hyprland):

```bash
git clone https://github.com/patrick-e/The_Legend_Of_Zelda_Hypr_Of_Time.git
cd The_Legend_Of_Zelda_Hypr_Of_Time
chmod +x install.sh
./install.sh
```

---

## 🎮 Atalhos e Uso

- **`SUPER + Space`** — Abre o menu de itens/aplicativos do Wofi (`LD_PRELOAD=~/.config/wofi/libwofi_hover.so wofi --show drun`).
- **`Setas ↑ / ↓` ou `Mouse Hover`** — Navega pelos aplicativos no Wofi.
- **`Tab` / `Seta →` ou clique na seta `▸` à esquerda do ícone** — Expande os modos de inicialização do aplicativo.
- **`Clique na Navi (Waybar)`** — Abre a Central de Notificações da Navi (`swaync`).
- **`~/.config/hypr/scripts/navi-say.sh`** — Dispara uma fala aleatória da Navi a qualquer momento.

---

## 📁 Estrutura do Repositório

```text
The_Legend_Of_Zelda_Hypr_Of_Time/
├── install.sh                       # Instalador e compilador automático
├── hypr/
│   ├── hyprland.lua                 # Configuração Hyprland (Lua API)
│   ├── hyprland.conf                # Configuração Hyprland (Conf tradicional)
│   ├── assets/
│   │   ├── zelda_border_ornate.png  # Borda 9-slice com arabesco central
│   │   └── zelda_border.png         # Borda 9-slice minimalista (cantos + linha dupla)
│   ├── plugins/imgborders/          # Código-fonte C++ do plugin imgborders (UV centralizado)
│   └── scripts/
│       ├── navi-say.sh              # Gerador de notificações da Navi
│       ├── navi_frases.txt          # Frases temáticas da Navi
│       ├── hyprsunset-toggle.sh     # Alternador de filtro de luz azul
│       └── waybar-fullscreen.sh     # Oculta Waybar automaticamente em fullscreen
├── wofi/
│   ├── config                       # Configuração de geometria do Wofi
│   ├── style.css                    # Tema Ocarina of Time 3D
│   ├── libwofi_hover.c              # Hook GTK3 para hover-select e trava de scroll horizontal
│   └── assets/                      # Slots normais, selecionados e cabeçalho
├── swaync/
│   ├── config.json                  # Configuração da caixa de diálogo da Navi
│   ├── style.css                    # Estilo Ocarina of Time 3D para notificações
│   └── icons/                       # Seta inferior do diálogo da Navi
└── waybar/
    ├── config.jsonc                 # Barra superior com módulo custom/notification da Navi
    ├── style.css                    # Transições entre estados idle/alert da Navi
    ├── icons/                       # Sprites SVG/PNG da Navi (idle e alert)
    └── scripts/                     # Script de monitoramento do estado da Navi
```
