# board/

The only layer allowed to include the NXP SDK (`fsl_*.h`): pin mux, clocks,
board init, thin wrappers over SDK peripherals (GPIO, I2C, timers).
Pin assignments must match `hardware/pinout.md`.
