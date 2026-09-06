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

        void SetUp() override {
            ASSERT_EQ(gpio_.initialize(), Gpio::SUCCESS);
            for (std::size_t i = 0; i < GPIO_TEST_PINS.size(); ++i) {
                const unsigned pin = GPIO_TEST_PINS[i];
                for (std::size_t j = 0; j < i; ++j) {
                    ASSERT_NE(pin, GPIO_TEST_PINS[j]) << "Duplicate GPIO_TEST_PINS entry";
                }
                ASSERT_EQ(gpio_.setMode(pin, GpioMode::output), Gpio::SUCCESS) << pin;
                const int level = gpio_.read(pin);
                ASSERT_GE(level, 0) << pin;
                ASSERT_LE(level, 1) << pin;
                original_[i] = level;
            }

            ASSERT_EQ(gpio_.write(OTHER, 0), 0);
        }

        void TearDown() override {
            for (std::size_t i = 0; i < GPIO_TEST_PINS.size(); ++i) {
                ASSERT_EQ(gpio_.setMode(GPIO_TEST_PINS[i], GpioMode::output), Gpio::SUCCESS);
                EXPECT_EQ(gpio_.write(GPIO_TEST_PINS[i], original_[i]), Gpio::SUCCESS);
            }
            EXPECT_EQ(gpio_.terminate(), 0);
        }

    };

    TEST_F(GpioTest, InvertReadBackAndRestore) {
        for (std::size_t i = 0; i < GPIO_TEST_PINS.size(); ++i) {
            const unsigned pin = GPIO_TEST_PINS[i];
            ASSERT_EQ(gpio_.write(pin, 1 - original_[i]), Gpio::SUCCESS) << pin;
            EXPECT_EQ(gpio_.read(pin), 1 - original_[i]) << pin;
            ASSERT_EQ(gpio_.write(pin, original_[i]), Gpio::SUCCESS) << pin;
            EXPECT_EQ(gpio_.read(pin), original_[i]) << pin;
        }
    }

    TEST_F(GpioTest, DigitalPinsAreIndependent) {
        ASSERT_EQ(gpio_.write(PIN, 1), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.read(PIN), 1);
        EXPECT_EQ(gpio_.read(OTHER), 0);
        ASSERT_EQ(gpio_.write(OTHER, 1), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.write(PIN, 0), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.read(PIN), 0);
        EXPECT_EQ(gpio_.read(OTHER), 1);
    }

    TEST_F(GpioTest, InvalidOperationsDoNotChangeState) {
        ASSERT_EQ(gpio_.write(PIN, 1), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.write(PIN, 2), Gpio::INVALID_ARGUMENT);
        EXPECT_EQ(gpio_.read(PIN), 1);

        EXPECT_EQ(gpio_.read(Gpio::PIN_COUNT), Gpio::INVALID_ARGUMENT);
        EXPECT_EQ(gpio_.setMode(PIN, static_cast<GpioMode>(99)), Gpio::INVALID_ARGUMENT);
        ASSERT_EQ(gpio_.setDuty(PIN, 25), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.setRange(PIN, 0), Gpio::INVALID_ARGUMENT);
        EXPECT_EQ(gpio_.setRange(PIN, 24), Gpio::INVALID_ARGUMENT);
        EXPECT_EQ(gpio_.setRange(PIN, 40001), Gpio::INVALID_ARGUMENT);
        EXPECT_EQ(gpio_.setDuty(PIN, 101), Gpio::INVALID_ARGUMENT);
        EXPECT_EQ(gpio_.setFrequency(PIN, 0), Gpio::INVALID_ARGUMENT);
        EXPECT_EQ(gpio_.setFrequency(PIN, 10001), Gpio::INVALID_ARGUMENT);

        PwmSettings settings;
        ASSERT_EQ(gpio_.getPwmSettings(PIN, settings), Gpio::SUCCESS);
        EXPECT_EQ(settings.range, 100u);
        EXPECT_EQ(settings.duty, 25u);
        EXPECT_EQ(settings.frequency, 1000u);
        EXPECT_FALSE(settings.enabled);
    }

    TEST_F(GpioTest, OffPreservesPwmSettingsAndDrivesLow) {
        ASSERT_EQ(gpio_.setRange(PIN, 50), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setDuty(PIN, 50), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setFrequency(PIN, 2000), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setEnabled(PIN, true), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.read(PIN), 1);
        EXPECT_EQ(gpio_.write(PIN, 0), Gpio::PWM_ACTIVE);
        ASSERT_EQ(gpio_.setEnabled(PIN, false), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.read(PIN), 0);

        PwmSettings settings;
        ASSERT_EQ(gpio_.getPwmSettings(PIN, settings), Gpio::SUCCESS);
        EXPECT_EQ(settings.range, 50u);
        EXPECT_EQ(settings.duty, 50u);
        EXPECT_EQ(settings.frequency, 2000u);
        EXPECT_FALSE(settings.enabled);

        ASSERT_EQ(gpio_.setEnabled(PIN, true), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.read(PIN), 1);
        ASSERT_EQ(gpio_.setDuty(PIN, 0), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.read(PIN), 0);
        ASSERT_EQ(gpio_.getPwmSettings(OTHER, settings), Gpio::SUCCESS);
        EXPECT_EQ(settings.duty, 0u);
        EXPECT_FALSE(settings.enabled);
    }

    TEST_F(GpioTest, DisabledConfigurationDoesNotGenerateOutput) {
        ASSERT_EQ(gpio_.setRange(PIN, 1), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.setDuty(PIN, 1), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.read(PIN), 0);
        ASSERT_EQ(gpio_.setEnabled(PIN, true), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.read(PIN), 1);
        ASSERT_EQ(gpio_.setMode(PIN, GpioMode::input), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.write(PIN, 1), Gpio::WRONG_MODE);
        EXPECT_EQ(gpio_.setEnabled(PIN, true), Gpio::WRONG_MODE);
        ASSERT_EQ(gpio_.setMode(PIN, GpioMode::output), Gpio::SUCCESS);
        PwmSettings settings;
        ASSERT_EQ(gpio_.getPwmSettings(PIN, settings), Gpio::SUCCESS);
        EXPECT_FALSE(settings.enabled);
    }

    TEST_F(GpioTest, InitializationIsIdempotentAndTerminationResetsState) {
        ASSERT_EQ(gpio_.write(PIN, 1), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.initialize(), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.read(PIN), 1);
        ASSERT_EQ(gpio_.setMode(PIN, GpioMode::output), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.read(PIN), 1);
        ASSERT_EQ(gpio_.terminate(), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.read(PIN), Gpio::NOT_INITIALIZED);
        EXPECT_EQ(gpio_.write(PIN, 0), Gpio::NOT_INITIALIZED);
        EXPECT_EQ(gpio_.setEnabled(PIN, true), Gpio::NOT_INITIALIZED);
        EXPECT_EQ(gpio_.terminate(), Gpio::SUCCESS);
        ASSERT_EQ(gpio_.initialize(), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.read(PIN), Gpio::WRONG_MODE);
        ASSERT_EQ(gpio_.setMode(PIN, GpioMode::output), Gpio::SUCCESS);
        EXPECT_EQ(gpio_.read(PIN), 0);
    }

} // namespace

