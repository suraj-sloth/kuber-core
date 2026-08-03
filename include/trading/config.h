#pragma once

#include "trading/common.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <variant>

namespace trading {

class Config {
public:
    virtual ~Config() = default;

    template<typename T>
    T get(const std::string& key) const;

    template<typename T>
    void set(const std::string& key, const T& value);

    std::string to_string() const;

protected:
    std::unordered_map<std::string, std::variant<int, double, std::string, bool>> values_;
};

} // namespace trading