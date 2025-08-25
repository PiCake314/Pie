#pragma once

#include <optional>
#include <stdexcept>
#include <type_traits>


template <typename T>
class Late {
    std::optional<T> value;

public:
    Late() noexcept : value{} {};
    Late(const T& v) noexcept(std::is_nothrow_copy_constructible_v<T>) : value{v} {};
    Late(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>) : value{std::move(v)} {};
    Late& operator=(const T& v) noexcept(std::is_nothrow_copy_assignable_v<T>) { value = v; return *this;};
    Late& operator=(T&& v) noexcept(std::is_nothrow_move_assignable_v<T>) { value = std::move(v); return *this; };


    operator T() {
        if(not value) throw std::runtime_error("Use bofore initialization!");
        return *value;
    }
};
