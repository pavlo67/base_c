# Hardware

`Gpio` in `gpio/gpio.h` is a shared, non-copyable backend selected by CMake's
`hardware_gpio`. Callers choose pins, serialize access, initialize before use and
terminate after all consumers stop. Lifecycle is not reference-counted.
[Stepper motors](stepper_motor/info.md) handle planning, execution and configuration.

## GPIO contract

Pins are BCM0..27, not physical header positions. Methods return zero/negative
error except digital `read()` and channel lookup; native errors pass through.
Initialization is repeatable without resetting live state; successful termination
stops PWM, closes the backend and resets state.

Changing modes stops/releases the previous mode; failed reconfiguration leaves
that pin unconfigured. First OUTPUT drives LOW. Repeated OUTPUT stops active PWM,
but preserves the digital level when PWM was already inactive. Repeated hardware
PWM mode preserves settings. Digital writes require OUTPUT with PWM disabled;
hardware PWM rejects digital reads/writes.

Range is 1..40000, duty 0..range, frequency 1..10000 Hz. A range change preserves
the numeric duty, not its percentage, and cannot shrink below it. Disabled settings
do not produce a signal. Invalid values preserve state; native multi-call failures
are not transactional. Reported settings are accepted requests, not measurements.

Software PWM OFF drives LOW; zero/full duty remain logically enabled at LOW/HIGH.
Hardware OFF holds zero duty while retaining the controller/channel; releasing it
does not guarantee LOW. Actual frequency/duty can be quantized.

## Backend selection

`BASE_GPIO_PLATFORM` accepts AUTO, RPI4, RPI5, "RPI CM4", "RPI CM5", DESKTOP.
AUTO detects Pi/CM on native ARM, otherwise desktop. Unrecognized ARM/cross-builds
need explicit selection. CM4 maps to RPI4, CM5 to RPI5. Missing native libraries
fail configuration; desktop does not simulate physical waveforms.

- RPI4: direct pigpio library; software gpioPWM and dedicated hardware PWM.
- RPI5: lgpio `/dev/gpiochip*` discovered by RP1 label; software lgTxPwm and RP1
  sysfs hardware PWM with Device Tree routing validation. Updates may land at a
  cycle boundary; zero/full duty switches to a constant line level.
- Desktop: in-memory pin state and RPI4 channel layout. Full duty reads HIGH,
  other enabled PWM reads LOW; no edges or external input are simulated.

`PwmSysfs` owns only channels it exports; shared hardware channels remain reserved
while OFF. Setup, pin mappings and waveform limitations:
[GPIO PWM](gpio/pwm/PWM.md). RPI4/CM4 needs the direct pigpio library, not just
client libraries. On Debian Trixie the recorded build used pigpio v79 from source;
installing that library does not require running its daemon.

## Configuration and checks

Hardware probes/tests use `loadPlatformMotorConfig()` through `stepper_platform`:
[complete two-axis schema](stepper_motor/info.md#platform-configuration).
Even pan-only probes validate both axes before GPIO. `HARDWARE_CONFIG_PATH` in
the relevant main selects the file; relative paths use the launch directory.
Configure actual wiring rather than relying on example pin assignments.

`gpio_test` drives the selected backend, runs serially and resets tested pins to
INPUT during cleanup; it does not restore pre-test modes/levels. PWM conflict cases
also use fixed pins (12/18 on RPI4/desktop, 14/18 on RPI5, plus 13 and 17).
Thus Raspberry Pi runs exercise real hardware and require routing/access permissions.
`pwm_sysfs_test` uses temporary files. `gpio_probe` blinks configured pan STEP/DIR/ENA
and cleans up. Desktop assertions establish API behavior, not waveform correctness.
