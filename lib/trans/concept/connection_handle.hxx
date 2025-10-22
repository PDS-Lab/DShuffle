#pragma once

namespace dpx {

template <typename T>
concept CanBeUsedAsConnectionHandle = requires(T h, T::Endpoint e) {
  { h.associate(e) };
  { h.listen_and_accept() };
  { h.wait_for_disconnect() };
  { h.connect() };
  { h.disconnect() };
};

}  // namespace dpx
