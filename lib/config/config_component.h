#ifndef BASE_CONFIG_SECTION_H
#define BASE_CONFIG_SECTION_H

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "config.h"
#include "lib/strlib.h"

using config_values_t = std::map<std::string, std::string>;

class ComponentState {
public:
    virtual ~ComponentState() = default;

    virtual bool load(const Config& cfg) = 0;
    [[nodiscard]] virtual config_values_t get() const = 0;
    virtual void test() = 0;

    template<typename T, typename TestFunction>
    void saveInterface(T& interface, TestFunction testFunction) {
        interfaceTests_.emplace_back([&interface, testFunction] { testFunction(interface); });
    }

protected:
    void testSavedInterfaces() {
        for (const auto& testFunction : interfaceTests_) {
            testFunction();
        }
    }

private:
    std::vector<std::function<void()>> interfaceTests_;
};

using config_components_t = std::vector<std::pair<std::string, ComponentState*>>;

inline void printComponentState(const std::string& name, const std::map<std::string, std::string>& values) {
    printf("\n\n%s\n%s\n", name.c_str(), stringMapToMultilineString(values).c_str());
}


#endif // BASE_CONFIG_SECTION_H
