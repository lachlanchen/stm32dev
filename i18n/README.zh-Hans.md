[English](../README.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Français](README.fr.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [中文 (简体)](README.zh-Hans.md) · [中文（繁體）](README.zh-Hant.md) · [Deutsch](README.de.md) · [Русский](README.ru.md)

[![LazyingArt banner](https://github.com/lachlanchen/lachlanchen/raw/main/figs/banner.png)](https://github.com/lachlanchen/lachlanchen/blob/main/figs/banner.png)

# stm32dev

*STM32H7 触摸屏开发工作区，包含 ST-Link/OpenOCD 脚本和持久 framebuffer 补丁源码。*

`stm32dev` 记录了一块 STM32H74/H75 触摸屏板子的真实调试过程：接线、主机环境、ST-Link 控制命令、flash 备份和恢复、最小 firmware，以及 RGB565 显示补丁的 assembly 源码。

这个公开仓库不上传 flash dump、生成的二进制文件、构建输出或本地下载的工具包。

## 内容

| 路径 | 用途 |
| --- | --- |
| `scripts/` | 探测、OpenOCD、flash 备份/恢复和串口监听脚本。 |
| `docs/wiring.md` | SWD、UART、电源和 Type-C 接线说明。 |
| `config/49-stm32-stlink-local.rules` | ST-Link 权限相关 udev 规则。 |
| `firmware/minimal/` | 用于验证 toolchain 的最小 firmware。 |
| `firmware/persistent_trio/` | 显示补丁源码和 linker script。 |
| `references/stm32-touch-board-session.md` | 会话记录、地址和验证结果。 |

## 快速开始

```sh
./scripts/probe.sh
./scripts/openocd.sh
./scripts/backup_flash.sh
make -C firmware/minimal
```

## 持久显示补丁

补丁使用 `0xC0000000` 处的 RGB565 framebuffer，并处理 Cortex-M7 D-cache，使 LTDC 能读取 CPU 写入的图像。

## 引用

如果在研究或文档中使用 `stm32dev`，请参考 [CITATION.cff](../CITATION.cff)。

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
