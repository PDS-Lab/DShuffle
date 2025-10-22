#pragma once

#include <zpp_bits.h>

#include <boost/core/demangle.hpp>
#include <concepts>
#include <cstdint>

#include "util/logger.hxx"

namespace dpx {

using rpc_id_t = uint64_t;

using rpc_seq_t = uint32_t;

template <zpp::bits::string_literal Name, typename RequestType, typename ResponseType>
  requires std::default_initializable<RequestType> &&
           (std::is_void_v<ResponseType> || std::default_initializable<ResponseType>)
struct RpcBase {
  using Request = RequestType;
  using Response = ResponseType;
  using Handler = std::function<Response(Request& req)>;
  inline constexpr static std::string name = std::string(Name.begin(), Name.end());
  inline constexpr static rpc_id_t id = zpp::bits::id_v<zpp::bits::sha1<Name>(), sizeof(int)>;
  Response operator()(Request&) const {
    WARN("Dummy default handler called, rpc id: {}", id);
    if constexpr (!std::is_void_v<Response>) {
      return Response{};
    }
  }  // dummy default handler
};

template <typename T>
concept Rpc = requires(T rpc) {
  typename T::Request;
  typename T::Response;
  typename T::Handler;
  { rpc.id } -> std::convertible_to<rpc_id_t>;
};

template <Rpc Rpc>
using req_t = typename Rpc::Request;

template <Rpc Rpc>
using resp_t = typename Rpc::Response;

template <Rpc Rpc>
using handler_t = typename Rpc::Handler;

template <typename T, Rpc rpc>
struct is_handler_of : std::is_same<T, typename rpc::Handler> {};

template <typename T, Rpc rpc>
inline constexpr bool is_handler_of_v = is_handler_of<T, rpc>::value;

template <Rpc Rpc>
struct is_oneway : std::is_void<resp_t<Rpc>> {};

template <Rpc Rpc>
inline constexpr bool is_oneway_v = is_oneway<Rpc>::value;

namespace detail {
template <typename Head, typename>
struct Cons;

template <typename Head, typename... Tail>
struct Cons<Head, std::tuple<Tail...>> {
  using type = std::tuple<Head, Tail...>;
};

template <template <typename T> class Pred, typename...>
struct Filter;

template <template <typename T> class Pred>
struct Filter<Pred> {
  using type = std::tuple<>;
};

template <template <typename T> class Pred, typename Head, typename... Tail>
struct Filter<Pred, Head, Tail...> {
  using type = std::conditional_t<Pred<Head>::value, typename Cons<Head, typename Filter<Pred, Tail...>::type>::type,
                                  typename Filter<Pred, Tail...>::type>;
};

}  // namespace detail

template <Rpc... rpcs>
struct OnewayRpcFilter {
  template <Rpc rpc>
  struct Predictor {
    static constexpr bool value = std::is_void_v<resp_t<rpc>>;
  };

  using type = detail::Filter<Predictor, rpcs...>::type;
};

template <Rpc... rpcs>
struct NormalRpcFilter {
  template <Rpc rpc>
  struct Predictor {
    static constexpr bool value = !std::is_void_v<resp_t<rpc>>;
  };

  using type = detail::Filter<Predictor, rpcs...>::type;
};

namespace detail {

template <typename... T>
std::variant<std::monostate, T...> f(std::tuple<T...>) {}

template <typename T>
using to_variant = decltype(f(std::declval<T>()));

template <Rpc... rpcs>
std::tuple<typename rpcs::Handler...> handlers(std::tuple<rpcs...>) {}

template <typename T>
using to_handlers = decltype(handlers(std::declval<T>()));

}  // namespace detail

template <typename T>
struct GeneralHandler {
  using type = detail::to_variant<detail::to_handlers<T>>;
};

template <typename T>
using general_handler_t = GeneralHandler<T>::type;

}  // namespace dpx
