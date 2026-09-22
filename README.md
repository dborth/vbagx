# Visual Boy Advance GX

[github.com/dborth/vbagx](https://github.com/dborth/vbagx) — GPL licensed

Visual Boy Advance GX is a Game Boy / Game Boy Color / Game Boy Advance emulator for the **Nintendo GameCube** and **Wii**, built on [VBA-M](https://github.com/visualboyadvance-m/visualboyadvance-m) and the shared [`libgui`](https://github.com/dborth/libgui) UI/driver framework.

Visual Boy Advance GX is homebrew — it isn't signed by Nintendo, so your console needs to be set up to run unsigned code first. If you haven't done that yet, jump to **[Installation](#installation)** below; it links to a step-by-step guide for whichever console you have.

## Table of Contents

- [Nightly Builds](#nightly-builds)
- [Features](#features)
- [Installation](#installation)
  - [All Platforms: SD Card & Folder Layout](#all-platforms-sd-card--folder-layout)
  - [Wii](#wii)
  - [GameCube](#gamecube)
  - [Wii U](#wii-u)
    - [Native Wii U (Aroma)](#native-wii-u-aroma)
    - [vWii (Wii Homebrew Channel, inside Wii U)](#vwii-wii-homebrew-channel-inside-wii-u)
    - [vWii via VC-style injection (GamePad support)](#vwii-via-vc-style-injection-gamepad-support)
- [Initial Setup](#initial-setup)
- [Configuration](#configuration)
  - [Button Mappings](#button-mappings)
  - [Video](#video)
  - [Emulation](#emulation)
  - [Saving & Loading](#saving--loading)
  - [Menu](#menu)
  - [Language & Custom Fonts](#language--custom-fonts)
  - [Artwork](#artwork)
- [File Browser](#file-browser)
- [Gameplay](#gameplay)
- [Cheats](#cheats)
- [Dynamic Recompilation (JIT)](#dynamic-recompilation-jit)
- [Super Game Boy Borders](#super-game-boy-borders)
- [Patches (IPS/UPS)](#patches-ipsups)
- [Special Wii Controls](#special-wii-controls)
- [Credits](#credits)
- [Links](#links)

> 📜 Looking for old version notes? They've moved to **[CHANGELOG.md](CHANGELOG.md)**.

---

## Nightly Builds

Every push builds automatically. Grab the latest continuous-integration build:

| Platform                   | Status                             | Download                                    |
|-----------------------------|-------------------------------------|-----------------------------------------------|
| Wii / vWii                  | [![Build Status][Build]][Actions]  | [![Download][Download]][vbagx-wii]            |
| GameCube                    | [![Build Status][Build]][Actions]  | [![Download][Download]][vbagx-gamecube]       |
| Wii U (native, `.wuhb`)     | [![Build Status][Build]][Actions]  | [![Download][Download]][vbagx-wiiu]           |

[Actions]: https://github.com/dborth/vbagx/actions/workflows/build.yml
[Build]: https://github.com/dborth/vbagx/actions/workflows/build.yml/badge.svg
[Download]: https://img.shields.io/badge/Download-blue
[vbagx-wii]: https://github.com/dborth/vbagx/releases/download/Pre-release/VisualBoyAdvanceGX.zip
[vbagx-gamecube]: https://github.com/dborth/vbagx/releases/download/Pre-release/VisualBoyAdvanceGX-GameCube.zip
[vbagx-wiiu]: https://github.com/dborth/vbagx/releases/download/Pre-release/VisualBoyAdvanceGX-WiiU.zip

> The Wii build also runs unmodified in **vWii** (the Wii U's built-in Wii-compatibility mode), including via VC-style injection for GamePad support. The Wii U build is a separate, **native** Wii U (Aroma) application — see [Wii U](#wii-u) below for how the three options compare.

---

## Features

- Custom-built dynamic recompiler (JIT) for GBA games, written from the ground up for VBA-GX — see [Dynamic Recompilation (JIT)](#dynamic-recompilation-jit)
- ARAM/SD hybrid virtual memory pager on GameCube — ROM data is backed by ARAM/SD completely transparently, which is what makes the JIT possible there
- Native 48kHz audio output with no upsampling, dropout-resistant buffering, and smooth fades instead of clicks
- Wiimote, Nunchuk, Wii Classic Controller, Wii U Pro Controller, and GameCube Controller support
- **Wii U GamePad** support — full touch + buttons on the **native Wii U build**; buttons/sticks and display (no touch) on **vWii via VC-style injection** — see [Wii U](#wii-u)
- Native Wii U build outputs up to **1080p**, with a **GX2 shader-based ScaleFX** upscaler built specifically for the Wii U's GPU
- Rotation sensor, solar sensor (Boktai), and rumble support — on Wii U, GamePad rumble uses a shorter, reduced-amplitude pattern than the Wiimote
- Special Wii-style motion/gesture controls built in for select games — see [Special Wii Controls](#special-wii-controls)
- SRAM and Snapshot (save state) saving
- Cheat code support (Libretro `.cht` format) for both GBA and GB/GBC games
- IPS/UPS patch support
- Fully customizable, per-controller button mappings, including "Match Wii Controls" per game
- SD, USB, DVD, and SMB network share support (native Wii U build supports FAT32/exFAT/NTFS USB via Mocha), plus ZIP/7z archive loading
- Turbo Mode, screen zoom, widescreen, unfiltered and 240p video options, fixed pixel ratio (1x/2x/3x)
- Upscaling filters — hq2x, Scale2x, 2xBR, DDT on GameCube/Wii; ScaleFX on Wii U — plus scanlines
- Native loading/saving of ROMs and SRAM from Goomba (a GB emulator for GBA)
- Super Game Boy border support, loaded from the game itself or from custom `.png` files
- Cover art / screenshot / artwork preview support
- Open source

---

## Installation

### All Platforms: SD Card & Folder Layout

However you load Visual Boy Advance GX, it looks for its files in a `vbagx` folder at the root of your storage device. Format your SD card as **FAT32** — it's the most reliable, best-tested option across both consoles, and the one this guide assumes throughout. USB drives, DVD, and SMB network shares are also supported (see [Saving & Loading](#saving--loading)), but SD is the simplest starting point.

Once you've placed the loader files for your console (below), also create these folders and drop your content in:

```text
SD:/vbagx/
├── roms/          ← your GB/GBC/GBA ROMs, or zipped/.7z
├── saves/         ← SRAM and Snapshot save states
├── cheats/        ← .cht cheat files (see Cheats)
├── borders/       ← custom Super Game Boy border .png files
├── screenshots/   ← in-game screenshots and/or screenshot preview images
├── covers/        ← cover art preview images
└── artwork/       ← artwork preview images
```

Only `roms/` needs anything in it to get started — the rest are created automatically the first time they're needed. You can point the emulator at different load/save folders later from [Saving & Loading](#saving--loading).

### Wii

1. Follow the **[Wii Homebrew Guide](https://wii.hacks.guide/)** if you haven't already installed the Homebrew Channel. This is a one-time setup per console.
2. Download the Wii build (`VisualBoyAdvanceGX.zip` above) and extract it to the root of your SD card. This adds two things:
   - `apps/vbagx/boot.dol` (plus its icon/meta files) — this is what the Homebrew Channel launches.
   - `vbagx/` — your ROMs and saves folder, per [above](#all-platforms-sd-card--folder-layout).
3. Insert the SD card, open the **Homebrew Channel**, and launch **Visual Boy Advance GX**.

Your SD card should look like this:

```text
SD:/
├── apps/
│   └── vbagx/
│       ├── boot.dol
│       ├── icon.png
│       └── meta.xml
└── vbagx/
    └── roms/
        └── ...
```

### GameCube

GameCube doesn't have anything like the Wii's Homebrew Channel sitting on the console itself — instead you boot a **loader**, a small piece of software that then launches your `.dol`. The de facto standard today is **[Swiss](https://github.com/emukidid/swiss-gc)**, a GameCube loader/multitool that can read `.dol` files straight off an SD card (via an SD Gecko or SD2SP2 adapter) and handles most other loading methods too. This README assumes Swiss.

Exactly how you get Swiss running (modchip, boot-disc exploit, Broadband Adapter, etc.) depends on your GameCube's hardware revision and what you already own — **[gc-forever.com](https://www.gc-forever.com/)** is the best community hub for GameCube homebrew and hardware guides matched to your exact setup; start there if you're not sure what applies to you.

Once Swiss is running, the **recommended setup is an SD Gecko (or SD2SP2) memory-card-slot adapter**, using the same FAT32 SD card approach as Wii/Wii U — by far the most reliable and lowest-latency option.

Visual Boy Advance GX also supports **GC Loader** and **DVD** (burned disc) loading, but be aware going in: both are noticeably rougher experiences than SD — GC Loader in particular has had more reported reliability issues in this port, and burned-disc loading is slow to start and inflexible to update. Use them only if SD Gecko/SD2SP2 genuinely isn't an option for your setup.

1. Set up Swiss (or another loader of your choice) for your GameCube — see [gc-forever.com](https://www.gc-forever.com/) for hardware-specific guides.
2. Download the GameCube build (`Snes9xGX-GameCube.zip` above) and extract it to the root of your SD card.
3. Boot Swiss, launch `snes9xgx-gc.dol` from your SD Gecko/SD2SP2, and you should land in the same file browser as the other platforms.

```text
SD:/
├── vbagx-gc.dol
└── vbagx/
    └── roms/
        └── ...
```

Note that GameCube does **not** use the `apps/` folder convention that Wii does — the `.dol` sits at the SD card root (or wherever your loader expects it), while your ROMs/saves still live in `vbagx/`, same as on Wii.

### Wii U

Wii U support comes in **three genuinely different forms** — pick the one that matches what you've set up on your console:

|                     | vWii (Homebrew Channel) | vWii (VC-style injection) | Native Wii U build |
|---------------------|--------------------------|-----------------------------|----------------------|
| What it is          | The regular **Wii** build, run inside vWii | The same Wii build, launched as its own injected Virtual-Console-style channel | A dedicated Wii U (Aroma) app, `.wuhb` |
| Requires            | Homebrew Channel *inside vWii* | Homebrew Channel *inside vWii*, plus a channel built with **[TeconMoon's WiiVC Injector Mod](https://github.com/timefox/TeconMoon-s-WiiVC-Injector-Mod)** | **Aroma** (Wii U homebrew environment) |
| GamePad             | Not usable | Usable as an **extra controller** (buttons/sticks; no touch) | **Full support** — touch, buttons |
| Output              | vWii-level, up to 480p | Same as plain vWii | Native, up to **1080p** |
| Upscaling filters   | hq2x, Scale2x, 2xBR, DDT | Same as plain vWii | GX2 shader-based **ScaleFX** |
| Which download      | `VisualBoyAdvanceGX.zip` (Wii build) | `VisualBoyAdvanceGX.zip` (Wii build) | `VisualBoyAdvanceGX-WiiU.zip` |

If you're not sure which you want: the **native build** is the strongest experience on a console with Aroma installed — full GamePad touch, 1080p, and GPU-based ScaleFX upscaling. **VC injection** is the best you'll get out of vWii itself (a usable GamePad, at Wii-level output), and plain **Homebrew Channel vWii** is the simplest but weakest of the three.

#### Native Wii U (Aroma)

1. Follow the **[Wii U Homebrew Guide](https://wiiu.hacks.guide/)** to install **Aroma** if you haven't already. One-time setup per console.
2. Download the Wii U build (`VisualBoyAdvanceGX-WiiU.zip` above) and copy `vbagx.wuhb` to `wiiu/apps/` on your SD card, alongside your other Aroma apps. Also add the `vbagx/` folder from the same download to the SD card root.
3. Insert the SD card and turn on your Wii U — with Aroma installed, **Visual Boy Advance GX shows up as its own icon directly on the Wii U Menu**, right alongside your other software. No separate app store or launcher step needed; just select it and go.

```text
SD:/
├── wiiu/
│   └── apps/
│       └── vbagx.wuhb
└── vbagx/
    └── roms/
        └── ...
```

Note the extra `wiiu/` nesting compared to Wii: the native Wii U app folder is kept separate from vWii's own `apps/` folder so the two can coexist on the same SD card without colliding.

> ⚠️ **Run the latest Aroma.** This build is only tested against, and only intended to work on, whatever the current Aroma release is at the time you're reading this. We can't promise it'll behave — or even boot — on an old Aroma build or an outdated Wii U system version. If something looks wrong, updating Aroma first is the right move before reporting it.

**Recommended companions, installed through the same Wii U Homebrew Guide:**

- **[Mocha](https://github.com/wiiu-env/MochaPayload)** — an Aroma component that gives Cafe OS access to USB storage (FAT32/exFAT/NTFS). Without it, USB drives simply won't show up as a load/save option on the native build; SD still works fine either way.
- **[Bloopair](https://github.com/GaryOderNichts/Bloopair)** — lets you pair non-Nintendo Bluetooth controllers (Switch Pro Controller, Joy-Con, DualShock/DualSense, Xbox controllers, and others) to your Wii U as if they were a Wii U Pro Controller. Handy if you don't have a GamePad or Pro Controller handy. Bloopair works at the system level within the native Wii U environment and doesn't apply inside vWii.

#### vWii (Wii Homebrew Channel, inside Wii U)

1. Follow the **[Wii Homebrew Guide](https://wii.hacks.guide/)** to install the Homebrew Channel in vWii — the process runs from inside the Wii U's Wii mode and is otherwise the same as on a standalone Wii.
2. Follow the [Wii instructions](#wii) above exactly, using the same SD card — the vWii build is the Wii build.
3. Boot into vWii on your Wii U (from the Wii U Menu) and launch it from the Homebrew Channel, same as on Wii.

This is the simplest Wii U path, but it's also the most limited one: no GamePad support at all. For GamePad support without going all the way to the native build, see VC-style injection below.

#### vWii via VC-style injection (GamePad support)

Rather than launching Visual Boy Advance GX from the Homebrew Channel every time, you can package it as its **own injected channel** using **[TeconMoon's WiiVC Injector Mod](https://github.com/timefox/TeconMoon-s-WiiVC-Injector-Mod)**. This installs it as a Virtual-Console-style title in your vWii NAND rather than something launched through the Homebrew Channel, which is what lets you use the Wii U GamePad for display (without touch) and as an extra controller.

At a high level:

1. Install the Homebrew Channel in vWii first (see [vWii](#vwii-wii-homebrew-channel-inside-wii-u) above) — you'll still want it for updates and other homebrew.
2. Download and run **TeconMoon's WiiVC Injector Mod** on a PC, and choose **Wii Homebrew Injection (DOL)** as the injection type.
3. Point it at Visual Boy Advance GX's `boot.dol` (from the Wii build), and pick one of the available **GamePad Emulation** modes so the injector configures GamePad input for the resulting channel.
4. Build the injected package and install it to your Wii U's vWii NAND with the tool of your choice (the injector's own documentation covers this step, since it depends on your existing vWii setup).
5. Keep the `vbagx/` ROMs/saves folder on your SD card exactly as described [above](#all-platforms-sd-card--folder-layout) — the injected channel reads from the SD card the same way the Homebrew Channel version does.

Consult the injector's own documentation/thread for anything version-specific — like forwarder tooling, this is third-party software this README doesn't track closely.

---

## Initial Setup

The first time you run Visual Boy Advance GX, it writes a new `settings.xml` next to the app (in `apps/vbagx/` on Wii, alongside `vbagx-gc.dol` on GameCube) to store your configuration. If you're upgrading from a previous version, the emulator may start with a message that your preferences have been reset — you'll need to set them again.

On launch, the emulator drops you into the ROM browser. Navigate with the D-Pad or the Wiimote pointer, and select with the **A** button; press **B** to swap between controlling the list and controlling the buttons. Press **Home** at the main menu to exit — what exactly that does is configurable, see [Menu](#menu). Click the logo to see the credits.

## Configuration

Press **A** on the **Settings** box from the main menu to open the settings screen. **Reset Settings** restores everything to defaults; **Go Back** returns to the ROM browser.

### Button Mappings

Configure the GBA/GB controller independently for each input device you have connected (GameCube Controller, Wiimote, Nunchuk+Wiimote, Classic Controller, Wii U Pro Controller). Different controls are used depending on what's plugged into the Wii Remote — Nunchuk means Nunchuk + Wii Remote. GameCube controllers can be used at the same time as Wii Remotes, controlling the same player. Press **Home** while configuring to cancel.

Game Boy and Game Boy Color games don't have L and R buttons — those only work in Game Boy Advance games.

Whatever controls you choose here can be overridden for certain games by turning on **Match Wii Controls** (or **Match GameCube Controls** on GameCube) with the appropriate expansion plugged in — see [Special Wii Controls](#special-wii-controls). Games without special Wii controls just use whatever you configured here.

In addition to the controls you configure, these always apply:

| Input | Action |
|---|---|
| Home, Escape | Opens the emulator's in-game menu |
| A+B, Spacebar, or right analog stick | Fast forward |
| Right analog stick | Zoom (if enabled) |
| + / - | Game Boy Start / Select |

### Video

| Setting | Options |
|---|---|
| **Video Mode** | Several output modes are available; pick whichever matches your display |
| **Aspect Ratio / Zoom** | Separate horizontal and vertical zoom, with separate settings for GB and GBA |
| **Fixed Pixel Ratio** | 1x, 2x, or 3x — overrides zoom and aspect ratio settings for a crisp integer-scaled image |
| **Filtering** | Unfiltered, Sharp, or Soft |
| **Upscaling** | GameCube/Wii: hq2x, Scale2x, 2xBR, DDT · Wii U: ScaleFX (GX2 shader-based) |
| **Scanline Overlay** | On/Off |
| **Screen Position** | Nudge the output if it isn't centered on your display |

### Emulation

| Option | Notes |
|---|---|
| **GBA Dynamic Recompilation** | See [Dynamic Recompilation (JIT)](#dynamic-recompilation-jit) |
| **GB Screen Palette** | Green or Monochrome — the classic Game Boy screen tint |
| **GB Hardware** | GB, SGB, GBC, or Auto — forces Super Game Boy mode when set to SGB, even for Game Boy Color games |
| **Super Game Boy Borders** | See [Super Game Boy Borders](#super-game-boy-borders) |
| **Auto Frame Skip** | Recommended to leave on, even with the JIT enabled — it also helps keep audio timing steady on the heaviest games |
| **Match Wii/GameCube Controls** | See [Special Wii Controls](#special-wii-controls) |

### Saving & Loading

| Option | Options |
|---|---|
| **Load Method** | SD, USB, DVD, Network, Auto |
| **Load Folder** | Opens an on-screen keyboard to set a custom ROM folder |
| **Save Method** | SD, USB, Network, Auto |
| **Save Folder** | Opens an on-screen keyboard to set a custom save folder |

Visual Boy Advance GX has two kinds of saves: **SRAM**, the in-game battery save you'd get on real hardware, and **Snapshots**, real-time save states that capture exactly where you are. Loading a Snapshot may overwrite your "SRAM (Auto)", so be careful. SRAM saved by VBA-M on other platforms (Mac/PC/Linux) can be imported directly, and vice versa — just make sure the `.srm` filename matches your ROM's filename.

### Menu

| Option | Options |
|---|---|
| **Exit Action** | Configurable — controls what pressing Home at the main menu does |
| **Music Volume** / **Sound Effects Volume** | |
| **Language** | See [Language & Custom Fonts](#language--custom-fonts) |
| **Preview Image** | See [Artwork](#artwork) |
| **Rumble** | Enabled/Disabled |

### Language & Custom Fonts

For Japanese or Korean, supply a matching font file yourself — `jp.ttf` or `ko.ttf` — placed in your app folder (`apps/vbagx/` on Wii, alongside `vbagx-gc.dol` on GameCube). Once the font file is in place, select that language from **Settings → Menu → Language** and it switches fonts automatically.

You can also customize the menu's background music by dropping a `bg_music.ogg` into the same app folder.

### Artwork

Cover art, screenshots, or general artwork can be shown on the main menu when a game is highlighted. Pick which one to display under **Settings → Menu → Preview Image**. Each image lives in its matching folder (`vbagx/covers`, `vbagx/screenshots`, `vbagx/artwork`) and must be a PNG named exactly the same as the ROM (e.g. `Pokemon Emerald.png` for `Pokemon Emerald.gba`).

---

## File Browser

The File Browser loads automatically on startup and lists the contents of your `vbagx/roms` folder (or wherever you've pointed Load Folder — see [Saving & Loading](#saving--loading)). Click a game — uncompressed, or zipped in a `.zip`/`.7z` archive — to load it. When choosing a file, use left/right to page up or down. ROMs in a `.zip` must be the first file in the archive, or you'll get an error.

## Gameplay

Once you choose a game, it starts. Press **Home** to return to the in-game menu, where you can save, load, reset, change controllers, or change settings — these apply to all games, not just the current one. If you're playing a Boktai game with the solar sensor active, a fifth button lets you set the in-game weather; sunlight is based on the weather, time of day, and the angle of your Wiimote. Note that if it's night time for real, there won't be any sun regardless of what you set the weather to.

- **Close** resumes play; **Main Menu** returns to the File Browser.

## Cheats

Visual Boy Advance GX supports cheat codes for both GBA and GB/GBC games, loaded from Libretro-format `.cht` files. Cheat files must be named to match the ROM (e.g. `Pokemon Emerald.gba` needs `Pokemon Emerald.cht`) and placed in `vbagx/cheats`. A `.cht` file is a simple text file with one description/code pair per cheat, numbered starting from 0:

```
cheat0_desc = "Infinite Health"
cheat0_code = "0203AD4C 00000063"
cheat1_desc = "Infinite Money"
cheat1_code = "83007CFC 270F"
```

Quotes around the value are optional. The description is what's shown in the in-game Cheats menu; if left out, the cheat is labelled "Unnamed Cheat". Once a matching `.cht` file is found, its cheats load automatically when the ROM starts — press **Home** during a game, then choose **Cheats** to see the list and toggle codes on or off. Toggling takes effect immediately, no reset needed.

Code formats are recognized automatically, so you don't need to tell Visual Boy Advance GX which kind you're entering:

| Platform | Recognized formats |
|---|---|
| GBA | CodeBreaker (12 hex digits), GameShark GBA v3 (16 hex digits) |
| GB/GBC | Game Boy Game Genie (`XXX-YYY` or `XXX-YYY-ZZZ`), Game Boy GameShark (8 hex digits) |

Spaces and hyphens inside a code are ignored, and hex digits are case-insensitive, so codes can be pasted in from most sources without reformatting. A single cheat entry can chain multiple codes together by joining them with `+` on the same `_code` line (some CodeBreaker/GameShark cheats need more than one line to work).

## Dynamic Recompilation (JIT)

Visual Boy Advance GX includes a dynamic recompiler (JIT) for GBA games, built entirely from scratch for VBA-GX. Rather than interpreting GBA code one instruction at a time, it translates hot game code directly into native Broadway/Gekko CPU instructions, while carefully preserving the timing accuracy of the original interpreter core. The result is a major speed boost with excellent compatibility — even demanding GBA titles run with plenty of headroom above a full, locked 60fps, rather than merely scraping by.

Turn it on or off from **Settings → Emulation → GBA Dynamic Recompilation**. On Wii it's on solid ground and safe to leave on by default. On GameCube it's newer, made possible by the ARAM/SD hybrid ROM pager, and is offered as an option rather than the default while it gets more mileage across a wider range of games and ROM sizes.

Dynamic Recompilation is very stable, but if you notice graphical glitches or other unexpected behavior in a specific game, try turning it off to see whether the issue goes away with the standard interpreter core.

## Super Game Boy Borders

Borders can be loaded from two places: PNG files in your `vbagx/borders` folder, or from the game itself when running in Super Game Boy mode (**Settings → Emulation** → border set to "From game (SGB only)"; you can also force SGB mode even for Game Boy Color games from the same menu).

If the `borders/` folder exists but no border for the current game is present there yet, the border loaded from the game will automatically be written out to a `.png` — so after running a game once in SGB mode, you can reuse that same border in GBC mode. Set the border option to "From .png file" to load borders exclusively from the folder; PNGs can be up to 640×480 and work for both GB(C) and GBA games.

The PNG filename must be `[TITLE].png`, where `[TITLE]` is the ROM's internal title (at offset `0x134` for GB games, `0xA0` for GBA games) — for example, `POKEMON_SFXAAXE.png` for Pokémon Silver. If no file by that exact name exists, Visual Boy Advance GX falls back to `default.png` (GB) or `defaultgba.png` (GBA).

Borders render along with the game's video output, so border pixels are the same size as game pixels: a Game Boy game appears in the middle 160×144 of the border, and a Game Boy Advance game in the middle 240×160, regardless of the border image's actual resolution.

## Patches (IPS/UPS)

Patches — for colourizing a monochrome Game Boy game, translating it, or removing a special-hardware requirement — are widely available online in IPS or UPS format; you don't need to patch anything yourself. Drop the `.ips`/`.ups` file in `vbagx/roms` alongside the ROM, named exactly the same as the ROM (patches can't be applied to a ROM inside a `.zip`, so check the archive for the real filename first).

Colourized/translated games may still show minor distortion, though it's improved over previous versions. Some unpatched monochrome games also have built-in color palettes and will appear in colour without any patch at all.

> ⚠️ **Don't use patched Boktai ROMs** (except the Boktai 3 translation patch, which is fine) — the real game relies on a solar sensor that VBA GX supports natively, and old-emulator compatibility patches will break that support. The same goes for WarioWare Twisted, Kirby's Tilt 'n' Tumble, and Yoshi's Universal Gravitation (Topsy Turvy): use the original ROMs, which are already fully supported.

## Special Wii Controls

Turn **Match Wii Controls** (or **Match GameCube Controls**) on in [Button Mappings](#button-mappings) to use motion/gesture controls modeled after each game's Wii/GameCube counterpart, with a Wii Remote, Nunchuk, Classic Controller, or GameCube Controller plugged in. Games without an entry here just use your regular configured controls.

| Games | Modeled after |
|---|---|
| The Legend of Zelda, Zelda II, A Link to the Past, Link's Awakening (DX), Oracle of Ages, Oracle of Seasons, Minish Cap | *Twilight Princess* |
| Super Mario Bros., Super Mario Bros. DX, Super Mario 2, Super Mario (2) Advance, Super Mario 3, Super Mario World, Yoshi's Island, Yoshi's Universal Gravitation (Topsy Turvy) | *Super Mario Galaxy* |
| Mario Kart | *Mario Kart Wii* (rough fit — doesn't work especially well) |
| Metroid Zero Mission, Metroid 1, Metroid 2, Metroid Fusion | *Metroid Prime 3: Corruption* |
| TMNT, Teenage Mutant Ninja Turtles, Fall of the Foot Clan, Back from the Sewers, Radical Rescue | *TMNT* (Wii/GameCube); Classic Controller uses the PS2 control scheme instead |
| Mortal Kombat, MK II, MK 3, MK 4, MK Advance, MK: Deadly Alliance, MK: Tournament Edition | *Mortal Kombat: Armageddon* |
| Lego Star Wars: The Video Game, Lego Star Wars: The Original Trilogy | *Lego Star Wars: The Complete Saga* |
| Harry Potter 1–5 (and GBC versions) | *Harry Potter and the Order of the Phoenix* (spell gestures not implemented) |
| Medal of Honor: Underground, Medal of Honor: Infiltrator | Various Medal of Honor Wii games/modes |
| One Piece | *One Piece: Unlimited Adventure* (Wii) / *One Piece: Grand Adventure* (GameCube) |
| Boktai 1–3, Kirby's Tilt 'n' Tumble, WarioWare: Twisted | Custom controls designed specifically for VBA-GX (see below) |

<details>
<summary><strong>Zelda</strong></summary>

**Wii Remote:** Swing to draw/swing your sword (A puts it away; the two-handed sword must be drawn from the items menu). Shake the Nunchuk for a spin attack. Z to Z-target and raise your shield (or Gust Jar, if equipped). A performs contextual actions (roll, talk, pick up, push, shrink/grow) and also sheathes your weapon; in Zelda II, A jumps. C fast-forwards. B uses the selected item, with three more mapped to D-Pad Left/Down/Right (swappable); in Minish Cap the D-Pad uses items directly instead of swapping. D-Pad Up talks to Midna/your hat, or opens the save screen (Link's Awakening) / secondary items (Oracle games). 1 = Map, - = Items, + = Quest Status. In Link's Awakening, select Bombs on the Items screen and press Z to toggle Bomb Arrows (equip the bow first).

**GameCube Controller:** B draws/swings your sword (hold for spin attack), A sheathes it. L Trigger targets and raises your shield/Gust Jar. A performs contextual actions. R Trigger pulls/lifts objects (auto-equips bracelet/gloves) — unique to this controller. Right stick fast-forwards. X/Y use your two equipped items (share the B slot, except Minish Cap where one is in slot A). D-Pad Right = Map, D-Pad Up = Items, Start = Quest Status, Z = talk to Midna/hat.

**Classic Controller:** Same sword/shield/action layout as GameCube (B/L/A), with ZL for fast-forward. R uses the selected item, with three more on the right stick directions (also mirrored to ZR/Y/X). + = subscreens, - = map/subscreens, stick-up talks to Midna/your hat.

</details>

<details>
<summary><strong>Mario &amp; Yoshi games</strong></summary>

**Wii Remote:** Shake to spin-attack or shoot fireballs as Fire Mario (some games use B for fireballs instead); shake to dismount Yoshi too. Move the stick a little to walk, a lot to run. A = jump, B = shoot/run/hold on/Yoshi's tongue, Z = crouch or lay an egg (press in the air to butt-stomp), C = hold to look around, D-Pad = look around or walk, + = pause, 1 = throw egg as Yoshi.

**Classic Controller:** B = jump, A = spin attack, X/Y = shoot/run/hold on/tongue, ZL (sometimes L) = crouch/lay egg/butt-stomp, L/R = look around (where supported), ZR = fast-forward (8-bit Game Boy only), + = pause. In Super Mario World and Super Mario Land 2, A or R also perform a spin jump.

Yoshi's Universal Gravitation (Topsy Turvy) uses the same scheme, except tilting the Wii Remote tilts the whole world and how you move within it.

</details>

<details>
<summary><strong>Metroid</strong></summary>

Point the Wii Remote up/down to aim. Flick it up while in Morph Ball to spring-jump. A = shoot, B = jump, D-Pad Down = missile, C = toggle Morph Ball, - = start, + = toggle super missiles, 1 = map, 2 = hint.

</details>

<details>
<summary><strong>TMNT</strong></summary>

**Wii Remote:** Shake to attack, pick up a weapon, or throw one away mid-air. Shake the Nunchuk for a spin kick. A = jump, B = swap turtle / charge attack (hold up + B for the family super move), C = roll, Z = special move.

**Classic Controller:** Uses the PlayStation control scheme instead.

</details>

<details>
<summary><strong>Boktai (custom controls)</strong></summary>

Not based on any other game — designed specifically for VBA-GX's solar sensor emulation, and identical with or without a Nunchuk attached. Point the Wii Remote at the sky to quickly charge your Gun del Sol; point it at the ground to block sunlight and prevent overheating, or hold it normally for in-between. Press Home during play to set the in-game weather — sunlight depends on weather, time of day, and Wiimote angle (max sun isn't actually best, since it rots fruit and overheats your gun; weather must be reset every session). Swing to swing your sword/weapon; D-Pad or Nunchuk stick walks; B fires the Gun del Sol; A reads signs/opens chests/talks; C/2 looks around or changes subscreen (R); +/- = start/select; Z/1 changes element or subscreen (L); 1 (with Nunchuk) fast-forwards.

</details>

<details>
<summary><strong>WarioWare: Twisted &amp; Kirby's Tilt 'n' Tumble</strong></summary>

**WarioWare: Twisted:** Rotate the Wii Remote to rotate. Hold Z to lock the current menu item. A = select, B = cancel, + = start.

**Kirby's Tilt 'n' Tumble:** Tilt the Wii Remote to tilt the world; shake to flick Kirby and enemies into the air. A shoots you out of holes in the ground or off clouds.

</details>

<details>
<summary><strong>Mortal Kombat, Lego Star Wars, Harry Potter, Medal of Honor, One Piece, Kid Dracula</strong></summary>

**Mortal Kombat** (Nunchuk stick to move/jump): D-Pad Left/Up/Down/Right = Low Punch/High Punch/Low Kick/High Kick, Z = block, A = throw, C = change style/run/costume/character, + = pause, - = costume/character.

**Lego Star Wars:** Swing to swing your lightsaber, flick up to grapple. A = jump, B = shoot, Z = use the Force / build Lego, C = change character/talk, - = force power, + = start, 1/2 = fast-forward.

**Harry Potter** (spell gestures not yet implemented): Wave to cast a spell. Nunchuk stick walks; D-Pad changes subscreen/navigates menus; the Order of the Phoenix game needs the IR pointer to aim spells. A = interact/Jinx, B = wand/charm/cancel, Z = run/sneak, C = location name/flute/jump, - = map/tasks, + = pause/menu, 1/2 = change spells.

**Medal of Honor** (Nunchuk stick to move): swing up to reload. In Underground you aim/turn with the IR pointer like an FPS; Infiltrator doesn't use pointer aiming. B = shoot, - = use, + = pause/objectives/menu, 2 or D-Pad Up = reload, D-Pad Left/Right = change weapon, D-Pad Down = crouch, C = strafe, 1 = run.

**One Piece — Wii:** A = attack, B = jump, - = change character, + = pause, C (double-click, hold) = dash, Z = grab, 2 = fast-forward. **GameCube:** A = attack, X = attack up, Y = jump, B = grab, R = change character, Start = pause, L (double-click, hold) = dash, Z = grab, right stick = fast-forward.

**Kid Dracula** (no Wii release of this game exists — controls are original to VBA-GX): pressing the fire button always shoots a small fireball; hold to charge and use your currently selected item instead. Switching back to your previous item is automatic unless you've since switched again.
- *Wii Remote + Nunchuk:* A = jump, B = use item, Z = NOR weapon (fireball), C = BAT weapon (turn into bat, 5s), + = pause, - = switch item, 1/2 = fast-forward.
- *Classic Controller:* A/B = jump, Y = use item, X = NOR weapon, R/ZR = BAT weapon, + = pause, - = switch item, 1/2 = fast-forward.

</details>

---

## Credits

| Role | Credit |
|---|---|
| Coding & menu design | Daryl Borth (Tantric) |
| Additional coding | libertyernie, Carl Kenner, dancinninjac, cebolleto |
| Menu artwork | the3seashells |
| Menu sound | Peter de Man |
| VBA GameCube/Wii | SoftDev, emukidid |
| VBA-M | VBA-M Team |
| Visual Boy Advance | Forgotten |
| libogc / devkitPPC | shagkur & WinterMute |

And many others who have contributed over the years!

## Links

- [Visual Boy Advance GX Project Page](https://github.com/dborth/vbagx)
- [Wii Homebrew Guide](https://wii.hacks.guide/)
- [Wii U Homebrew Guide](https://wiiu.hacks.guide/)
- [gc-forever.com — GameCube homebrew/hardware hub](https://www.gc-forever.com/)
- [Swiss](https://github.com/emukidid/swiss-gc) — the recommended GameCube loader
- [TeconMoon's WiiVC Injector Mod](https://github.com/timefox/TeconMoon-s-WiiVC-Injector-Mod)
- [Mocha](https://github.com/wiiu-env/MochaPayload) — USB storage access for the native Wii U build
- [Bloopair](https://github.com/GaryOderNichts/Bloopair) — Bluetooth controller pairing for the native Wii U build
- [Change History (CHANGELOG.md)](CHANGELOG.md)
