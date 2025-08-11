#pragma once

#include "Token.hxx"
#include <iostream>
#include <source_location>
#include <unordered_map>
#include <format>
#include <print>

// not sure where to put it
// keep it here for now...
[[noreturn]] inline void error(const std::string& msg = {}, [[maybe_unused]] const std::source_location& location = std::source_location::current()) noexcept {
    puts(msg.c_str());

    std::cout
        << "(File: " << location.file_name() 
        << ", Line: " << location.line() 
        << ", Function: " << location.function_name()
        << ")\n";

    exit(1);
}



[[noreturn]] inline void expected(const TokenKind exp, const TokenKind got, const std::source_location& location = std::source_location::current()) noexcept {
    using std::operator""s;
    error("Expected token "s + stringify(exp) + " and found "s + stringify(got), location);
}