TEST(GpioHardwarePwmTest, PlatformChannelMappings) {
    EXPECT_EQ(gpioHardwarePwmChannel(12, GpioPwmLayout::rpi4), 0);
    EXPECT_EQ(gpioHardwarePwmChannel(18, GpioPwmLayout::rpi4), 0);
    EXPECT_EQ(gpioHardwarePwmChannel(13, GpioPwmLayout::rpi4), 1);
    EXPECT_EQ(gpioHardwarePwmChannel(19, GpioPwmLayout::rpi4), 1);
    EXPECT_EQ(gpioHardwarePwmChannel(14, GpioPwmLayout::rpi4), Gpio::NOT_SUPPORTED);
    EXPECT_EQ(gpioHardwarePwmChannel(12, GpioPwmLayout::rpi5), 0);
    EXPECT_EQ(gpioHardwarePwmChannel(13, GpioPwmLayout::rpi5), 1);
    EXPECT_EQ(gpioHardwarePwmChannel(14, GpioPwmLayout::rpi5), 2);
    EXPECT_EQ(gpioHardwarePwmChannel(18, GpioPwmLayout::rpi5), 2);
    EXPECT_EQ(gpioHardwarePwmChannel(15, GpioPwmLayout::rpi5), 3);
    EXPECT_EQ(gpioHardwarePwmChannel(19, GpioPwmLayout::rpi5), 3);
    EXPECT_EQ(gpioHardwarePwmChannel(17, GpioPwmLayout::rpi5), Gpio::NOT_SUPPORTED);
}

class GpioHardwareModeTest : public testing::Test {
protected:
    Gpio& gpio_ = Gpio::instance();
    void SetUp() override { ASSERT_EQ(gpio_.initialize(), Gpio::SUCCESS); }
    void TearDown() override {
        EXPECT_EQ(gpio_.terminate(), Gpio::SUCCESS);
        // TODO!!! restore original pin modes
    }
};

TEST_F(GpioHardwareModeTest, ExplicitModeReservesChannelEvenWhenOff) {
    ASSERT_EQ(gpio_.setMode(12, GpioMode::hardwarePwm), Gpio::SUCCESS);
    EXPECT_EQ(gpio_.setMode(18, GpioMode::hardwarePwm), Gpio::CHANNEL_BUSY);
    EXPECT_EQ(gpio_.setMode(17, GpioMode::hardwarePwm), Gpio::NOT_SUPPORTED);
    EXPECT_EQ(gpio_.write(12, 1), Gpio::WRONG_MODE);
    EXPECT_EQ(gpio_.read(12), Gpio::WRONG_MODE);
    ASSERT_EQ(gpio_.setRange(12, 50), Gpio::SUCCESS);
    ASSERT_EQ(gpio_.setDuty(12, 25), Gpio::SUCCESS);
    ASSERT_EQ(gpio_.setEnabled(12, true), Gpio::SUCCESS);
    ASSERT_EQ(gpio_.setEnabled(12, false), Gpio::SUCCESS);

    PwmSettings settings;
    ASSERT_EQ(gpio_.getPwmSettings(12, settings), Gpio::SUCCESS);
    EXPECT_EQ(settings.duty, 25u);
    EXPECT_FALSE(settings.enabled);
    EXPECT_EQ(gpio_.setMode(18, GpioMode::hardwarePwm), Gpio::CHANNEL_BUSY);
    ASSERT_EQ(gpio_.setMode(12, GpioMode::output), Gpio::SUCCESS);
    EXPECT_EQ(gpio_.write(12, 1), Gpio::SUCCESS);
    EXPECT_EQ(gpio_.setMode(18, GpioMode::hardwarePwm), Gpio::SUCCESS);
    EXPECT_EQ(gpio_.setMode(13, GpioMode::hardwarePwm), Gpio::SUCCESS);
}
