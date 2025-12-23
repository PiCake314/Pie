#pragma once

#include <string>
#include <stdexcept>
#include <exception>


namespace except {
    class TypeMismatch : std::exception {
        std::string err;
        explicit TypeMismatch(std::string msg) noexcept : err{std::move(msg)} {};

        const char* what() const noexcept override { return err.c_str(); }
    };
}

