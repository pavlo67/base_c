# Hardware

`gpio/gpio.h` declares the abstract, non-copyable `Gpio` class. `hardware_gpio` selects the implementation at build time. `hardware.yaml` contains local BCM pins and hardware parameters; `hardware_example.yaml` is the two-axis template matching the hardware sections of `machina.yaml`. Stepper planning, timer simulation and motor execution are documented in [stepper_motor/info.md](stepper_motor/info.md).

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

BASE_GPIO_PLATFORM in CMake selects AUTO, RPI4, RPI5, "RPI CM4", "RPI CM5" or DESKTOP. AUTO detects Raspberry Pi 4/5 and Compute Module 4/5 from /proc/device-tree/model on native ARM, and selects desktop on non-ARM. ARM cross-builds and unrecognized ARM boards require explicit selection. Missing platform libraries fail configuration. Native includes and implementations are guarded by SYSTEM_IS_RPI4/RPI5/DESKTOP.

"RPI CM4" uses the RPI4 implementation (pigpio and SYSTEM_IS_RPI4); "RPI CM5" uses the RPI5 implementation (lgpio, RP1 sysfs PWM and SYSTEM_IS_RPI5). CMake reports both the selected board and its implementation. For explicit selection, quote the value: `cmake -S . -B cmake-build-release -DBASE_GPIO_PLATFORM="RPI CM4"` (or "RPI CM5"). AUTO remains the default. Hardware access and PWM routing requirements of the corresponding implementation still apply.

RPI4 requires pigpio. Software PWM uses gpioPWM with a native scale of 40000, converting public duty ratios; pigpio chooses its nearest frequency. Dedicated hardware PWM uses pigpio hardware PWM.

RPI5 requires lgpio and access to /dev/gpiochip*. The RP1 chip is located by its pinctrl-rp1 label. Lines are claimed/released through lgpio; software PWM uses lgTxPwm with percentage duty. Active changes can take effect at a cycle boundary; LOW/HIGH endpoints stop PWM before writing the level. Dedicated hardware PWM uses RP1 PWM0 through Linux sysfs, validating live Device Tree pin routing.

Desktop is an in-memory stub with independent pin state and the same mode/lifecycle validation. It simulates neither external inputs nor PWM edges: full duty reads HIGH, other enabled duty values read LOW. Hardware PWM simulates the RPI4 channel layout.

Hardware channels remain reserved while logically OFF. `PwmSysfs` owns only channels it exports; `PwmSysfsIo` is an injectable transport and `Rpi5PwmSysfsIo` supplies native I/O and discovery. `gpioHardwarePwmChannel()` maps pins for an explicit platform layout. Hardware-mode errors include NOT_SUPPORTED, CHANNEL_BUSY and PWM_NOT_CONFIGURED; native sysfs errors use negative errno. See [GPIO PWM documentation](gpio/pwm/PWM.md) for pin tables, setup and waveform limitations.

## Tests and probes

`gpio_test` uses the selected backend and the GpioTest fixture. Before running tests, its main loads the pan configuration; setup configures its STEP, DIR and ENA pins as OUTPUT. The digital inversion test saves levels after this configuration, writes the inverse, verifies it, then restores the saved levels. It does not restore pre-test pin modes or levels. Initialization/access errors fail tests. CTest runs this target serially.

Teardown attempts to set every test pin to INPUT and terminate the backend. Fatal cleanup assertions stop the remaining teardown on failure. It includes hardware-mode pins and handles partial setup or earlier termination; failed initial initialization causes no pin changes. Cleanup failures are test failures.

Hardware conflict tests use GPIO12/18 on RPI4/desktop or GPIO14/18 on RPI5, plus GPIO13 and unsupported GPIO17. On Raspberry Pi these tests drive real pins and require hardware PWM routing/access. Repeated OUTPUT is tested both after PWM and with an existing digital HIGH. Desktop assertions validate API state, not physical waveforms.

`gpio_contract_test` is currently disabled in hardware/CMakeLists.txt; gpio_test includes digital, PWM, lifecycle and channel-mapping checks. `pwm_sysfs_test` uses temporary files without hardware. GPIO_SYSFS_TEST enables its sysfs implementation; normal native sysfs code remains guarded by SYSTEM_IS_RPI5.

`gpio_probe` loads the pan configuration before GPIO initialization and blinks its STEP, DIR and ENA pins, five times each with 500-ms high/low intervals. It takes no CLI parameters. Each tested pin returns to INPUT; GPIO errors stop the probe with a nonzero exit status. Diagnostics go to stdout as `[main()] ERROR: ...`.

From this repository root:

```sh
cmake -S . -B cmake-build-release
cmake --build cmake-build-release --target gpio_test pwm_sysfs_test gpio_probe stepper_motor_test
ctest --test-dir cmake-build-release -R '^(gpio_test|pwm_sysfs_test|stepper_motor_test)$' --output-on-failure
```

