// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "../SqlError.hpp"
#include "ColumnType.hpp"
#include "Detail.hpp"

#include <format>
#include <string_view>

class SqlStatement;

namespace Model
{
struct SqlColumnNameView
{
    std::string_view name;
};
} // namespace Model

template <>
struct std::formatter<Model::SqlColumnNameView>: std::formatter<std::string>
{
    auto format(Model::SqlColumnNameView const& column, format_context& ctx) const -> format_context::iterator
    {
        return std::formatter<string>::format(std::format("\"{}\"", column.name), ctx);
    }
};

namespace Model
{

enum class FieldValueRequirement : uint8_t
{
    NULLABLE,
    NOT_NULL,
};

constexpr inline FieldValueRequirement SqlNullable = FieldValueRequirement::NULLABLE;
constexpr inline FieldValueRequirement SqlNotNullable = FieldValueRequirement::NULLABLE;

struct AbstractRecord;

// Base class for all fields in a table row (Record).
class AbstractField
{
  public:
    AbstractField(AbstractRecord& record,
                  SQLSMALLINT index,
                  std::string_view name,
                  SqlColumnType type,
                  FieldValueRequirement requirement):
        m_record { &record },
        m_index { index },
        m_name { name },
        m_type { type },
        m_requirement { requirement }
    {
    }

    AbstractField() = delete;
    AbstractField(AbstractField&&) = default;
    AbstractField& operator=(AbstractField&&) = default;
    AbstractField(AbstractField const&) = delete;
    AbstractField& operator=(AbstractField const&) = delete;
    virtual ~AbstractField() = default;

    // Returns the syntax for the SQL constraint specification for this field, if any, otherwise an empty string.
    [[nodiscard]] virtual std::string SqlConstraintSpecifier() const
    {
        return "";
    }

    [[nodiscard]] virtual std::string InspectValue() const = 0;
    virtual void BindInputParameter(SQLSMALLINT parameterIndex, SqlStatement& stmt) const = 0;
    virtual void BindOutputColumn(SqlStatement& stmt) = 0;
    virtual void BindOutputColumn(SQLSMALLINT outputIndex, SqlStatement& stmt) = 0;
    virtual void LoadValueFrom(AbstractField& other) = 0;

    // clang-format off
    AbstractRecord& GetRecord() noexcept { return *m_record; }
    [[nodiscard]] AbstractRecord const& GetRecord() const noexcept { return *m_record; }
    void SetRecord(AbstractRecord& record) noexcept { m_record = &record; }
    [[nodiscard]] bool IsModified() const noexcept { return m_modified; }
    void SetModified(bool value) noexcept { m_modified = value; }
    [[nodiscard]] SQLSMALLINT Index() const noexcept { return m_index; }
    [[nodiscard]] SqlColumnNameView Name() const noexcept { return m_name; }
    [[nodiscard]] SqlColumnType Type() const noexcept { return m_type; }
    [[nodiscard]] bool IsNullable() const noexcept { return m_requirement == FieldValueRequirement::NULLABLE; }
    [[nodiscard]] bool IsRequired() const noexcept { return m_requirement == FieldValueRequirement::NOT_NULL; }
    // clang-format on

  private:
    AbstractRecord* m_record;
    SQLSMALLINT m_index;
    SqlColumnNameView m_name;
    SqlColumnType m_type;
    FieldValueRequirement m_requirement;
    bool m_modified = false;
};

} // namespace Model
