# H7 PA2 Brief 12 V RMS-Equivalent Test

This is an independent, single-run safety target for the nominal 6 V tungsten
lamp on PA2 through YYNMOS-1. It does not replace any earlier firmware target.

## Programmed trajectory

- Supply assumption: `12.5 V`
- PWM carrier: `443 Hz`
- Target duty: `(12.0 / 12.5)^2 = 0.9216`
- 16-bit target: `60397 / 65535`
- Startup off: `1.0 s`
- Ramp up: `1.5 s`
- Peak hold: `0 s`
- Ramp down: `1.5 s`
- Final state: PA2 off forever; PA0, PA1, and PA3 remain off

For an approximately resistive load, this duty gives an electrical RMS value
near 12 V. A tungsten filament is nonlinear and its resistance rises strongly
with temperature, so this is not a calibrated optical or thermal operating
point.

## Safety limit

Applying 12 V RMS to a nominal 6 V lamp is approximately four times nominal
electrical power if hot resistance were unchanged. The firmware therefore
never holds the maximum, never loops, and immediately returns to zero. This
trajectory can still shorten filament life or break the lamp.

## Measured result (2026-08-03)

The programmed trajectory completed and left PA2 at 0% duty. The active third
INA219 channel (`0x42`) returned one turning-point sample:

- sampled current: `72 mA`
- sampled power: `552 mW`
- sampled bus voltage: `7.676 V` (PWM-aliased instantaneous reading)

The INA219 sample was not synchronized to the PWM edges. Therefore the bus
register must not be interpreted as the lamp RMS voltage. The current and power
are a relative confirmation only. Compared with the 10 V trial (`54 mA`,
`369 mW`), measured power rose by about 50%, which is consistent with the
higher drive. The lamp was not held at the turning point and remains off.
