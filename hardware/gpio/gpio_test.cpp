#include <gtest/gtest.h>

#include "gpio.h"
#include "hardware/hardware.h"

namespace {
    static_assert(GPIO_TEST_PINS.size() >= 2, "Contract tests require two configured pins");
    constexpr unsigned PIN   = GPIO_TEST_PINS[0];
    constexpr unsigned OTHER = GPIO_TEST_PINS[1];

    class GpioTest : public testing::Test {
    protected:
        Gpio& gpio_ = Gpio::instance();
        std::array<int, GPIO_TEST_PINS.size()> original_{};
        std::array<bool, Gpio::PIN_COUNT> cleanupPins_{};
        bool initialized_ = false;

        void SetUp() override {
            ASSERT_EQ(gpio_.initialize(), Gpio::SUCCESS);
            initialized_ = true;
            for (std::size_t i = 0; i < GPIO_TEST_PINS.size(); ++i) {
                const unsigned pin = GPIO_TEST_PINS[i];
                ASSERT_LT(pin, Gpio::PIN_COUNT);
                for (std::size_t j = 0; j < i; ++j) {
                    ASSERT_NE(pin, GPIO_TEST_PINS[j]) << "Duplicate GPIO_TEST_PINS entry";
                }
                cleanupPins_[pin] = true;
                ASSERT_EQ(gpio_.setMode(pin, GpioMode::output), Gpio::SUCCESS) << pin;
                const int level = gpio_.read(pin);
                ASSERT_GE(level, 0) << pin;
                ASSERT_LE(level, 1) << pin;
                original_[i] = level;
            }

            ASSERT_EQ(gpio_.write(OTHER, 0), 0);
        }

        void TearDown() override {
            if (!initialized_) {
                return;
            }

            // A lifecycle assertion may have left the backend terminated.
            const int initialized = gpio_.initialize();

            // Це страховка для тесту InitializationIsIdempotentAndTerminationResetsState: він викликає terminate(), а потім знову
            // initialize(). Якщо між ними спрацює ASSERT_*, тест достроково завершиться, залишивши backend закритим. Без повторної
            // ініціалізації TearDown() не зможе перевести піни в input.
            // initialized_ означає, що початкова ініціалізація в SetUp() вдалася; він не відстежує подальші виклики terminate().
            // Для решти тестів повторний initialize() нічого не змінює: він повертає успіх, якщо backend уже відкритий.

            ASSERT_EQ(initialized, Gpio::SUCCESS);
            for (unsigned pin = 0; pin < Gpio::PIN_COUNT; ++pin) {
                if (!cleanupPins_[pin]) { continue; }
                ASSERT_EQ(gpio_.setMode(pin, GpioMode::input), Gpio::SUCCESS) << pin;
            }
            ASSERT_EQ(gpio_.terminate(), Gpio::SUCCESS);
        }

    };

    TEST_F(GpioTest, InvertReadBackAndRestore) {
        for (std::size_t i = 0; i < GPIO_TEST_PINS.size(); ++i) {
            const unsigned pin = GPIO_TEST_PINS[i];
            ASSERT_EQ(gpio_.write(pin, 1 - original_[i]), Gpio::SUCCESS) << pin;
            ASSERT_EQ(gpio_.read(pin), 1 - original_[i]) << pin;
            ASSERT_EQ(gpio_.write(pin, original_[i]), Gpio::SUCCESS) << pin;
            ASSERT_EQ(gpio_.read(pin), original_[i]) << pin;
        }
    }

    TEST_F(GpioTest, DigitalPinsAreIndependent) {
        ASSERT_EQ(gpio_.write(PIN, 1), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.read(PIN), 1);
        ASSERT_EQ(gpio_.read(OTHER), 0);
        ASSERT_EQ(gpio_.write(OTHER, 1), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.write(PIN, 0), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.read(PIN), 0);
        ASSERT_EQ(gpio_.read(OTHER), 1);
    }

