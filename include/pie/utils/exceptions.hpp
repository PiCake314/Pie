#pragma once

#include <exception>
#include <stdexcept>
#include <string>

namespace except {
    class TypeMismatch : public std::exception {
        std::string err;

      public:
        explicit TypeMismatch(std::string msg) noexcept : err{std::move(msg)} {};

        const char* what() const noexcept override { return err.c_str(); }
    };
} // namespace except
