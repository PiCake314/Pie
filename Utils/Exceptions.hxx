#pragma once

#include <string>
#include <stdexcept>
#include <exception>


#define DefineError(NAME)                                             \
    class NAME : public std::exception {                               \
        std::string err;                                                \
    public:                                                              \
        explicit NAME(std::string msg) noexcept : err{std::move(msg)} {}; \
        const char* what() const noexcept override { return err.c_str(); } \
    }                                                                       \


inline namespace pie {

namespace except {
    // class TypeMismatch : public std::exception {
    //     std::string err;
    // public:
    //     explicit TypeMismatch(std::string msg) noexcept : err{std::move(msg)} {};

    //     const char* what() const noexcept override { return err.c_str(); }
    // };

    DefineError(TypeMismatch);
    DefineError(NameLookup  );
}


} // namespace pie

#undef DefineError