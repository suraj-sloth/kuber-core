#include "trading/config.h"
#include <sstream>

namespace trading {

template<typename T>
T Config::get(const std::string& key) const {
    auto it = values_.find(key);
    if (it != values_.end()) {
        return std::get<T>(it->second);
    }
    throw std::runtime_error("Key not found: " + key);
}

template<typename T>
void Config::set(const std::string& key, const T& value) {
    values_[key] = value;
}

std::string Config::to_string() const {
    std::stringstream ss;
    ss << "{\n";
    for (const auto& [key, value] : values_) {
        ss << "  \"" << key << "\": ";
        std::visit([&ss](const auto& v) {
            if constexpr (std::is_same_v<decltype(v), int>) {
                ss << v;
            } else if constexpr (std::is_same_v<decltype(v), double>) {
                ss << v;
            } else if constexpr (std::is_same_v<decltype(v), std::string>) {
                ss << \"\" << v << "\"\";
            } else if constexpr (std::is_same_v<decltype(v), bool>) {
                ss << (v ? "true" : "false");
            }
        }, value);
        ss << ",\n";
    }
    ss << "}\n";
    return ss.str();
}

} // namespace trading