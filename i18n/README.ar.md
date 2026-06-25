[English](../README.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Français](README.fr.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [中文 (简体)](README.zh-Hans.md) · [中文（繁體）](README.zh-Hant.md) · [Deutsch](README.de.md) · [Русский](README.ru.md)

[![LazyingArt banner](https://github.com/lachlanchen/lachlanchen/raw/main/figs/banner.png)](https://github.com/lachlanchen/lachlanchen/blob/main/figs/banner.png)

# stm32dev

*مساحة عمل للوحة لمس STM32H7، مع سكربتات ST-Link/OpenOCD ومصدر رقعة framebuffer دائمة.*

`stm32dev` يوثق إعداد لوحة لمس STM32H74/H75 متصلة فعليا: التوصيل، أدوات المضيف، أوامر ST-Link، نسخ/استرجاع الفلاش، هيكل firmware بسيط، ومصدر assembly لرقعة RGB565 دائمة.

هذا المستودع العام يستبعد ملفات الفلاش الثنائية ونتائج البناء والأدوات المحملة محليا.

## المحتويات

| المسار | الغرض |
| --- | --- |
| `scripts/` | أوامر الفحص، OpenOCD، النسخ الاحتياطي، الاسترجاع، الاستماع التسلسلي. |
| `docs/wiring.md` | ملاحظات SWD وUART والطاقة وType-C. |
| `config/49-stm32-stlink-local.rules` | قواعد udev لصلاحيات ST-Link ومنع طلب كلمة المرور. |
| `firmware/minimal/` | Firmware بسيط للتحقق من toolchain. |
| `firmware/persistent_trio/` | مصدر رقعة العرض وملفات linker. |
| `references/stm32-touch-board-session.md` | سجل الجلسة والعناوين والتحقق. |

## البدء السريع

```sh
./scripts/probe.sh
./scripts/openocd.sh
./scripts/backup_flash.sh
make -C firmware/minimal
```

## الرقعة الدائمة

تستخدم الرقعة framebuffer بصيغة RGB565 عند `0xC0000000`، وتتعامل مع D-cache في Cortex-M7 حتى يرى LTDC الصورة المكتوبة من المعالج.

## الاقتباس

إذا استخدمت `stm32dev` في بحث أو توثيق، استخدم [CITATION.cff](../CITATION.cff).

```bibtex
@software{chen_stm32dev_2026,
  author = {Chen, Lachlan},
  title = {stm32dev: STM32H7 Touch-Screen Board Development Workspace},
  year = {2026},
  url = {https://github.com/lachlanchen/stm32dev}
}
```

## الدعم

| Donate | PayPal | Stripe |
| --- | --- | --- |
| [![Donate](https://img.shields.io/badge/Donate-LazyingArt-0EA5E9?style=for-the-badge&logo=kofi&logoColor=white)](https://chat.lazying.art/donate) | [![PayPal](https://img.shields.io/badge/PayPal-RongzhouChen-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://paypal.me/RongzhouChen) | [![Stripe](https://img.shields.io/badge/Stripe-Donate-635BFF?style=for-the-badge&logo=stripe&logoColor=white)](https://buy.stripe.com/aFadR8gIaflgfQV6T4fw400) |
