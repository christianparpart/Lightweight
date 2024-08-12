#pragma once

#include <vector>
#include <string_view>

namespace Model
{

class SqlModelFieldBase;

class SqlModelQueryLogger
{
  public:
    virtual ~SqlModelQueryLogger() = default;

    using FieldList = std::vector<SqlModelFieldBase*>;

    virtual void QueryStart(std::string_view /*query*/, FieldList const& /*output*/) {};
    virtual void QueryEnd() {}

    static void Set(SqlModelQueryLogger* next) noexcept
    {
        m_instance = next;
    }

    static SqlModelQueryLogger& Get() noexcept
    {
        return *m_instance;
    }

    static SqlModelQueryLogger* NullLogger() noexcept;
    static SqlModelQueryLogger* StandardLogger() noexcept;

  private:
    static SqlModelQueryLogger* m_instance;
};

namespace detail
{

struct SqlScopedModelQueryLogger
{
    using FieldList = SqlModelQueryLogger::FieldList;

    SqlScopedModelQueryLogger(std::string_view query, FieldList const& output)
    {
        SqlModelQueryLogger::Get().QueryStart(query, output);
    }

    ~SqlScopedModelQueryLogger()
    {
        SqlModelQueryLogger::Get().QueryEnd();
    }
};

} // namespace detail

} // namespace Model
