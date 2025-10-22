#pragma once

#include <format>
#include <iostream>
#include <source_location>

namespace std {
inline string to_string(const source_location& l) {
  return format("{}:{} `{}`: ", l.file_name(), l.line(), l.function_name());
}
}  // namespace std

namespace dpx {

[[noreturn]] inline void die(std::string why) { throw std::runtime_error(why); }

inline void footprint(std::string what) { std::cerr << what << std::endl; }

#define die(fmt, ...) \
  dpx::die(std::to_string(std::source_location::current()) + std::vformat(fmt, std::make_format_args(__VA_ARGS__)))

#define footprint(fmt, ...)                                        \
  dpx::footprint(std::to_string(std::source_location::current()) + \
                 std::vformat(fmt, std::make_format_args(__VA_ARGS__)))

}  // namespace dpx
