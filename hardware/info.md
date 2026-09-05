# Pan/tilt hardware interface and desktop stub

`PanTlt` is an abstract hardware interface for zeroing, absolute and relative movement, configuration load/save, and state display. It does not own or inherit file-backed `Config` state.

`PanTltStepper` initializes the GPIO backend once. Initialization failure is retained as state and reported through `Info` by movement operations instead of throwing an exception. Relative movement drives the pan and tilt step/direction pin pairs independently and updates the stored positions; absolute movement delegates to relative movement. GPIO uses the shared `Gpio` abstraction. Desktop builds use a stateful stub, so digital writes can be read back without hardware. Raspberry Pi 4 uses pigpio and Raspberry Pi 5 uses lgpio.

The minimal JSON configuration contains integer `pan` and `tilt` positions. Loading a non-object reports an error through `Info`; saving produces an object with both positions.

## GPIO

`gpio/gpio.h` declares the abstract `Gpio` class. The `hardware_gpio` CMake target selects the implementation for the platform. `hardware.h` contains the programmer-selected BCM pin numbers for automatic digital tests and existing probe constants.

GPIO API, platform selection, test behavior, and limitations are documented in [info_add_1788642380974.md](info_add_1788642380974.md).

Platform implementations, including their native library includes, are guarded by SYSTEM_IS_RPI4, SYSTEM_IS_RPI5, or SYSTEM_IS_DESKTOP. The gpio_contract_test target defines GPIO_DESKTOP_TEST to enable the desktop stub on any platform.

The optional `GpioMode::hardwarePwm` mode uses dedicated PWM channels. See [GPIO PWM documentation](gpio/pwm/PWM.md) for the API, pin/channel tables, Raspberry Pi setup, OFF semantics, errors, and software tests.
