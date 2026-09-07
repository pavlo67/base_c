# Hardware

`gpio/gpio.h` declares the abstract, non-copyable `Gpio` class. `hardware_gpio` selects the implementation at build time. `hardware.h` contains programmer-selected BCM test pins and probe constants. Stepper planning, timer simulation and motor execution are documented in [stepper_motor/info.md](stepper_motor/info.md).

## GPIO API

Public methods return zero on success and negative error codes on failure, except `read()` (digital zero/one) and `hardwarePwmChannel()` (channel number). Native error codes pass through; abstraction errors are named constants in gpio.h, starting at -10000. GPIO numbers are BCM0..27, not physical header positions. Callers choose free pins and serialize access. Lifecycle is shared and not reference-counted.

- `initialize()` opens the backend; repeated calls preserve state. `terminate()` stops enabled PWM, closes the backend and resets state after successful close. Repeated termination is allowed; concrete destructors also attempt cleanup.
- `setMode(pin, input/output/hardwarePwm)` configures a pin. Initial OUTPUT drives LOW. Repeating OUTPUT stops active software PWM and drives LOW while preserving range, duty and frequency; with no active PWM it preserves the digital level. Repeating hardwarePwm preserves its existing configuration. Changing modes stops PWM and releases the old mode. Failed reconfiguration leaves the pin unconfigured until a successful retry.
- `read()` returns a digital level; `write()` accepts zero/one and requires OUTPUT. Digital writes while software PWM is enabled return PWM_ACTIVE. Digital read/write are rejected in hardwarePwm mode.
- `setRange()` accepts 1..40000; `setDuty()` accepts 0..range. Reducing range below the current duty is rejected; changing range preserves the numeric duty, not its percentage.
- `setFrequency()` accepts 1..10000 Hz. Hardware may quantize frequency and duty; exact timing is not promised.
- `setEnabled()` starts/stops PWM and preserves settings. Software PWM OFF drives LOW, including repeated OFF. Zero/full duty are constant LOW/HIGH while logically enabled. Hardware PWM OFF holds zero duty with its controller enabled; releasing it does not promise a LOW pin.
- `getPwmSettings()` returns accepted settings, not waveform measurements. Defaults: range=100, duty=0, frequency=1000 Hz, enabled=false.

PWM setters accept OUTPUT or hardwarePwm. Settings changed while disabled produce no signal. Enabled settings apply to the backend before saving; invalid arguments preserve state, but multi-call native operations are not transactional on backend failure.

## Platforms and PWM

BASE_GPIO_PLATFORM in CMake selects AUTO, RPI4, RPI5 or DESKTOP. AUTO detects Raspberry Pi 4/5 from /proc/device-tree/model on native ARM, and selects desktop on non-ARM. ARM cross-builds and unrecognized ARM boards require explicit selection. Missing platform libraries fail configuration. Native includes and implementations are guarded by SYSTEM_IS_RPI4/RPI5/DESKTOP.

RPI4 requires pigpio. Software PWM uses gpioPWM with a native scale of 40000, converting public duty ratios; pigpio chooses its nearest frequency. Dedicated hardware PWM uses pigpio hardware PWM.

RPI5 requires lgpio and access to /dev/gpiochip*. The RP1 chip is located by its pinctrl-rp1 label. Lines are claimed/released through lgpio; software PWM uses lgTxPwm with percentage duty. Active changes can take effect at a cycle boundary; LOW/HIGH endpoints stop PWM before writing the level. Dedicated hardware PWM uses RP1 PWM0 through Linux sysfs, validating live Device Tree pin routing.

Desktop is an in-memory stub with independent pin state and the same mode/lifecycle validation. It simulates neither external inputs nor PWM edges: full duty reads HIGH, other enabled duty values read LOW. Hardware PWM simulates the RPI4 channel layout.

Hardware channels remain reserved while logically OFF. `PwmSysfs` owns only channels it exports; `PwmSysfsIo` is an injectable transport and `Rpi5PwmSysfsIo` supplies native I/O and discovery. `gpioHardwarePwmChannel()` maps pins for an explicit platform layout. Hardware-mode errors include NOT_SUPPORTED, CHANNEL_BUSY and PWM_NOT_CONFIGURED; native sysfs errors use negative errno. See [GPIO PWM documentation](gpio/pwm/PWM.md) for pin tables, setup and waveform limitations.

## Tests and probes

`gpio_test` uses the selected backend and the GpioTest fixture. Setup configures GPIO_TEST_PINS from hardware.h as OUTPUT. The digital inversion test saves levels after this configuration, writes the inverse, verifies it, then restores the saved levels. It does not restore pre-test pin modes or levels. Initialization/access errors fail tests. CTest runs this target serially.

Teardown attempts to set every test pin to INPUT and terminate the backend, continuing after cleanup errors. It includes hardware-mode pins and handles partial setup or earlier termination; failed initial initialization causes no pin changes. Cleanup failures are test failures.

Hardware conflict tests use GPIO12/18 on RPI4/desktop or GPIO14/18 on RPI5, plus GPIO13 and unsupported GPIO17. On Raspberry Pi these tests drive real pins and require hardware PWM routing/access. Repeated OUTPUT is tested both after PWM and with an existing digital HIGH. Desktop assertions validate API state, not physical waveforms.

`gpio_contract_test` is currently disabled in hardware/CMakeLists.txt; gpio_test includes digital, PWM, lifecycle and channel-mapping checks. `pwm_sysfs_test` uses temporary files without hardware. GPIO_SYSFS_TEST enables its sysfs implementation; normal native sysfs code remains guarded by SYSTEM_IS_RPI5.

`gpio_probe` blinks PIN_STEP, PIN_DIR and PIN_ENA; it does not run the old PWM/inversion demonstration. Configuration is in source/hardware.h, with no CLI parameters. Error output goes to stdout with ERROR:.

From this repository root:

```sh
cmake -S . -B cmake-build-release
cmake --build cmake-build-release --target gpio_test pwm_sysfs_test gpio_probe stepper_motor_test
ctest --test-dir cmake-build-release -R '^(gpio_test|pwm_sysfs_test|stepper_motor_test)$' --output-on-failure
```

The stepper_motor target publicly links hardware_gpio, making platform dependencies transitive to its consumers. Callers use Gpio/GpioMode instead of the removed global GPIO functions and PI_INPUT/PI_OUTPUT aliases.
