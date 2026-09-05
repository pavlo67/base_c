#include <gtest/gtest.h>

#include "gpio.h"
#include "hardware/hardware.h"

namespace {
    static_assert(GPIO_TEST_PINS.size() >= 2, "Contract tests require two configured pins");
    constexpr unsigned PIN = GPIO_TEST_PINS[0];
    constexpr unsigned OTHER = GPIO_TEST_PINS[1];
    static_assert(PIN < Gpio::PIN_COUNT && OTHER < Gpio::PIN_COUNT && PIN != OTHER);

    class GpioContractTest : public testing::Test {
    protected:
        Gpio& gpio = Gpio::instance();
        void SetUp() override {
            ASSERT_EQ(gpio.initialize(), 0);
            ASSERT_EQ(gpio.setMode(PIN, GpioMode::output), 0);
            ASSERT_EQ(gpio.setMode(OTHER, GpioMode::output), 0);
            ASSERT_EQ(gpio.write(OTHER, 0), 0);
        }
        void TearDown() override { EXPECT_EQ(gpio.terminate(), 0); }
    };

    TEST_F(GpioContractTest, DigitalPinsAreIndependent) {
        ASSERT_EQ(gpio.write(PIN, 1), 0);
        EXPECT_EQ(gpio.read(PIN), 1);
        EXPECT_EQ(gpio.read(OTHER), 0);
        ASSERT_EQ(gpio.write(OTHER, 1), 0);
        ASSERT_EQ(gpio.write(PIN, 0), 0);
        EXPECT_EQ(gpio.read(PIN), 0);
        EXPECT_EQ(gpio.read(OTHER), 1);
    }

    TEST_F(GpioContractTest, InvalidOperationsDoNotChangeState) {
        ASSERT_EQ(gpio.write(PIN, 1), 0);
        EXPECT_EQ(gpio.write(PIN, 2), Gpio::INVALID_ARGUMENT);
        EXPECT_EQ(gpio.read(PIN), 1);
        EXPECT_EQ(gpio.read(Gpio::PIN_COUNT), Gpio::INVALID_ARGUMENT);
        EXPECT_EQ(gpio.setMode(PIN, static_cast<GpioMode>(99)), Gpio::INVALID_ARGUMENT);
        ASSERT_EQ(gpio.setDuty(PIN, 25), 0);
        EXPECT_EQ(gpio.setRange(PIN, 0), Gpio::INVALID_ARGUMENT);
        EXPECT_EQ(gpio.setRange(PIN, 24), Gpio::INVALID_ARGUMENT);
        EXPECT_EQ(gpio.setRange(PIN, 40001), Gpio::INVALID_ARGUMENT);
        EXPECT_EQ(gpio.setDuty(PIN, 101), Gpio::INVALID_ARGUMENT);
        EXPECT_EQ(gpio.setFrequency(PIN, 0), Gpio::INVALID_ARGUMENT);
        EXPECT_EQ(gpio.setFrequency(PIN, 10001), Gpio::INVALID_ARGUMENT);
        PwmSettings settings;
        ASSERT_EQ(gpio.getPwmSettings(PIN, settings), 0);
        EXPECT_EQ(settings.range, 100u);
        EXPECT_EQ(settings.duty, 25u);
        EXPECT_EQ(settings.frequency, 1000u);
        EXPECT_FALSE(settings.enabled);
    }

    TEST_F(GpioContractTest, OffPreservesPwmSettingsAndDrivesLow) {
        ASSERT_EQ(gpio.setRange(PIN, 50), 0);
        ASSERT_EQ(gpio.setDuty(PIN, 50), 0);
        ASSERT_EQ(gpio.setFrequency(PIN, 2000), 0);
        ASSERT_EQ(gpio.setEnabled(PIN, true), 0);
        EXPECT_EQ(gpio.read(PIN), 1);
        EXPECT_EQ(gpio.write(PIN, 0), Gpio::PWM_ACTIVE);
        ASSERT_EQ(gpio.setEnabled(PIN, false), 0);
        EXPECT_EQ(gpio.read(PIN), 0);
        PwmSettings settings;
        ASSERT_EQ(gpio.getPwmSettings(PIN, settings), 0);
        EXPECT_EQ(settings.range, 50u);
        EXPECT_EQ(settings.duty, 50u);
        EXPECT_EQ(settings.frequency, 2000u);
        EXPECT_FALSE(settings.enabled);
        ASSERT_EQ(gpio.setEnabled(PIN, true), 0);
        EXPECT_EQ(gpio.read(PIN), 1);
        ASSERT_EQ(gpio.setDuty(PIN, 0), 0);
        EXPECT_EQ(gpio.read(PIN), 0);
        ASSERT_EQ(gpio.getPwmSettings(OTHER, settings), 0);
        EXPECT_EQ(settings.duty, 0u);
        EXPECT_FALSE(settings.enabled);
    }

    TEST_F(GpioContractTest, DisabledConfigurationDoesNotGenerateOutput) {
        ASSERT_EQ(gpio.setRange(PIN, 1), 0);
        ASSERT_EQ(gpio.setDuty(PIN, 1), 0);
        EXPECT_EQ(gpio.read(PIN), 0);
        ASSERT_EQ(gpio.setEnabled(PIN, true), 0);
        EXPECT_EQ(gpio.read(PIN), 1);
        ASSERT_EQ(gpio.setMode(PIN, GpioMode::input), 0);
        EXPECT_EQ(gpio.write(PIN, 1), Gpio::WRONG_MODE);
        EXPECT_EQ(gpio.setEnabled(PIN, true), Gpio::WRONG_MODE);
        ASSERT_EQ(gpio.setMode(PIN, GpioMode::output), 0);
        PwmSettings settings;
        ASSERT_EQ(gpio.getPwmSettings(PIN, settings), 0);
        EXPECT_FALSE(settings.enabled);
    }

    TEST_F(GpioContractTest, InitializationIsIdempotentAndTerminationResetsState) {
        ASSERT_EQ(gpio.write(PIN, 1), 0);
        ASSERT_EQ(gpio.initialize(), 0);
        EXPECT_EQ(gpio.read(PIN), 1);
        ASSERT_EQ(gpio.setMode(PIN, GpioMode::output), 0);
        EXPECT_EQ(gpio.read(PIN), 1);
        ASSERT_EQ(gpio.terminate(), 0);
        EXPECT_EQ(gpio.read(PIN), Gpio::NOT_INITIALIZED);
        EXPECT_EQ(gpio.write(PIN, 0), Gpio::NOT_INITIALIZED);
        EXPECT_EQ(gpio.setEnabled(PIN, true), Gpio::NOT_INITIALIZED);
        EXPECT_EQ(gpio.terminate(), 0);
        ASSERT_EQ(gpio.initialize(), 0);
        EXPECT_EQ(gpio.read(PIN), Gpio::WRONG_MODE);
        ASSERT_EQ(gpio.setMode(PIN, GpioMode::output), 0);
        EXPECT_EQ(gpio.read(PIN), 0);
    }

} // namespace