    TEST_F(GpioTest, InvalidOperationsDoNotChangeState) {
        ASSERT_EQ(gpio_.write(PIN, 1), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.write(PIN, 2), Gpio::INVALID_ARGUMENT);
        ASSERT_EQ(gpio_.read(PIN), 1);

        ASSERT_EQ(gpio_.read(Gpio::PIN_COUNT), Gpio::INVALID_ARGUMENT);
        ASSERT_EQ(gpio_.setMode(PIN, static_cast<GpioMode>(99)), Gpio::INVALID_ARGUMENT);
        ASSERT_EQ(gpio_.setDuty(PIN, 25), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setRange(PIN, 0), Gpio::INVALID_ARGUMENT);
        ASSERT_EQ(gpio_.setRange(PIN, 24), Gpio::INVALID_ARGUMENT);
        ASSERT_EQ(gpio_.setRange(PIN, 40001), Gpio::INVALID_ARGUMENT);
        ASSERT_EQ(gpio_.setDuty(PIN, 101), Gpio::INVALID_ARGUMENT);
        ASSERT_EQ(gpio_.setFrequency(PIN, 0), Gpio::INVALID_ARGUMENT);
        ASSERT_EQ(gpio_.setFrequency(PIN, 10001), Gpio::INVALID_ARGUMENT);

        PwmSettings settings;
        ASSERT_EQ(gpio_.getPwmSettings(PIN, settings), Gpio::SUCCESS);
        ASSERT_EQ(settings.range, 100u);
        ASSERT_EQ(settings.duty, 25u);
        ASSERT_EQ(settings.frequency, 1000u);
        ASSERT_FALSE(settings.enabled);
    }

    TEST_F(GpioTest, OffPreservesPwmSettingsAndDrivesLow) {
        ASSERT_EQ(gpio_.setRange(PIN, 50), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setDuty(PIN, 50), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setFrequency(PIN, 2000), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setEnabled(PIN, true), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.read(PIN), 1);
        ASSERT_EQ(gpio_.write(PIN, 0), Gpio::PWM_ACTIVE);
        ASSERT_EQ(gpio_.setEnabled(PIN, false), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.read(PIN), 0);

        PwmSettings settings;
        ASSERT_EQ(gpio_.getPwmSettings(PIN, settings), Gpio::SUCCESS);
        ASSERT_EQ(settings.range, 50u);
        ASSERT_EQ(settings.duty, 50u);
        ASSERT_EQ(settings.frequency, 2000u);
        ASSERT_FALSE(settings.enabled);

        ASSERT_EQ(gpio_.setEnabled(PIN, true), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.read(PIN), 1);
        ASSERT_EQ(gpio_.setDuty(PIN, 0), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.read(PIN), 0);
        ASSERT_EQ(gpio_.getPwmSettings(OTHER, settings), Gpio::SUCCESS);
        ASSERT_EQ(settings.duty, 0u);
        ASSERT_FALSE(settings.enabled);
    }

    TEST_F(GpioTest, RepeatedOutputModeStopsPwmAndPreservesSettings) {
        ASSERT_EQ(gpio_.setRange(PIN, 50), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setDuty(PIN, 50), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setFrequency(PIN, 2000), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setEnabled(PIN, true), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setMode(PIN, GpioMode::output), Gpio::SUCCESS);

        PwmSettings settings;
        ASSERT_EQ(gpio_.getPwmSettings(PIN, settings), Gpio::SUCCESS);
        ASSERT_FALSE(settings.enabled);
        ASSERT_EQ(settings.range, 50u);
        ASSERT_EQ(settings.duty, 50u);
        ASSERT_EQ(settings.frequency, 2000u);
        ASSERT_EQ(gpio_.read(PIN), 0);
        ASSERT_EQ(gpio_.write(PIN, 1), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setMode(PIN, GpioMode::output), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.read(PIN), 1);
    }

    TEST_F(GpioTest, DisabledConfigurationDoesNotGenerateOutput) {
        ASSERT_EQ(gpio_.setRange(PIN, 1), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setDuty(PIN, 1), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.read(PIN), 0);
        ASSERT_EQ(gpio_.setEnabled(PIN, true), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.read(PIN), 1);
        ASSERT_EQ(gpio_.setMode(PIN, GpioMode::input), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.write(PIN, 1), Gpio::WRONG_MODE);
        ASSERT_EQ(gpio_.setEnabled(PIN, true), Gpio::WRONG_MODE);
        ASSERT_EQ(gpio_.setMode(PIN, GpioMode::output), Gpio::SUCCESS);
        PwmSettings settings;
        ASSERT_EQ(gpio_.getPwmSettings(PIN, settings), Gpio::SUCCESS);
        ASSERT_FALSE(settings.enabled);
    }

