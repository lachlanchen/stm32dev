# H7 Optical Joint Scan Latch Synchronization

The preserved experimental controller is:

`firmware/stm32_sensor_head_lcd/src/h7_optical_joint_scan_latch_sync_main.c`

It keeps the established PA2/TIM5 tungsten PWM, PA3 SK6812 GRBW protocol, finite RAM table, CRC, sequence counter, and emergency-off behavior. The only optical-timing change is update order:

1. Transmit a changed SK6812 word.
2. Wait through the SK6812 reset/latch interval.
3. Update PA2 immediately after the LED output latches.
4. Complete the state dwell and continue.

This reduces the interval in which the event camera sees tungsten and LED changes separately. The original `h7_optical_joint_scan_controller_main.c` and its build remain unchanged.

Build and flash:

```powershell
make h7-optical-joint-scan-latch-sync
make flash-h7-optical-joint-scan-latch-sync
```

Output:

`build_h7_optical_joint_scan_latch_sync/stm32_h7_optical_joint_scan_latch_sync.elf`

The firmware starts and ends with PA2 off and SK6812 RGBW zero. Host experiments must still use finite tables and issue OFF in a `finally` path.
