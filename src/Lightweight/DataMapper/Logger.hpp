// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "../Api.hpp"

#include <string_view>
#include <vector>

class AbstractField;
struct AbstractRecord;

class QueryLogger
{
  public:
    virtual ~QueryLogger() = default;

    using FieldList = std::vector<AbstractField*>;

    LIGHTWEIGHT_API virtual void QueryStart(std::string_view /*query*/, FieldList const& /*output*/) {}
    LIGHTWEIGHT_API virtual void QueryNextRow(AbstractRecord const& /*DataMapper*/) {}
    LIGHTWEIGHT_API virtual void QueryEnd() {}

    LIGHTWEIGHT_API static void Set(QueryLogger* next) noexcept
    {
        m_instance = next;
    }

    LIGHTWEIGHT_API static QueryLogger& Get() noexcept
    {
        return *m_instance;
    }

    LIGHTWEIGHT_API static QueryLogger* NullLogger() noexcept;
    LIGHTWEIGHT_API static QueryLogger* StandardLogger() noexcept;

  private:
    LIGHTWEIGHT_API static QueryLogger* m_instance;
};

namespace detail
{

    struct SqlScopedModelQueryLogger
    {
        using FieldList = QueryLogger::FieldList;

        LIGHTWEIGHT_FORCE_INLINE SqlScopedModelQueryLogger(std::string_view query, FieldList const& output)
        {
            QueryLogger::Get().QueryStart(query, output);
        }

        // TODO(pr): find equivalent to that
        //           probably through Reflection::Inspect()
        // SqlScopedModelQueryLogger& operator+=(AbstractRecord const& record)
        // {
        //     QueryLogger::Get().QueryNextRow(record);
        //     return *this;
        // }

        LIGHTWEIGHT_FORCE_INLINE ~SqlScopedModelQueryLogger()
        {
            QueryLogger::Get().QueryEnd();
        }
    };

} // namespace detail
