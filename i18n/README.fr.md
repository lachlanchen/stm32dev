[English](../README.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Français](README.fr.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [中文 (简体)](README.zh-Hans.md) · [中文（繁體）](README.zh-Hant.md) · [Deutsch](README.de.md) · [Русский](README.ru.md)

[![LazyingArt banner](https://github.com/lachlanchen/lachlanchen/raw/main/figs/banner.png)](https://github.com/lachlanchen/lachlanchen/blob/main/figs/banner.png)

# stm32dev

*Espace de travail pour carte tactile STM32H7 avec scripts ST-Link/OpenOCD et source de patch framebuffer persistant.*

`stm32dev` documente une session matérielle réelle avec une carte tactile STM32H74/H75 : câblage, configuration hôte, commandes ST-Link, sauvegarde/restauration de flash, firmware minimal et source assembly pour un patch RGB565 persistant.

Ce dépôt public exclut les dumps de firmware, binaires générés, sorties de compilation et archives d'outils locales.

## Contenu

| Chemin | Rôle |
| --- | --- |
| `scripts/` | Probe, OpenOCD, sauvegarde/restauration flash et écoute série. |
| `docs/wiring.md` | Notes SWD, UART, alimentation et Type-C. |
| `config/49-stm32-stlink-local.rules` | Règles udev pour les permissions ST-Link. |
| `firmware/minimal/` | Firmware minimal pour valider la toolchain. |
| `firmware/persistent_trio/` | Source du patch d'affichage et scripts linker. |
| `references/stm32-touch-board-session.md` | Journal de session, adresses et vérifications. |

## Démarrage rapide

```sh
./scripts/probe.sh
./scripts/openocd.sh
./scripts/backup_flash.sh
make -C firmware/minimal
```

## Patch persistant

Le patch utilise le framebuffer RGB565 à `0xC0000000` et gère le D-cache du Cortex-M7 afin que LTDC voie l'image écrite par le CPU.

## Citation

Si vous utilisez `stm32dev` dans une recherche ou une documentation, citez [CITATION.cff](../CITATION.cff).

```bibtex
@software{chen_stm32dev_2026,
  author = {Chen, Lachlan},
  title = {stm32dev: STM32H7 Touch-Screen Board Development Workspace},
  year = {2026},
  url = {https://github.com/lachlanchen/stm32dev}
}
```

## Soutien

| Donate | PayPal | Stripe |
| --- | --- | --- |
| [![Donate](https://img.shields.io/badge/Donate-LazyingArt-0EA5E9?style=for-the-badge&logo=kofi&logoColor=white)](https://chat.lazying.art/donate) | [![PayPal](https://img.shields.io/badge/PayPal-RongzhouChen-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://paypal.me/RongzhouChen) | [![Stripe](https://img.shields.io/badge/Stripe-Donate-635BFF?style=for-the-badge&logo=stripe&logoColor=white)](https://buy.stripe.com/aFadR8gIaflgfQV6T4fw400) |
