#pragma once

#include <zpp_bits.h>

#include "doca/buffer.hxx"
#include "memory/local_buffer.hxx"
#include "memory/naive_buffer.hxx"

namespace dpx {

template <typename BufferType>
concept ByteView = zpp::bits::concepts::byte_view<BufferType>;

static_assert(ByteView<LocalBuffer>, "LocalBuffer should be a valid byte view.");

static_assert(ByteView<naive::OwnedBuffer>, "naive::Buffer is not a valid byte view");
static_assert(ByteView<naive::BorrowedBuffer>, "naive::Buffer is not a valid byte view");
static_assert(ByteView<doca::OwnedBuffer>, "doca::Buffer is not a valid byte view");
static_assert(ByteView<doca::BorrowedBuffer>, "doca::Buffer is not a valid byte view");

}  // namespace dpx
