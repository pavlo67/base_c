#include "gpio.h"
#include "hardware/hardware.h"
#include <gtest/gtest.h>
#include <array>

namespace {

class GpioOutputTest : public testing::Test {
protected:
    Gpio& gpio = Gpio::instance();
    std::array<int, GPIO_TEST_PINS.size()> original{};
    std::size_t configured = 0;

    void SetUp() override {
        ASSERT_FALSE(GPIO_TEST_PINS.empty());
        ASSERT_EQ(gpio.initialize(), 0);
        for (std::size_t i = 0; i < GPIO_TEST_PINS.size(); ++i) {
            const unsigned pin = GPIO_TEST_PINS[i];
            for (std::size_t j = 0; j < i; ++j) {
                ASSERT_NE(pin, GPIO_TEST_PINS[j]) << "Duplicate GPIO_TEST_PINS entry";
            }
            ASSERT_EQ(gpio.setMode(pin, GpioMode::output), 0) << pin;
            const int level = gpio.read(pin);
            ASSERT_GE(level, 0) << pin;
            ASSERT_LE(level, 1) << pin;
            original[i] = level;
            ++configured;
        }
    }

    void TearDown() override {
        for (std::size_t i = 0; i < configured; ++i) {
            EXPECT_EQ(gpio.write(GPIO_TEST_PINS[i], original[i]), 0);
        }
        EXPECT_EQ(gpio.terminate(), 0);
    }
};

TEST_F(GpioOutputTest, InvertReadBackAndRestore) {
    for (std::size_t i = 0; i < configured; ++i) {
        const unsigned pin = GPIO_TEST_PINS[i];
        ASSERT_EQ(gpio.write(pin, 1 - original[i]), 0) << pin;
        EXPECT_EQ(gpio.read(pin), 1 - original[i]) << pin;
        ASSERT_EQ(gpio.write(pin, original[i]), 0) << pin;
        EXPECT_EQ(gpio.read(pin), original[i]) << pin;
    }
}

} // namespace
