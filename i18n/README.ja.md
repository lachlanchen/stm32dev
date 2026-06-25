[English](../README.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Français](README.fr.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [中文 (简体)](README.zh-Hans.md) · [中文（繁體）](README.zh-Hant.md) · [Deutsch](README.de.md) · [Русский](README.ru.md)

[![LazyingArt banner](https://github.com/lachlanchen/lachlanchen/raw/main/figs/banner.png)](https://github.com/lachlanchen/lachlanchen/blob/main/figs/banner.png)

# stm32dev

*STM32H7 タッチスクリーン基板の作業記録、ST-Link/OpenOCD スクリプト、永続 framebuffer パッチのソース。*

`stm32dev` は、STM32H74/H75 タッチ基板を実際に接続して調べた作業スペースです。配線、ホスト設定、ST-Link コマンド、flash のバックアップと復元、最小 firmware、RGB565 表示パッチの assembly ソースをまとめています。

この公開リポジトリには、flash dump、生成済みバイナリ、build 出力、ローカルに取得したツールは含めません。

## 内容

| パス | 目的 |
| --- | --- |
| `scripts/` | probe、OpenOCD、flash バックアップ/復元、シリアル監視。 |
| `docs/wiring.md` | SWD、UART、電源、Type-C のメモ。 |
| `config/49-stm32-stlink-local.rules` | ST-Link 権限用の udev ルール。 |
| `firmware/minimal/` | toolchain 検証用の最小 firmware。 |
| `firmware/persistent_trio/` | 表示パッチのソースと linker script。 |
| `references/stm32-touch-board-session.md` | セッションログ、アドレス、検証結果。 |

## クイックスタート

```sh
./scripts/probe.sh
./scripts/openocd.sh
./scripts/backup_flash.sh
make -C firmware/minimal
```

## 永続表示パッチ

パッチは `0xC0000000` の RGB565 framebuffer を使い、Cortex-M7 の D-cache を処理して LTDC が CPU の書き込んだ画像を読めるようにします。

## 引用

研究や文書で `stm32dev` を使う場合は [CITATION.cff](../CITATION.cff) を参照してください。

```bibtex
@software{chen_stm32dev_2026,
  author = {Chen, Lachlan},
  title = {stm32dev: STM32H7 Touch-Screen Board Development Workspace},
  year = {2026},
  url = {https://github.com/lachlanchen/stm32dev}
}
```

## サポート

| Donate | PayPal | Stripe |
| --- | --- | --- |
| [![Donate](https://img.shields.io/badge/Donate-LazyingArt-0EA5E9?style=for-the-badge&logo=kofi&logoColor=white)](https://chat.lazying.art/donate) | [![PayPal](https://img.shields.io/badge/PayPal-RongzhouChen-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://paypal.me/RongzhouChen) | [![Stripe](https://img.shields.io/badge/Stripe-Donate-635BFF?style=for-the-badge&logo=stripe&logoColor=white)](https://buy.stripe.com/aFadR8gIaflgfQV6T4fw400) |
