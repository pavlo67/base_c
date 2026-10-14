#ifndef BASE_JOINER_H
#define BASE_JOINER_H

#include <any>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

using InterfaceKey = std::string;

class Joiner {
public:
    template<typename... BaseTypes, typename T>
    void Set(const InterfaceKey& key, T value) {
        static_assert(std::is_pointer_v<T>,
                      "Joiner::Set requires a pointer");
        static_assert((std::is_base_of_v<BaseTypes, std::remove_pointer_t<T>> && ...),
                      "Joiner::Set base type must be a base of the pointer target");

        auto& interfaces = interfaces_[key];
        interfaces.clear();
        interfaces.emplace_back(value);
        (interfaces.emplace_back(static_cast<BaseTypes*>(value)), ...);
    }

    template<typename T>

    T Get(const InterfaceKey& key) const {
        static_assert(std::is_pointer_v<T>,
                      "Joiner::Get requires a pointer");

        auto it = interfaces_.find(key);

        if (it == interfaces_.end()) {
            return nullptr;
        }

        return findInterface<T>(it->second);
    }

    template<typename T>
    [[nodiscard]] std::vector<T> GetAll() const {
        static_assert(std::is_pointer_v<T>,
                      "Joiner::GetAll requires a pointer");

        std::vector<T> result;
        for (const auto& [key, interfaces] : interfaces_) {
            (void)key;
            const auto interface = findInterface<T>(interfaces);
            if (interface) {
                result.push_back(interface);
            }
        }
        return result;
    }

    template<typename T>
    [[nodiscard]] std::vector<std::pair<InterfaceKey, T>> GetAllNamed() const {
        static_assert(std::is_pointer_v<T>,
                      "Joiner::GetAllNamed requires a pointer");

        std::vector<std::pair<InterfaceKey, T>> result;
        for (const auto& [key, interfaces] : interfaces_) {
            const auto interface = findInterface<T>(interfaces);
            if (interface) {
                result.emplace_back(key, interface);
            }
        }
        return result;
    }

private:
    template<typename T>
    static T findInterface(const std::vector<std::any>& interfaces) {
        for (const auto& interface : interfaces) {
            const auto* ptr = std::any_cast<T>(&interface);
            if (ptr && *ptr) {
                return *ptr;
            }
        }
        return nullptr;
    }

    std::unordered_map<InterfaceKey, std::vector<std::any>> interfaces_;
};

#endif
