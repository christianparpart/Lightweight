// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <format>
#include <stdexcept>
#include <string_view>

class SqlRequireLoadedError: public std::runtime_error
{
  public:
    explicit SqlRequireLoadedError(std::string_view columnType):
        std::runtime_error(std::format("Could not load the data record: {}", columnType))
    {
    }
};
