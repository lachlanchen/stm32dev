[English](../README.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Français](README.fr.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [中文 (简体)](README.zh-Hans.md) · [中文（繁體）](README.zh-Hant.md) · [Deutsch](README.de.md) · [Русский](README.ru.md)

[![LazyingArt banner](https://github.com/lachlanchen/lachlanchen/raw/main/figs/banner.png)](https://github.com/lachlanchen/lachlanchen/blob/main/figs/banner.png)

# stm32dev

*Рабочее пространство для сенсорной платы STM32H7 со скриптами ST-Link/OpenOCD и исходником постоянного framebuffer-патча.*

`stm32dev` документирует реальную аппаратную сессию с платой STM32H74/H75: подключение, настройку хоста, команды ST-Link, резервное копирование и восстановление flash, минимальную firmware и assembly-код RGB565-патча дисплея.

Публичный репозиторий не содержит flash dump, сгенерированные бинарные файлы, результаты сборки и локально загруженные инструменты.

## Содержимое

| Путь | Назначение |
| --- | --- |
| `scripts/` | Probe, OpenOCD, backup/restore flash и serial listen. |
| `docs/wiring.md` | Заметки по SWD, UART, питанию и Type-C. |
| `config/49-stm32-stlink-local.rules` | udev-правила для доступа к ST-Link. |
| `firmware/minimal/` | Минимальная firmware для проверки toolchain. |
| `firmware/persistent_trio/` | Исходники display patch и linker scripts. |
| `references/stm32-touch-board-session.md` | Журнал сессии, адреса и проверка. |

## Быстрый старт

```sh
./scripts/probe.sh
./scripts/openocd.sh
./scripts/backup_flash.sh
make -C firmware/minimal
```

## Постоянный display patch

Патч использует RGB565 framebuffer по адресу `0xC0000000` и обслуживает D-cache Cortex-M7, чтобы LTDC видел изображение, записанное CPU.

## Цитирование

Если вы используете `stm32dev` в исследовании или документации, ссылайтесь на [CITATION.cff](../CITATION.cff).

```bibtex
@software{chen_stm32dev_2026,
  author = {Chen, Lachlan},
  title = {stm32dev: STM32H7 Touch-Screen Board Development Workspace},
  year = {2026},
  url = {https://github.com/lachlanchen/stm32dev}
}
```

## Поддержка

| Donate | PayPal | Stripe |
| --- | --- | --- |
| [![Donate](https://img.shields.io/badge/Donate-LazyingArt-0EA5E9?style=for-the-badge&logo=kofi&logoColor=white)](https://chat.lazying.art/donate) | [![PayPal](https://img.shields.io/badge/PayPal-RongzhouChen-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://paypal.me/RongzhouChen) | [![Stripe](https://img.shields.io/badge/Stripe-Donate-635BFF?style=for-the-badge&logo=stripe&logoColor=white)](https://buy.stripe.com/aFadR8gIaflgfQV6T4fw400) |
