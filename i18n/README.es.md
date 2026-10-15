[English](../README.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Français](README.fr.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [中文 (简体)](README.zh-Hans.md) · [中文（繁體）](README.zh-Hant.md) · [Deutsch](README.de.md) · [Русский](README.ru.md)

[![LazyingArt banner — apps and four buddies](https://lazying.art/media/brand/lazyingart-intro.gif)](https://lazying.art)

# stm32dev

*Espacio de trabajo para una placa táctil STM32H7 con scripts ST-Link/OpenOCD y una fuente de parche persistente para framebuffer.*

`stm32dev` documenta una sesión real con una placa táctil STM32H74/H75: cableado, configuración del host, comandos ST-Link, copia y restauración de flash, firmware mínimo y el código assembly de un parche RGB565 persistente.

Este repositorio público excluye volcados de firmware, binarios generados, resultados de compilación y herramientas descargadas localmente.

## Contenido

| Ruta | Propósito |
| --- | --- |
| `scripts/` | Prueba, OpenOCD, copia/restauración de flash y escucha serie. |
| `docs/wiring.md` | Notas de SWD, UART, alimentación y Type-C. |
| `config/49-stm32-stlink-local.rules` | Reglas udev para permisos de ST-Link. |
| `firmware/minimal/` | Firmware mínimo para validar la toolchain. |
| `firmware/persistent_trio/` | Fuente del parche de pantalla y scripts de enlace. |
| `references/stm32-touch-board-session.md` | Registro de sesión, direcciones y verificación. |

## Inicio rápido

```sh
./scripts/probe.sh
./scripts/openocd.sh
./scripts/backup_flash.sh
make -C firmware/minimal
```

## Parche persistente

El parche trabaja con el framebuffer RGB565 en `0xC0000000` y mantiene la coherencia de D-cache del Cortex-M7 para que LTDC lea la imagen escrita por la CPU.

## Cita

Si usas `stm32dev` en investigación o documentación, cita [CITATION.cff](../CITATION.cff).

```bibtex
@software{chen_stm32dev_2026,
  author = {Chen, Lachlan},
  title = {stm32dev: STM32H7 Touch-Screen Board Development Workspace},
  year = {2026},
  url = {https://github.com/lachlanchen/stm32dev}
}
```

## Soporte

<!-- support:start -->
[![GitHub Sponsors](https://img.shields.io/badge/GitHub-Sponsors-EA4AAA?style=for-the-badge&logo=githubsponsors&logoColor=white)](https://github.com/sponsors/lachlanchen)
[![LazyingArt Support](https://img.shields.io/badge/LazyingArt-Support-0EA5E9?style=for-the-badge)](https://chat.lazying.art/donate)
[![PayPal Support](https://img.shields.io/badge/PayPal-Support-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://paypal.me/RongzhouChen)
[![Stripe Support](https://img.shields.io/badge/Stripe-Support-635BFF?style=for-the-badge&logo=stripe&logoColor=white)](https://buy.stripe.com/aFadR8gIaflgfQV6T4fw400)
<!-- support:end -->
