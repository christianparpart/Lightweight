// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"

enum class SqlResultOrdering : uint8_t
{
    ASCENDING,
    DESCENDING
};

class [[nodiscard]] SqlSelectQueryBuilder final: public detail::SqlWhereClauseBuilder<SqlSelectQueryBuilder>
{
  public:
    enum class SelectType
    {
        Undefined,
        Count,
        All,
        First,
        Range
    };

    struct ComposedQuery
    {
        SelectType selectType = SelectType::Undefined;
        SqlQueryFormatter const* formatter = nullptr;

        bool distinct = false;
        SqlSearchCondition searchCondition {};

        std::string fields;

        std::string orderBy;
        std::string groupBy;

        size_t offset = 0;
        size_t limit = (std::numeric_limits<size_t>::max)();

        [[nodiscard]] std::string ToSql() const;
    };

    explicit SqlSelectQueryBuilder(SqlQueryFormatter const& formatter,
                                   std::string table,
                                   std::string tableAlias) noexcept:
        detail::SqlWhereClauseBuilder<SqlSelectQueryBuilder> {},
        m_formatter { formatter }
    {
        m_query.formatter = &formatter;
        m_query.searchCondition.tableName = std::move(table);
        m_query.searchCondition.tableAlias = std::move(tableAlias);
        m_query.fields.reserve(256);
    }

    // Adds a DISTINCT clause to the SELECT query.
    SqlSelectQueryBuilder& Distinct() noexcept;

    // Adds a sequence of columns to the SELECT clause.
    template <typename... MoreFields>
    SqlSelectQueryBuilder& Fields(std::string_view const& firstField, MoreFields&&... moreFields);

    // Adds a single column to the SELECT clause.
    SqlSelectQueryBuilder& Field(std::string_view const& fieldName);

    // Adds a single column to the SELECT clause.
    SqlSelectQueryBuilder& Field(SqlQualifiedTableColumnName const& fieldName);

    // Adds a single column to the SELECT clause.
    SqlSelectQueryBuilder& Fields(std::vector<std::string_view> const& fieldNames);

    // Adds a sequence of columns from the given table to the SELECT clause.
    SqlSelectQueryBuilder& Fields(std::vector<std::string_view> const& fieldNames, std::string_view tableName);

    // Adds a single column with an alias to the SELECT clause.
    SqlSelectQueryBuilder& FieldAs(std::string_view const& fieldName, std::string_view const& alias);

    // Adds a single column with an alias to the SELECT clause.
    SqlSelectQueryBuilder& FieldAs(SqlQualifiedTableColumnName const& fieldName, std::string_view const& alias);

    // Constructs or extends a ORDER BY clause.
    SqlSelectQueryBuilder& OrderBy(std::string_view columnName,
                                   SqlResultOrdering ordering = SqlResultOrdering::ASCENDING);

    // Constructs or extends a GROUP BY clause.
    SqlSelectQueryBuilder& GroupBy(std::string_view columnName);

    // Finalizes building the query as SELECT COUNT(*) ... query.
    ComposedQuery Count();

    // Finalizes building the query as SELECT field names FROM ... query.
    ComposedQuery All();

    // Finalizes building the query as SELECT TOP n field names FROM ... query.
    ComposedQuery First(size_t count = 1);

    // Finalizes building the query as SELECT field names FROM ... query with a range.
    ComposedQuery Range(std::size_t offset, std::size_t limit);

    SqlSearchCondition& SearchCondition() noexcept
    {
        return m_query.searchCondition;
    }

    [[nodiscard]] SqlQueryFormatter const& Formatter() const noexcept
    {
        return m_formatter;
    }

  private:
    SqlQueryFormatter const& m_formatter;
    ComposedQuery m_query;
};

template <typename... MoreFields>
SqlSelectQueryBuilder& SqlSelectQueryBuilder::Fields(std::string_view const& firstField, MoreFields&&... moreFields)
{
    using namespace std::string_view_literals;

    std::ostringstream fragment;

    if (!m_query.fields.empty())
        fragment << ", "sv;

    fragment << '"' << firstField << '"';

    if constexpr (sizeof...(MoreFields) > 0)
        ((fragment << R"(, ")"sv << std::forward<MoreFields>(moreFields) << '"') << ...);

    m_query.fields += fragment.str();
    return *this;
}
