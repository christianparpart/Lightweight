// SPDX-License-Identifier: MIT
#pragma once

#include "Core.hpp"

#include <string>

// Specialized traits for std::string as output string parameter
template <>
struct SqlOutputStringTraits<std::string>
{
    static char const* Data(std::string const* str) noexcept
    {
        return str->data();
    }

    static char* Data(std::string* str) noexcept
    {
        return str->data();
    }

    static SQLULEN Size(std::string const* str) noexcept
    {
        return str->size();
    }

    static void Clear(std::string* str) noexcept
    {
        str->clear();
    }

    static void Reserve(std::string* str, size_t capacity) noexcept
    {
        // std::string tries to defer the allocation as long as possible.
        // So we first tell std::string how much to reserve and then resize it to the *actually* reserved
        // size.
        str->reserve(capacity);
        str->resize(str->capacity());
    }

    static void Resize(std::string* str, SQLLEN indicator) noexcept
    {
        if (indicator > 0)
            str->resize(indicator);
    }
};

#include "OutputStringBinder.hpp"
