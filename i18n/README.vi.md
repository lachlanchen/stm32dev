[English](../README.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Français](README.fr.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [中文 (简体)](README.zh-Hans.md) · [中文（繁體）](README.zh-Hant.md) · [Deutsch](README.de.md) · [Русский](README.ru.md)

[![LazyingArt banner](https://github.com/lachlanchen/lachlanchen/raw/main/figs/banner.png)](https://github.com/lachlanchen/lachlanchen/blob/main/figs/banner.png)

# stm32dev

*Không gian làm việc cho bo mạch cảm ứng STM32H7, kèm script ST-Link/OpenOCD và mã nguồn bản vá framebuffer bền vững.*

`stm32dev` ghi lại một phiên làm việc thật với bo STM32H74/H75: đấu dây, thiết lập máy chủ, lệnh ST-Link, sao lưu/khôi phục flash, firmware tối giản và mã assembly cho bản vá hiển thị RGB565.

Kho công khai này không đưa lên các bản dump flash, file nhị phân sinh ra, kết quả build hoặc bộ công cụ tải cục bộ.

## Nội dung

| Đường dẫn | Mục đích |
| --- | --- |
| `scripts/` | Probe, OpenOCD, sao lưu/khôi phục flash và nghe serial. |
| `docs/wiring.md` | Ghi chú SWD, UART, nguồn và Type-C. |
| `config/49-stm32-stlink-local.rules` | Quy tắc udev cho quyền ST-Link. |
| `firmware/minimal/` | Firmware tối giản để kiểm tra toolchain. |
| `firmware/persistent_trio/` | Nguồn bản vá màn hình và linker script. |
| `references/stm32-touch-board-session.md` | Nhật ký phiên, địa chỉ và kết quả xác minh. |

## Bắt đầu nhanh

```sh
./scripts/probe.sh
./scripts/openocd.sh
./scripts/backup_flash.sh
make -C firmware/minimal
```

## Bản vá hiển thị bền vững

Bản vá dùng framebuffer RGB565 tại `0xC0000000` và xử lý D-cache của Cortex-M7 để LTDC đọc được ảnh do CPU ghi.

## Trích dẫn

Nếu dùng `stm32dev` trong nghiên cứu hoặc tài liệu, hãy tham khảo [CITATION.cff](../CITATION.cff).

```bibtex
@software{chen_stm32dev_2026,
  author = {Chen, Lachlan},
  title = {stm32dev: STM32H7 Touch-Screen Board Development Workspace},
  year = {2026},
  url = {https://github.com/lachlanchen/stm32dev}
}
```

## Ủng hộ

| Donate | PayPal | Stripe |
| --- | --- | --- |
| [![Donate](https://img.shields.io/badge/Donate-LazyingArt-0EA5E9?style=for-the-badge&logo=kofi&logoColor=white)](https://chat.lazying.art/donate) | [![PayPal](https://img.shields.io/badge/PayPal-RongzhouChen-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://paypal.me/RongzhouChen) | [![Stripe](https://img.shields.io/badge/Stripe-Donate-635BFF?style=for-the-badge&logo=stripe&logoColor=white)](https://buy.stripe.com/aFadR8gIaflgfQV6T4fw400) |
