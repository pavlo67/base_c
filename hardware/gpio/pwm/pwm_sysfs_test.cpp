#include "pwm_sysfs.h"
#include <gtest/gtest.h>
#include <cerrno>
#include <map>
#include <vector>

class FakePwmSysfsIo : public PwmSysfsIo {
public:
    std::map<std::string, std::string> files{{"chip/npwm", "4\n"}};
    std::vector<std::pair<std::string, std::string>> writes;
    std::string failPath;
    int failure = -EIO;
    int findChip(unsigned pin, std::string& chip) override {
        if (pin != 12 && pin != 13 && pin != 18 && pin != 19) { return Gpio::PWM_NOT_CONFIGURED; }
        chip = "chip";
        return 0;
    }
    int read(const std::string& path, std::string& value) override {
        const auto it = files.find(path);
        if (it == files.end()) { return -ENOENT; }
        value = it->second;
        return 0;
    }
    int write(const std::string& path, const std::string& value) override {
        writes.emplace_back(path, value);
        if (path == failPath) { failPath.clear(); return failure; }
        if (path == "chip/export") {
            const auto base = "chip/pwm" + value;
            if (files.contains(base + "/enable")) { return -EBUSY; }
            for (const auto& attribute : {"enable", "period", "duty_cycle", "polarity"}) {
                files[base + "/" + attribute] = "0";
            }
            return 0;
        }
        if (path == "chip/unexport") {
            const auto base = "chip/pwm" + value;
            for (const auto& attribute : {"enable", "period", "duty_cycle", "polarity"}) {
                files.erase(base + "/" + attribute);
            }
            return 0;
        }
        if (!files.contains(path)) { return -ENOENT; }
        const auto base = path.substr(0, path.rfind('/'));
        if (path.ends_with("/period") && std::stoull(value) < std::stoull(files[base + "/duty_cycle"])) { return -EINVAL; }
        if (path.ends_with("/duty_cycle") && std::stoull(value) > std::stoull(files[base + "/period"])) { return -EINVAL; }
        files[path] = value;
        return 0;
    }
};

TEST(PwmSysfsTest, ConvertsRatioAndReducesDutyBeforeShorteningPeriod) {
    FakePwmSysfsIo io;
    PwmSysfs pwm(io);
    ASSERT_EQ(pwm.claim(12, 0), 0);
    ASSERT_EQ(pwm.apply(0, {100, 75, 100, true}), 0);
    EXPECT_EQ(io.files["chip/pwm0/period"], "10000000");
    EXPECT_EQ(io.files["chip/pwm0/duty_cycle"], "7500000");
    ASSERT_EQ(pwm.apply(0, {100, 25, 1000, true}), 0);
    EXPECT_EQ(io.files["chip/pwm0/period"], "1000000");
    EXPECT_EQ(io.files["chip/pwm0/duty_cycle"], "250000");
    EXPECT_EQ(pwm.close(), 0);
}

TEST(PwmSysfsTest, OffHoldsLowAndFullDutyIsSupported) {
    FakePwmSysfsIo io;
    PwmSysfs pwm(io);
    ASSERT_EQ(pwm.claim(18, 2), 0);
    ASSERT_EQ(pwm.apply(2, {50, 50, 2000, true}), 0);
    EXPECT_EQ(io.files["chip/pwm2/duty_cycle"], "500000");
    ASSERT_EQ(pwm.apply(2, {50, 50, 2000, false}), 0);
    EXPECT_EQ(io.files["chip/pwm2/duty_cycle"], "0");
    EXPECT_EQ(io.files["chip/pwm2/enable"], "1");
    EXPECT_EQ(pwm.close(), 0);
    EXPECT_FALSE(io.files.contains("chip/pwm2/enable"));
}

TEST(PwmSysfsTest, DoesNotAdoptOrReleaseSomeoneElsesChannel) {
    FakePwmSysfsIo io;
    io.files["chip/pwm0/enable"] = "1";
    PwmSysfs pwm(io);
    EXPECT_EQ(pwm.claim(12, 0), Gpio::CHANNEL_BUSY);
    EXPECT_EQ(pwm.close(), 0);
    EXPECT_EQ(io.files["chip/pwm0/enable"], "1");
    EXPECT_EQ(io.writes.size(), 1u);
}

TEST(PwmSysfsTest, RejectsMissingRoutesAndChannelsWithoutWrites) {
    FakePwmSysfsIo io;
    PwmSysfs pwm(io);
    EXPECT_EQ(pwm.claim(17, 0), Gpio::PWM_NOT_CONFIGURED);
    EXPECT_EQ(pwm.claim(12, 4), Gpio::NOT_SUPPORTED);
    io.files["chip/npwm"] = "2";
    EXPECT_EQ(pwm.claim(18, 2), Gpio::NOT_SUPPORTED);
    EXPECT_TRUE(io.writes.empty());
}

TEST(PwmSysfsTest, FailedSetupUnexportsAndAllowsRetry) {
    FakePwmSysfsIo io;
    io.failPath = "chip/pwm0/polarity";
    PwmSysfs pwm(io);
    EXPECT_EQ(pwm.claim(12, 0), -EIO);
    EXPECT_FALSE(io.files.contains("chip/pwm0/enable"));
    ASSERT_EQ(pwm.claim(12, 0), 0);
    EXPECT_EQ(pwm.close(), 0);
}

