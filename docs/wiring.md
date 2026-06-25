# Wiring Notes

The Type-C port that makes the touch screen work is the board power path. Keep it connected while using SWD.

## SWD Debug

```text
ST-Link GND    -> board GND
ST-Link SWDIO  -> board TMS
ST-Link SWCLK  -> board TCK
ST-Link NRST   -> board RST   optional, but useful
ST-Link 3.3V   -> board 3.3V  voltage reference/sense only
```

Do not use the ST-Link 3.3 V pin as the main power source for the touch screen board unless the board documentation says it is safe and the programmer can supply enough current.

## UART

```text
USB-TTL TX -> board RX
USB-TTL RX -> board TX
USB-TTL GND -> board GND
```

Use 3.3 V TTL only. Do not connect a 5 V UART adapter to the RX/TX pins.

## Why The Touch Screen Stops On Header-Only Power

The RX/TX/TMS/TCK/RST header is a debug and serial header. It usually does not power the LCD backlight, touch controller, and board power rails. If `RST` is held low, or SWD/UART pins are miswired, the MCU can also remain reset or fail to boot.
