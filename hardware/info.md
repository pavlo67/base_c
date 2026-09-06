## GPIO

`gpio/gpio.h` declares the abstract `Gpio` class. The `hardware_gpio` CMake target selects the implementation for the platform. `hardware.h` contains the programmer-selected BCM pin numbers for automatic digital tests and existing probe constants.

GPIO API, platform selection, test behavior, and limitations are documented in [info_add_1788642380974.md](info_add_1788642380974.md).

Platform implementations, including their native library includes, are guarded by SYSTEM_IS_RPI4, SYSTEM_IS_RPI5, or SYSTEM_IS_DESKTOP. The gpio_contract_test target defines GPIO_DESKTOP_TEST to enable the desktop stub on any platform.

The optional `GpioMode::hardwarePwm` mode uses dedicated PWM channels. See [GPIO PWM documentation](gpio/pwm/PWM.md) for the API, pin/channel tables, Raspberry Pi setup, OFF semantics, errors, and software tests.

GPIO mode switching and unified test cleanup are documented in [info_add_1788702028059.md](info_add_1788702028059.md).
