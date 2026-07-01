BQ27545-G1 mock I2C slave for JBL Xtreme 2

Presents address 0x55 (7-bit) on I2C1 (SCL=PB6, SDA=PB7).
Returns "healthy battery" register values so the JBL MCU will boot
and run normally when the real battery/fuel gauge is absent.

Protocol: two separate transactions per register read
  1. S 0x55 W [A] <reg> [A] P   — master writes register address
  2. S 0x55 R [A] <lo> [A] <hi> [N] P — master reads 2-byte value

Quick Commands (S 0x55 R [A] P) are handled by the error/listen
callback — we ACK the address and the master sends STOP; this is
just a presence-check and requires no data.

USB CDC prints every transaction and a keep-alive every 5 s.
