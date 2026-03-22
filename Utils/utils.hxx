#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <source_location>
#include <unordered_map>
#include <variant>
#include <format>
#include <print>
#include <concepts>
#include <type_traits>
#include <stdexcept>

#include "../Lex/Token.hxx"



inline namespace pie {
namespace util {


template <typename Except = std::runtime_error, bool print_loc = true>
[[noreturn]] inline void error(
    const std::string_view msg = "[no diagnostic]. If you see this, please file a bug report!",
    [[maybe_unused]] const std::source_location& location = std::source_location::current()
)
{
    #if not NO_ERR_LOC
    if constexpr (print_loc)
        std::print(std::cerr, "\033[1m{}:{}:{}: \033[31merror:\033[0m ", location.file_name(), location.line(), location.column());
    #endif

    // std::println(std::cerr, "{}", msg);

    // exit(1);

    throw Except{std::string{msg}};
}


[[noreturn]] inline void expected(const TokenKind exp, const Token& got, const std::source_location& location = std::source_location::current()) {
    error(std::string{"Expected token "} + stringify(exp) + " and found " + stringify(got.kind) + ": " + got.text, location);
}

[[noreturn]] inline void expected(const TokenKind exp, const TokenKind got, const std::source_location& location = std::source_location::current()) {
    error(std::string{"Expected token "} + stringify(exp) + " and found " + stringify(got), location);
}

[[noreturn]] inline void expected(const std::string& exp, const Token& got, const std::source_location& location = std::source_location::current()) {
    error(std::string{"Expected '"} + exp + "' and found " + stringify(got.kind) + ": " + got.text, location);
}



template <typename F>
struct Deferred {
    F f;

    Deferred(std::invocable auto func) : f{std::move(func)} {};
    ~Deferred() { f(); }
};

template <typename F>
Deferred(F) -> Deferred<F>;




[[nodiscard]] inline std::string readFile(const std::string& fname) {
    const std::ifstream fin{fname};

    if (not fin.is_open()) error("File \"" + fname + " \" not found!");

    std::stringstream ss;
    ss << fin.rdbuf();

    return ss.str();
}


} // namespace util
} // namespace pie