The stepper_motor target publicly links hardware_gpio, making platform dependencies transitive to its consumers. Callers use Gpio/GpioMode instead of the removed global GPIO functions and PI_INPUT/PI_OUTPUT aliases.

## YAML hardware configuration

`loadPlatformMotorConfig()` from `stepper_motor/config/platform_config.h` is
shared by the probes and tests through the `stepper_platform` CMake target.
It loads and validates complete `pan` and `tilt` configurations atomically,
including mechanics, timing, PWM support and six distinct BCM pins in 0..27.
Both `hardware.yaml` and `machina.yaml` use this schema. Probes and GPIO tests
select the first configuration (`motors[0]`, pan) for their pins; `pulses_probe`
also uses its `pulseHigh_` duration. Invalid or missing settings on either axis
stop startup before GPIO initialization. There is no separate pan-only loader.
See [platform configuration](stepper_motor/info.md#platform-configuration) for
required fields and units.

`gpio_test`, `stepper_motor_test`, `gpio_probe` and `pulses_probe` each define
`HARDWARE_CONFIG_PATH` in their main source. Replace its value with a string path
when another configuration is required. Relative custom paths are relative to the 
process working directory. Files are read once at program startup, so editing YAML 
needs no rebuild; changing a source path constant needs a rebuild. A failed load 
exits before tests or hardware activity. Moving binaries to another machine requires 
setting a suitable path. There is no fallback to compiled pins.

The local `hardware.yaml` contains both axes: pan STEP=17, DIR=22, ENA=27 and
tilt STEP=18, DIR=23, ENA=26. Tilt ENA uses BCM26 to avoid pan DIR=22. The tracked
`hardware_example.yaml` contains pan 17/24/27 and tilt 18/23/22, with mechanics and
timing fields matching `machina.yaml`. Both YAML files are repository files; adjust `hardware.yaml` to the actual wiring.
The old hardware headers and obsolete header ignore rule have been removed.

Mathematical motor test inputs remain fixed test data. The primary desktop smart
motor tests use loaded pan pins; independent mock pins are selected outside that
set. Explicit hardware-PWM mapping/invalid-pin scenarios retain their fixed test
pins. GPIO hardware-channel tests likewise require specific platform pins.
`stepper_platform_test` covers the shared loader, mechanics and atomic rejection
of invalid reloads. Both supplied hardware YAML files are validated by that target.

## CM4 build setup on Debian Trixie

The local board reports `Raspberry Pi Compute Module 4 Rev 1.1` in
`/proc/device-tree/model`, with `aarch64` and Debian 13 (Trixie). Existing AUTO
detection selects `RPI CM4`, using the RPI4 backend. No distribution-specific
platform detection or explicit platform override is needed.

Initial configuration failed because Git was absent, followed by the missing
direct pigpio library. Install `git` and `libjsoncpp-dev` alongside CMake and the
C++ build tools. The configured Trixie package repositories provide pigpio client
libraries but not the direct library required by this backend.

The following installs only the header and direct library from pigpio v79
(commit `c33738a320a3e28824af7807edafda440952c05d`); it does not install or start
a daemon:

```sh
git clone --depth 1 --branch v79 https://github.com/joan2937/pigpio.git /tmp/pan-tilt-pigpio-v79
make -C /tmp/pan-tilt-pigpio-v79 -j2 libpigpio.so
sudo install -m 0644 /tmp/pan-tilt-pigpio-v79/pigpio.h /usr/local/include/pigpio.h
sudo install -m 0755 /tmp/pan-tilt-pigpio-v79/libpigpio.so.1 /usr/local/lib/libpigpio.so.1
sudo ln -s libpigpio.so.1 /usr/local/lib/libpigpio.so
sudo ldconfig
cmake -S . -B build
cmake --build build --parallel 2
```

These installation commands assume the checkout and library symlink do not
already exist. CMake finds the installed header and library under `/usr/local`.
This setup validates compilation and linking; physical GPIO/PWM operation is a
separate hardware check. `gpio_test` drives real pins on this platform.

### Historical validation on the CM4 machine

These results were recorded during the original CM4 setup, before the YAML probe
migration; they are not results from the current desktop validation.

The root project configured with AUTO and all default targets built successfully
with GCC 14.2.0. CTest passed `pwm_sysfs_test`, `stepper_platform_test`, `http_test`
and `server_http_mngs_test`; the HTTP tests require local socket access.

`stepper_motor_test` passed 17 of 19 GTest cases. Two numerical checks failed:
`clockedAccelerationSwitchesAtHalfActualAngle` observed final speed 0.025027
against a 0.01 tolerance, and
`cruiseUsesTimerBrakingBudgetAndPreservesIdealTriangle` observed acceleration
9.12298e-06 instead of exact zero. Their cause has not been established.

`machina_control_test` failed GPIO initialization when run as the regular user:
even its API-key-only configuration initializes the native backend. It was not
rerun as root. `gpio_test` was not executed because it drives physical pins.
These runtime limitations do not prevent compilation or linking.