TEST(PwmSysfsTest, FailedUpdateAttemptsLowAndCanBeRetried) {
    FakePwmSysfsIo io;
    PwmSysfs pwm(io);
    ASSERT_EQ(pwm.claim(12, 0), 0);
    ASSERT_EQ(pwm.apply(0, {100, 50, 1000, true}), 0);
    io.failPath = "chip/pwm0/period";
    EXPECT_EQ(pwm.apply(0, {100, 25, 2000, true}), -EIO);
    EXPECT_EQ(io.files["chip/pwm0/duty_cycle"], "0");
    EXPECT_EQ(pwm.apply(0, {100, 25, 2000, true}), 0);
    EXPECT_EQ(pwm.close(), 0);
}

TEST(PwmSysfsTest, ReleaseFailureRetainsOwnershipForRetry) {
    FakePwmSysfsIo io;
    PwmSysfs pwm(io);
    ASSERT_EQ(pwm.claim(12, 0), 0);
    io.failPath = "chip/unexport";
    EXPECT_EQ(pwm.close(), -EIO);
    EXPECT_EQ(pwm.claim(12, 0), Gpio::CHANNEL_BUSY);
    EXPECT_EQ(pwm.close(), 0);
    EXPECT_FALSE(io.files.contains("chip/pwm0/enable"));
}

#include <filesystem>
#include <fstream>
#include <cstdlib>

class PwmDeviceTreeTest : public testing::Test {
protected:
    std::filesystem::path root;
    std::filesystem::path node;
    void SetUp() override {
        char name[] = "/tmp/pan_tilt_pwm_XXXXXX";
        const char* created = mkdtemp(name);
        ASSERT_NE(created, nullptr);
        root = created;
        node = root / "tree/rp1/pwm@98000";
        std::filesystem::create_directories(node);
        std::filesystem::create_directories(root / "tree/rp1/gpio/pwm_pins");
        std::filesystem::create_directories(root / "pwm/pwmchip7/device");
        std::filesystem::create_directory_symlink(node, root / "pwm/pwmchip7/device/of_node");
        put(node / "compatible", std::string("raspberrypi,rp1-pwm\0", 19));
        put(node / "pinctrl-0", std::string("\0\0\0\x42", 4));
        put(root / "tree/rp1/gpio/pwm_pins/phandle", std::string("\0\0\0\x42", 4));
    }
    void TearDown() override {
        if (!root.empty()) {
            std::error_code error;
            std::filesystem::remove_all(root, error);
            EXPECT_FALSE(error);
        }
    }
    void put(const std::filesystem::path& path, const std::string& bytes) {
        std::ofstream stream(path, std::ios::binary);
        stream.write(bytes.data(), bytes.size());
        ASSERT_TRUE(stream.good());
    }
};

TEST_F(PwmDeviceTreeTest, FindsRenumberedChipWithRp1PinStrings) {
    const auto group = root / "tree/rp1/gpio/pwm_pins";
    put(group / "pins", std::string("gpio12\0gpio13\0", 14));
    put(group / "function", std::string("pwm0\0", 5));
    Rpi5PwmSysfsIo io((root / "pwm").string(), (root / "tree").string());
    std::string chip;
    EXPECT_EQ(io.findChip(12, chip), 0);
    EXPECT_EQ(chip, (root / "pwm/pwmchip7").string());
    EXPECT_EQ(io.findChip(1, chip), Gpio::PWM_NOT_CONFIGURED);
    EXPECT_EQ(io.findChip(18, chip), Gpio::PWM_NOT_CONFIGURED);
}

TEST_F(PwmDeviceTreeTest, ChecksLegacyPinFunctionAndReferencedPhandle) {
    const auto group = root / "tree/rp1/gpio/pwm_pins";
    put(group / "brcm,pins", std::string("\0\0\0\x12\0\0\0\x13", 8));
    put(group / "brcm,function", std::string("\0\0\0\x02", 4));
    Rpi5PwmSysfsIo io((root / "pwm").string(), (root / "tree").string());
    std::string chip;
    EXPECT_EQ(io.findChip(18, chip), 0);
    put(group / "brcm,function", std::string("\0\0\0\x04", 4));
    EXPECT_EQ(io.findChip(18, chip), Gpio::PWM_NOT_CONFIGURED);
    put(group / "brcm,function", std::string("\0\0\0\x02", 4));
    put(node / "pinctrl-0", std::string("\0\0\0\x43", 4));
    EXPECT_EQ(io.findChip(18, chip), Gpio::PWM_NOT_CONFIGURED);
}

TEST_F(PwmDeviceTreeTest, DoesNotSelectFanController) {
    const auto group = root / "tree/rp1/gpio/pwm_pins";
    put(group / "pins", std::string("gpio12\0", 7));
    put(group / "function", std::string("pwm0\0", 5));
    std::filesystem::rename(node, root / "tree/rp1/pwm@9c000");
    std::filesystem::remove(root / "pwm/pwmchip7/device/of_node");
    std::filesystem::create_directory_symlink(root / "tree/rp1/pwm@9c000", root / "pwm/pwmchip7/device/of_node");
    Rpi5PwmSysfsIo io((root / "pwm").string(), (root / "tree").string());
    std::string chip;
    EXPECT_EQ(io.findChip(12, chip), Gpio::PWM_NOT_CONFIGURED);
}
