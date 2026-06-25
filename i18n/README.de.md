[English](../README.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Français](README.fr.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [中文 (简体)](README.zh-Hans.md) · [中文（繁體）](README.zh-Hant.md) · [Deutsch](README.de.md) · [Русский](README.ru.md)

[![LazyingArt banner](https://github.com/lachlanchen/lachlanchen/raw/main/figs/banner.png)](https://github.com/lachlanchen/lachlanchen/blob/main/figs/banner.png)

# stm32dev

*Arbeitsbereich für ein STM32H7-Touchscreen-Board mit ST-Link/OpenOCD-Skripten und persistentem Framebuffer-Patch.*

`stm32dev` dokumentiert eine reale Hardware-Sitzung mit einem STM32H74/H75-Touchboard: Verdrahtung, Host-Setup, ST-Link-Befehle, Flash-Backup und Restore, minimales Firmware-Gerüst und Assembly-Quellcode für einen persistenten RGB565-Anzeigepatch.

Dieses öffentliche Repository enthält keine Flash-Dumps, erzeugten Binärdateien, Build-Ausgaben oder lokal heruntergeladenen Tools.

## Inhalt

| Pfad | Zweck |
| --- | --- |
| `scripts/` | Probe, OpenOCD, Flash-Backup/Restore und serielles Mithören. |
| `docs/wiring.md` | Notizen zu SWD, UART, Stromversorgung und Type-C. |
| `config/49-stm32-stlink-local.rules` | udev-Regeln für ST-Link-Berechtigungen. |
| `firmware/minimal/` | Minimale Firmware zur Toolchain-Prüfung. |
| `firmware/persistent_trio/` | Quellcode des Display-Patches und Linker-Skripte. |
| `references/stm32-touch-board-session.md` | Sitzungsprotokoll, Adressen und Verifikation. |

## Schnellstart

```sh
./scripts/probe.sh
./scripts/openocd.sh
./scripts/backup_flash.sh
make -C firmware/minimal
```

## Persistenter Display-Patch

Der Patch nutzt den RGB565-Framebuffer bei `0xC0000000` und behandelt den Cortex-M7-D-cache, damit LTDC das von der CPU geschriebene Bild lesen kann.

## Zitieren

Wenn du `stm32dev` in Forschung oder Dokumentation verwendest, nutze [CITATION.cff](../CITATION.cff).

```bibtex
@software{chen_stm32dev_2026,
  author = {Chen, Lachlan},
  title = {stm32dev: STM32H7 Touch-Screen Board Development Workspace},
  year = {2026},
  url = {https://github.com/lachlanchen/stm32dev}
}
```

## Unterstützung

| Donate | PayPal | Stripe |
| --- | --- | --- |
| [![Donate](https://img.shields.io/badge/Donate-LazyingArt-0EA5E9?style=for-the-badge&logo=kofi&logoColor=white)](https://chat.lazying.art/donate) | [![PayPal](https://img.shields.io/badge/PayPal-RongzhouChen-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://paypal.me/RongzhouChen) | [![Stripe](https://img.shields.io/badge/Stripe-Donate-635BFF?style=for-the-badge&logo=stripe&logoColor=white)](https://buy.stripe.com/aFadR8gIaflgfQV6T4fw400) |
