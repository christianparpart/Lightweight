// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string_view>
#include <vector>

namespace Model
{

class AbstractField;
struct AbstractRecord;

class QueryLogger
{
  public:
    virtual ~QueryLogger() = default;

    using FieldList = std::vector<AbstractField*>;

    virtual void QueryStart(std::string_view /*query*/, FieldList const& /*output*/) {}
    virtual void QueryNextRow(AbstractRecord const& /*model*/) {}
    virtual void QueryEnd() {}

    static void Set(QueryLogger* next) noexcept
    {
        m_instance = next;
    }

    static QueryLogger& Get() noexcept
    {
        return *m_instance;
    }

    static QueryLogger* NullLogger() noexcept;
    static QueryLogger* StandardLogger() noexcept;

  private:
    static QueryLogger* m_instance;
};

namespace detail
{

    struct SqlScopedModelQueryLogger
    {
        using FieldList = QueryLogger::FieldList;

        SqlScopedModelQueryLogger(std::string_view query, FieldList const& output)
        {
            QueryLogger::Get().QueryStart(query, output);
        }

        SqlScopedModelQueryLogger& operator+=(AbstractRecord const& model)
        {
            QueryLogger::Get().QueryNextRow(model);
            return *this;
        }

        ~SqlScopedModelQueryLogger()
        {
            QueryLogger::Get().QueryEnd();
        }
    };

} // namespace detail

} // namespace Model