    TEST_F(GpioTest, InitializationIsIdempotentAndTerminationResetsState) {
        ASSERT_EQ(gpio_.write(PIN, 1), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.initialize(), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.read(PIN), 1);
        ASSERT_EQ(gpio_.setMode(PIN, GpioMode::output), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.read(PIN), 1);
        ASSERT_EQ(gpio_.terminate(), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.read(PIN), Gpio::NOT_INITIALIZED);
        ASSERT_EQ(gpio_.write(PIN, 0), Gpio::NOT_INITIALIZED);
        ASSERT_EQ(gpio_.setEnabled(PIN, true), Gpio::NOT_INITIALIZED);
        ASSERT_EQ(gpio_.terminate(), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.initialize(), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.read(PIN), Gpio::WRONG_MODE);
        ASSERT_EQ(gpio_.setMode(PIN, GpioMode::output), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.read(PIN), 0);
    }

    TEST_F(GpioTest, PlatformChannelMappings) {
        ASSERT_EQ(gpioHardwarePwmChannel(12, GpioPwmLayout::rpi4), 0);
        ASSERT_EQ(gpioHardwarePwmChannel(18, GpioPwmLayout::rpi4), 0);
        ASSERT_EQ(gpioHardwarePwmChannel(13, GpioPwmLayout::rpi4), 1);
        ASSERT_EQ(gpioHardwarePwmChannel(19, GpioPwmLayout::rpi4), 1);
        ASSERT_EQ(gpioHardwarePwmChannel(14, GpioPwmLayout::rpi4), Gpio::NOT_SUPPORTED);
        ASSERT_EQ(gpioHardwarePwmChannel(12, GpioPwmLayout::rpi5), 0);
        ASSERT_EQ(gpioHardwarePwmChannel(13, GpioPwmLayout::rpi5), 1);
        ASSERT_EQ(gpioHardwarePwmChannel(14, GpioPwmLayout::rpi5), 2);
        ASSERT_EQ(gpioHardwarePwmChannel(18, GpioPwmLayout::rpi5), 2);
        ASSERT_EQ(gpioHardwarePwmChannel(15, GpioPwmLayout::rpi5), 3);
        ASSERT_EQ(gpioHardwarePwmChannel(19, GpioPwmLayout::rpi5), 3);
        ASSERT_EQ(gpioHardwarePwmChannel(17, GpioPwmLayout::rpi5), Gpio::NOT_SUPPORTED);
    }

    TEST_F(GpioTest, ExplicitModeReservesChannelEvenWhenOff) {
        const unsigned first = gpio_.hardwarePwmChannel(12) == gpio_.hardwarePwmChannel(18) ? 12 : 14;
        for (const unsigned pin : {first, 18u, 17u, 13u}) {
            cleanupPins_[pin] = true;
        }
        ASSERT_EQ(gpio_.setMode(first, GpioMode::hardwarePwm), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setMode(18, GpioMode::hardwarePwm), Gpio::CHANNEL_BUSY);
        ASSERT_EQ(gpio_.setMode(17, GpioMode::hardwarePwm), Gpio::NOT_SUPPORTED);
        ASSERT_EQ(gpio_.write(first, 1), Gpio::WRONG_MODE);
        ASSERT_EQ(gpio_.read(first), Gpio::WRONG_MODE);
        ASSERT_EQ(gpio_.setRange(first, 50), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setDuty(first, 25), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setEnabled(first, true), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setEnabled(first, false), Gpio::SUCCESS);

        PwmSettings settings;
        ASSERT_EQ(gpio_.getPwmSettings(first, settings), Gpio::SUCCESS);
        ASSERT_EQ(settings.duty, 25u);
        ASSERT_FALSE(settings.enabled);
        ASSERT_EQ(gpio_.setMode(18, GpioMode::hardwarePwm), Gpio::CHANNEL_BUSY);
        ASSERT_EQ(gpio_.setMode(first, GpioMode::output), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.write(first, 1), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setMode(18, GpioMode::hardwarePwm), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setMode(13, GpioMode::hardwarePwm), Gpio::SUCCESS);
    }

} // namespace
