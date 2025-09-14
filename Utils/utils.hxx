#pragma once

#include <iostream>
#include <source_location>
#include <unordered_map>
#include <variant>
#include <format>
#include <print>
#include <type_traits>

#include <stdx/tuple.hpp>

#include "../Token/Token.hxx"


// not sure where to put it
// keep it here for now...

[[noreturn]] inline void error(
    const std::string& msg = "[no diagnostic]. If you see this, please file a bug report!",
    const std::source_location& location = std::source_location::current()) noexcept {
    // puts(msg.c_str());

    // std::cerr << location.file_name()  << ", Line: " << location.line() << ", Function: " << location.function_name() << '';
    std::println(std::cerr, "\033[1m{}:{}:{}: \033[31merror:\033[0m {}", location.file_name(), location.line(), location.column(), msg);

    exit(1);
}

#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

inline void trace() {
    void *callstack[128]; // Array to store addresses
    int frames = backtrace(callstack, 128); // Get addresses
    char **symbols = backtrace_symbols(callstack, frames); // Get symbol names

    if (symbols == NULL) {
        perror("backtrace_symbols");
        return;
    }

    fprintf(stderr, "Stack Trace:\n");
    for (int i = 0; i < frames; ++i) {
        fprintf(stderr, "%s\n", symbols[i]);
    }

    free(symbols);
}


[[noreturn]] inline void expected(const TokenKind exp, const Token& got, const std::source_location& location = std::source_location::current()) noexcept {
    error(std::string{"Expected token "} + stringify(exp) + " and found " + stringify(got.kind) + ": " + got.text, location);
}

[[noreturn]] inline void expected(const TokenKind exp, const TokenKind got, const std::source_location& location = std::source_location::current()) noexcept {
    error(std::string{"Expected token "} + stringify(exp) + " and found " + stringify(got), location);
}



