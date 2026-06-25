[English](../README.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Français](README.fr.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [中文 (简体)](README.zh-Hans.md) · [中文（繁體）](README.zh-Hant.md) · [Deutsch](README.de.md) · [Русский](README.ru.md)

[![LazyingArt banner](https://github.com/lachlanchen/lachlanchen/raw/main/figs/banner.png)](https://github.com/lachlanchen/lachlanchen/blob/main/figs/banner.png)

# stm32dev

*STM32H7 터치스크린 보드 작업 공간, ST-Link/OpenOCD 스크립트, 영구 framebuffer 패치 소스.*

`stm32dev`는 실제 STM32H74/H75 터치 보드를 연결해 정리한 작업 기록입니다. 배선, 호스트 설정, ST-Link 명령, flash 백업/복원, 최소 firmware, RGB565 표시 패치 assembly 소스를 포함합니다.

이 공개 저장소에는 flash dump, 생성된 바이너리, 빌드 결과물, 로컬 다운로드 도구를 포함하지 않습니다.

## 구성

| 경로 | 목적 |
| --- | --- |
| `scripts/` | probe, OpenOCD, flash 백업/복원, serial listen 도구. |
| `docs/wiring.md` | SWD, UART, 전원, Type-C 메모. |
| `config/49-stm32-stlink-local.rules` | ST-Link 권한용 udev 규칙. |
| `firmware/minimal/` | toolchain 검증용 최소 firmware. |
| `firmware/persistent_trio/` | 표시 패치 소스와 linker script. |
| `references/stm32-touch-board-session.md` | 세션 로그, 주소, 검증 결과. |

## 빠른 시작

```sh
./scripts/probe.sh
./scripts/openocd.sh
./scripts/backup_flash.sh
make -C firmware/minimal
```

## 영구 표시 패치

패치는 `0xC0000000`의 RGB565 framebuffer를 사용하며, LTDC가 CPU가 쓴 이미지를 읽을 수 있도록 Cortex-M7 D-cache를 처리합니다.

## 인용

연구나 문서에서 `stm32dev`를 사용한다면 [CITATION.cff](../CITATION.cff)를 참고하세요.

```bibtex
@software{chen_stm32dev_2026,
  author = {Chen, Lachlan},
  title = {stm32dev: STM32H7 Touch-Screen Board Development Workspace},
  year = {2026},
  url = {https://github.com/lachlanchen/stm32dev}
}
```

## 후원

| Donate | PayPal | Stripe |
| --- | --- | --- |
| [![Donate](https://img.shields.io/badge/Donate-LazyingArt-0EA5E9?style=for-the-badge&logo=kofi&logoColor=white)](https://chat.lazying.art/donate) | [![PayPal](https://img.shields.io/badge/PayPal-RongzhouChen-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://paypal.me/RongzhouChen) | [![Stripe](https://img.shields.io/badge/Stripe-Donate-635BFF?style=for-the-badge&logo=stripe&logoColor=white)](https://buy.stripe.com/aFadR8gIaflgfQV6T4fw400) |
