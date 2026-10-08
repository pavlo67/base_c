#ifndef BASE_JOINER_H
#define BASE_JOINER_H

#include <any>
#include <string>
#include <unordered_map>

using InterfaceKey = std::string;

class Joiner {
public:
    template<typename T>
    void Set(const InterfaceKey& key, T value) {
        static_assert(std::is_pointer_v<T>,
                      "Joiner::Set requires a pointer");

        interfaces_[key] = value;
    }

    template<typename T>
    T Get(const InterfaceKey& key) const {
        static_assert(std::is_pointer_v<T>,
                      "Joiner::Get requires a pointer");

        auto it = interfaces_.find(key);

        if (it == interfaces_.end()) {
            return nullptr;
        }

        auto* ptr = std::any_cast<T>(&it->second);
        return ptr ? *ptr : nullptr;
    }

private:
    std::unordered_map<InterfaceKey, std::any> interfaces_;
};

#endif
