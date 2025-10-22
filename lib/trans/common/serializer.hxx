#pragma once

#include <zpp_bits.h>

#include "memory/local_buffer.hxx"

namespace dpx {

using Serializer = zpp::bits::out<LocalBuffer>;
using Deserializer = zpp::bits::in<LocalBuffer>;

}  // namespace dpx
