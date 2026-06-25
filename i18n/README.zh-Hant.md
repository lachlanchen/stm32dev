[English](../README.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Français](README.fr.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [中文 (简体)](README.zh-Hans.md) · [中文（繁體）](README.zh-Hant.md) · [Deutsch](README.de.md) · [Русский](README.ru.md)

[![LazyingArt banner](https://github.com/lachlanchen/lachlanchen/raw/main/figs/banner.png)](https://github.com/lachlanchen/lachlanchen/blob/main/figs/banner.png)

# stm32dev

*STM32H7 觸控螢幕開發工作區，包含 ST-Link/OpenOCD 腳本與持久 framebuffer 補丁原始碼。*

`stm32dev` 記錄一塊 STM32H74/H75 觸控板的實際調試流程：接線、主機設定、ST-Link 控制命令、flash 備份與還原、最小 firmware，以及 RGB565 顯示補丁的 assembly 原始碼。

此公開倉庫不提交 flash dump、生成的二進位檔、建置輸出或本機下載工具。

## 內容

| 路徑 | 用途 |
| --- | --- |
| `scripts/` | 偵測、OpenOCD、flash 備份/還原與串口監聽腳本。 |
| `docs/wiring.md` | SWD、UART、電源與 Type-C 接線說明。 |
| `config/49-stm32-stlink-local.rules` | ST-Link 權限相關 udev 規則。 |
| `firmware/minimal/` | 用於驗證 toolchain 的最小 firmware。 |
| `firmware/persistent_trio/` | 顯示補丁原始碼與 linker script。 |
| `references/stm32-touch-board-session.md` | 會話記錄、位址與驗證結果。 |

## 快速開始

```sh
./scripts/probe.sh
./scripts/openocd.sh
./scripts/backup_flash.sh
make -C firmware/minimal
```

## 持久顯示補丁

補丁使用 `0xC0000000` 的 RGB565 framebuffer，並處理 Cortex-M7 D-cache，讓 LTDC 能讀取 CPU 寫入的影像。

## 引用

若在研究或文件中使用 `stm32dev`，請參考 [CITATION.cff](../CITATION.cff)。

```bibtex
@software{chen_stm32dev_2026,
  author = {Chen, Lachlan},
  title = {stm32dev: STM32H7 Touch-Screen Board Development Workspace},
  year = {2026},
  url = {https://github.com/lachlanchen/stm32dev}
}
```

## 支持

| Donate | PayPal | Stripe |
| --- | --- | --- |
| [![Donate](https://img.shields.io/badge/Donate-LazyingArt-0EA5E9?style=for-the-badge&logo=kofi&logoColor=white)](https://chat.lazying.art/donate) | [![PayPal](https://img.shields.io/badge/PayPal-RongzhouChen-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://paypal.me/RongzhouChen) | [![Stripe](https://img.shields.io/badge/Stripe-Donate-635BFF?style=for-the-badge&logo=stripe&logoColor=white)](https://buy.stripe.com/aFadR8gIaflgfQV6T4fw400) |
