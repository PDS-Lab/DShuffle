#pragma once

#include <cstdint>
#include <format>
#include <iomanip>
#include <ostream>
#include <span>
#include <sstream>

namespace dpx {

template <size_t RowSize = 16, bool ShowAscii = true>
struct CustomHexdump {
  explicit CustomHexdump(const std::span<uint8_t> s_) : s(s_) {}
  CustomHexdump(const void *data, size_t length) : s(reinterpret_cast<const uint8_t *>(data), length) {}
  std::span<const uint8_t> s;
};

using Hexdump = CustomHexdump<16, true>;

}  // namespace dpx

namespace std {
template <size_t RowSize, bool ShowAscii>
std::string to_string(const dpx::CustomHexdump<RowSize, ShowAscii> &dump) {
  constexpr static char hex_lookup[] = "0123456789ABCDEF";
  std::stringstream out;
  auto &s = dump.s;
  out.fill('0');
  out << std::uppercase << std::hex << "0x" << reinterpret_cast<uintptr_t>(s.data());
  out << std::dec << ' ' << s.size() << '\n';
  for (auto i = 0uz; i < s.size(); i += RowSize) {
    out << std::setw(8) << std::hex << i << ": ";
    for (auto j = 0uz; j < RowSize; ++j) {
      if (i + j < s.size()) {
        out << hex_lookup[s[i + j] >> 4] << hex_lookup[s[i + j] & 0xF] << ' ';
      } else {
        out << "   ";
      }
    }
    out << ' ';
    if (ShowAscii) {
      for (auto j = 0uz; j < RowSize; ++j) {
        if (i + j < s.size()) {
          out << (std::isprint(s[i + j]) ? static_cast<char>(s[i + j]) : '.');
        }
      }
    }
    if (i + RowSize < s.size()) {
      out << '\n';
    }
  }
  return out.str();
}
}  // namespace std

template <size_t RowSize, bool ShowAscii>
std::ostream &operator<<(std::ostream &out, const dpx::CustomHexdump<RowSize, ShowAscii> &dump) {
  return out << std::to_string(dump);
}

template <size_t RowSize, bool ShowAscii>
struct std::formatter<dpx::CustomHexdump<RowSize, ShowAscii>> : std::formatter<std::string> {
  template <typename Context>
  Context::iterator format(const dpx::CustomHexdump<RowSize, ShowAscii> &dump, Context &ctx) const {
    return std::formatter<std::string>::format(std::to_string(dump), ctx);
  }
};