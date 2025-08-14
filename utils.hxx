#pragma once

#include <iostream>
#include <source_location>
#include <unordered_map>
#include <variant>
#include <format>
#include <print>
#include <type_traits>

#include "Token.hxx"
#include "Expr.hxx"


#include <stdx/tuple.hpp>

// not sure where to put it
// keep it here for now...
[[noreturn]] inline void error(const std::string& msg = {}, [[maybe_unused]] const std::source_location& location = std::source_location::current()) noexcept {
    puts(msg.c_str());

    std::cerr
        << "(File: " << location.file_name() 
        << ", Line: " << location.line() 
        << ", Function: " << location.function_name()
    << ")\n";

    exit(1);
}


[[noreturn]] inline void expected(const TokenKind exp, const Token got, const std::source_location& location = std::source_location::current()) noexcept {
    using std::operator""s;
    error("Expected token "s + stringify(exp) + " and found "s + stringify(got.kind) + ": " + got.text, location);
}



// Some Bullshit

template<size_t Sz>
struct ConstexprString
{
  static constexpr size_t buffer_size = Sz + 1;
  char _str[buffer_size];

  template<size_t InnerSz>
  constexpr ConstexprString(char const (&str)[InnerSz]) { std::ranges::copy(str, _str); }

  constexpr auto begin() { return _str; }
  constexpr auto end() { return _str + buffer_size; }
};

template<size_t InnerSz>
ConstexprString(char const (&)[InnerSz]) -> ConstexprString<InnerSz - 1>;


template <ConstexprString s>
struct S;

template <typename Key, typename Value> struct MapEntry {
  using key_t = Key;
  using value_t = Value;
  value_t value;
};
template <typename T> using KeyFor = typename T::key_t;




template <size_t N, typename First, typename... Ts>
auto getHelper() {
    if constexpr (N == 0) return First{};
    else return getHelper<N - 1, Ts...>();
}


template <typename... Ts>
struct TypeList {
  inline constexpr static size_t count = sizeof...(Ts);

    template <size_t N>
    auto get() {
        return getHelper<N, Ts...>();
    }
};


template <size_t N, size_t M, typename L1, typename... Ls>
auto getHelper2() {
    if constexpr (N == 0) return L1{}.template get<M>();
    else return getHelper2<N - 1, M, Ls...>();
}



template <typename Lambda, typename First, typename... Ts>
struct Func {
    Lambda func;
    inline static constexpr size_t count = sizeof...(Ts) + not std::is_same_v<First, void>; // + 1 for First

    template <size_t N, size_t M>
    auto get2() {
        static_assert(not std::is_same_v<First, void>);

        return getHelper2<N, M, First, Ts...>();
    }

};


using Value = std::variant<int, double, bool, std::string, Closure>;

struct Any {};


template <size_t SIZE, size_t N = 0, typename... Ts>
requires (SIZE == 0)
static Value execute(Func<Ts...> func, const std::vector<Value>&, const auto& that) {
    static_assert(SIZE == decltype(func)::count);

    return func.func(that);
}

template <size_t SIZE, size_t N = 0, typename... Ts>
requires (SIZE == 1)
Value execute(Func<Ts...> func, const std::vector<Value>& args, const auto& that) {
    if constexpr (N < decltype(func)::count) {
        using T = decltype(func.template get2<N, 0>());

        if constexpr (not std::is_same_v<T, Any>)
            if (not std::holds_alternative<T>(args[0])) 
                return execute<SIZE, N + 1>(func, args, that); // try the next type list


        constexpr bool is_any1 = std::is_same_v<T, Any>;
        std::conditional_t<is_any1, Value, T> v1;
        if constexpr (is_any1) v1 = args[0];
        else v1 = std::get<T>(args[0]);


        return func.func(v1, that);
    }
    else error("Wrong type passed to function!");
}

template <size_t SIZE, size_t N = 0, typename... Ts>
requires (SIZE == 2)
Value execute(Func<Ts...> func, const std::vector<Value>& args, const auto& that) {
    if constexpr (N < decltype(func)::count) {
        using T1 = decltype(func.template get2<N, 0>());
        using T2 = decltype(func.template get2<N, 1>());

        if constexpr (not std::is_same_v<T1, Any>)
            if (not std::holds_alternative<T1>(args[0])) 
                return execute<SIZE, N + 1>(func, args, that); // try the next type list

        if constexpr (not std::is_same_v<T2, Any>)
            if (not std::holds_alternative<T2>(args[1])) 
                return execute<SIZE, N + 1>(func, args, that);


        constexpr bool is_any1 = std::is_same_v<T1, Any>;
        std::conditional_t<is_any1, Value, T1> v1;
        if constexpr (is_any1) v1 = args[0];
        else v1 = std::get<T1>(args[0]);


        constexpr bool is_any2 = std::is_same_v<T2, Any>;
        std::conditional_t<is_any2, Value, T2> v2;
        if constexpr (is_any2) v2 = args[1];
        else v2 = std::get<T2>(args[1]);


        return func.func(v1, v2, that);
    }

    puts("Args:");
    that->print(args[0]);
    that->print(args[1]);
    error("Wrong type passed to function!");
}

template <size_t SIZE, size_t N = 0, typename... Ts>
requires (SIZE == 3)
Value execute(Func<Ts...> func, const std::vector<Value>& args, const auto& that) {
    if constexpr (N < decltype(func)::count) {
        using T1 = decltype(func.template get2<N, 0>());
        using T2 = decltype(func.template get2<N, 1>());
        using T3 = decltype(func.template get2<N, 2>());

        if constexpr (not std::is_same_v<T1, Any>)
            if (not std::holds_alternative<T1>(args[0])) 
                return execute<SIZE, N + 1>(func, args, that); // try the next type list

        if constexpr (not std::is_same_v<T2, Any>)
            if (not std::holds_alternative<T2>(args[1])) 
                return execute<SIZE, N + 1>(func, args, that);

        if constexpr (not std::is_same_v<T3, Any>)
            if (not std::holds_alternative<T3>(args[2])) 
                return execute<SIZE, N + 1>(func, args, that);


        constexpr bool is_any1 = std::is_same_v<T1, Any>;
        std::conditional_t<is_any1, Value, T1> v1;
        if constexpr (is_any1) v1 = args[0];
        else v1 = std::get<T1>(args[0]);


        constexpr bool is_any2 = std::is_same_v<T2, Any>;
        std::conditional_t<is_any2, Value, T2> v2;
        if constexpr (is_any2) v2 = args[1];
        else v2 = std::get<T2>(args[1]);


        constexpr bool is_any3 = std::is_same_v<T3, Any>;
        std::conditional_t<is_any3, Value, T3> v3;
        if constexpr (is_any3) v3 = args[2];
        else v3 = std::get<T3>(args[2]);


        return func.func(v1, v2, v3, that);

    }

    else error("Wrong type passed to function!");
}
