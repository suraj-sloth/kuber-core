//=== trading ===//

#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace trading {

using Timestamp = std::chrono::nanoseconds::rep;
using String = std::string;
using StringView = std::string_view;
using Id = uint64_t;

} // namespace trading