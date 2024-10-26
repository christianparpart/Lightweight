// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../Api.hpp"
#include "../SqlDataBinder.hpp"
#include "../SqlQueryFormatter.hpp"

#include <ranges>

// SqlWildcardType is a placeholder for an explicit wildcard input parameter in a SQL query.
//
// Use this in the SqlQueryBuilder::Where method to insert a '?' placeholder for a wildcard.
struct SqlWildcardType
{
};

static constexpr inline auto SqlWildcard = SqlWildcardType {};

namespace detail
{

struct RawSqlCondition
{
    std::string condition;
};

} // namespace detail

struct SqlQualifiedTableColumnName
{
    std::string_view tableName;
    std::string_view columnName;
};

namespace detail
{

template <typename ColumnName>
std::string MakeSqlColumnName(ColumnName const& columnName)
{
    using namespace std::string_view_literals;
    std::string output;

    if constexpr (std::is_same_v<ColumnName, SqlQualifiedTableColumnName>)
    {
        output.reserve(columnName.tableName.size() + columnName.columnName.size() + 5);
        output += '"';
        output += columnName.tableName;
        output += R"(".")"sv;
        output += columnName.columnName;
        output += '"';
    }
    else
    {
        output += '"';
        output += columnName;
        output += '"';
    }
    return output;
}

} // namespace detail

struct [[nodiscard]] SqlSearchCondition
{
    std::string tableName;
    std::string tableAlias;
    std::string tableJoins;
    std::string condition;
    std::vector<SqlVariant>* inputBindings = nullptr;
};

namespace detail
{

// Helper CRTP-based class for building WHERE clauses.
//
// This class is inherited by the SqlSelectQueryBuilder, SqlUpdateQueryBuilder, and SqlDeleteQueryBuilder
template <typename Derived>
class [[nodiscard]] SqlWhereClauseBuilder
{
  public:
    // Indicates, that the next WHERE clause should be AND-ed (default).
    [[nodiscard]] Derived& And() noexcept;

    // Indicates, that the next WHERE clause should be OR-ed.
    [[nodiscard]] Derived& Or() noexcept;

    // Indicates, that the next WHERE clause should be negated.
    [[nodiscard]] Derived& Not() noexcept;

    // Constructs or extends a raw WHERE clause.
    [[nodiscard]] Derived& WhereRaw(std::string_view sqlConditionExpression);

    // Constructs or extends a WHERE clause to test for a binary operation.
    template <typename ColumnName, typename T>
    [[nodiscard]] Derived& Where(ColumnName const& columnName, std::string_view binaryOp, T const& value);

    // Constructs or extends a WHERE clause to test for a binary operation for RHS as string literal.
    template <typename ColumnName, std::size_t N>
    Derived& Where(ColumnName const& columnName, std::string_view binaryOp, char const (&value)[N]);

    // Constructs or extends a WHERE clause to test for equality.
    template <typename ColumnName, typename T>
    [[nodiscard]] Derived& Where(ColumnName const& columnName, T const& value);

    // Constructs or extends a WHERE/AND clause to test for a group of values.
    template <typename Callable>
        requires std::invocable<Callable, SqlWhereClauseBuilder<Derived>&>
    [[nodiscard]] Derived& Where(Callable const& callable);

    // Constructs or extends an WHERE/OR clause to test for a group of values.
    template <typename Callable>
        requires std::invocable<Callable, SqlWhereClauseBuilder<Derived>&>
    [[nodiscard]] Derived& OrWhere(Callable const& callable);

    template <typename ColumnName, std::ranges::input_range InputRange>
    [[nodiscard]] Derived& WhereIn(ColumnName const& columnName, InputRange const& values);

    template <typename ColumnName, typename T>
    [[nodiscard]] Derived& WhereIn(ColumnName const& columnName, std::initializer_list<T> const& values);

    template <typename ColumnName>
    [[nodiscard]] Derived& WhereNull(ColumnName const& columnName);

    template <typename ColumnName>
    [[nodiscard]] Derived& WhereNotNull(ColumnName const& columnName);

    template <typename ColumnName>
    [[nodiscard]] Derived& WhereTrue(ColumnName const& columnName);

    template <typename ColumnName>
    [[nodiscard]] Derived& WhereFalse(ColumnName const& columnName);

    // Construts or extends a WHERE clause to test for a binary operation between two columns.
    template <typename LeftColumn, typename RightColumn>
    [[nodiscard]] Derived& WhereColumn(LeftColumn const& left, std::string_view binaryOp, RightColumn const& right);

    // Constructs an INNER JOIN clause.
    [[nodiscard]] Derived& InnerJoin(std::string_view joinTable,
                                     std::string_view joinColumnName,
                                     SqlQualifiedTableColumnName onOtherColumn);

    // Constructs an INNER JOIN clause.
    [[nodiscard]] Derived& InnerJoin(std::string_view joinTable,
                                     std::string_view joinColumnName,
                                     std::string_view onMainTableColumn);

    // Constructs an LEFT OUTER JOIN clause.
    [[nodiscard]] Derived& LeftOuterJoin(std::string_view joinTable,
                                         std::string_view joinColumnName,
                                         SqlQualifiedTableColumnName onOtherColumn);

    // Constructs an LEFT OUTER JOIN clause.
    [[nodiscard]] Derived& LeftOuterJoin(std::string_view joinTable,
                                         std::string_view joinColumnName,
                                         std::string_view onMainTableColumn);

    // Constructs an RIGHT OUTER JOIN clause.
    [[nodiscard]] Derived& RightOuterJoin(std::string_view joinTable,
                                          std::string_view joinColumnName,
                                          SqlQualifiedTableColumnName onOtherColumn);

    // Constructs an RIGHT OUTER JOIN clause.
    [[nodiscard]] Derived& RightOuterJoin(std::string_view joinTable,
                                          std::string_view joinColumnName,
                                          std::string_view onMainTableColumn);

    // Constructs an FULL OUTER JOIN clause.
    [[nodiscard]] Derived& FullOuterJoin(std::string_view joinTable,
                                         std::string_view joinColumnName,
                                         SqlQualifiedTableColumnName onOtherColumn);

    // Constructs an FULL OUTER JOIN clause.
    [[nodiscard]] Derived& FullOuterJoin(std::string_view joinTable,
                                         std::string_view joinColumnName,
                                         std::string_view onMainTableColumn);

  private:
    SqlSearchCondition& SearchCondition() noexcept;
    [[nodiscard]] SqlQueryFormatter const& Formatter() const noexcept;

    enum class WhereJunctor : uint8_t
    {
        Null,
        Where,
        And,
        Or,
    };

    WhereJunctor m_nextWhereJunctor = WhereJunctor::Where;
    bool m_nextIsNot = false;

    void AppendWhereJunctor();

    template <typename ColumnName>
        requires(std::is_same_v<ColumnName, SqlQualifiedTableColumnName>
                 || std::is_convertible_v<ColumnName, std::string_view>
                 || std::is_convertible_v<ColumnName, std::string>)
    void AppendColumnName(ColumnName const& columnName);

    enum class JoinType : uint8_t
    {
        INNER,
        LEFT,
        RIGHT,
        FULL
    };

    // Constructs a JOIN clause.
    [[nodiscard]] Derived& Join(JoinType joinType,
                                std::string_view joinTable,
                                std::string_view joinColumnName,
                                SqlQualifiedTableColumnName onOtherColumn);

    // Constructs a JOIN clause.
    [[nodiscard]] Derived& Join(JoinType joinType,
                                std::string_view joinTable,
                                std::string_view joinColumnName,
                                std::string_view onMainTableColumn);
};

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::And() noexcept
{
    m_nextWhereJunctor = WhereJunctor::And;
    return static_cast<Derived&>(*this);
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::Or() noexcept
{
    m_nextWhereJunctor = WhereJunctor::Or;
    return static_cast<Derived&>(*this);
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::Not() noexcept
{
    m_nextIsNot = !m_nextIsNot;
    return static_cast<Derived&>(*this);
}

template <typename Derived>
template <typename ColumnName, typename T>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::Where(ColumnName const& columnName,
                                                                               T const& value)
{
    return Where(columnName, "=", value);
}

template <typename Derived>
template <typename Callable>
    requires std::invocable<Callable, SqlWhereClauseBuilder<Derived>&>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::OrWhere(Callable const& callable)
{
    return Or().Where(callable);
}

template <typename Derived>
template <typename Callable>
    requires std::invocable<Callable, SqlWhereClauseBuilder<Derived>&>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::Where(Callable const& callable)
{
    auto& condition = SearchCondition().condition;

    auto const originalSize = condition.size();

    AppendWhereJunctor();
    m_nextWhereJunctor = WhereJunctor::Null;
    condition += '(';

    auto const sizeBeforeCallable = condition.size();

    (void) callable(*this);

    if (condition.size() == sizeBeforeCallable)
        condition.resize(originalSize);
    else
        condition += ')';

    return static_cast<Derived&>(*this);
}

inline LIGHTWEIGHT_FORCE_INLINE RawSqlCondition PopulateSqlSetExpression(auto const& values)
{
    using namespace std::string_view_literals;
    std::ostringstream fragment;
    fragment << '(';
    for (auto const&& [index, value]: values | std::views::enumerate)
    {
        if (index > 0)
            fragment << ", "sv;
        fragment << value;
    }
    fragment << ')';
    return RawSqlCondition { fragment.str() };
}

template <typename Derived>
template <typename ColumnName, std::ranges::input_range InputRange>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::WhereIn(ColumnName const& columnName,
                                                                                 InputRange const& values)
{
    return Where(columnName, "IN", detail::PopulateSqlSetExpression(values));
}

template <typename Derived>
template <typename ColumnName, typename T>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::WhereIn(ColumnName const& columnName,
                                                                                 std::initializer_list<T> const& values)
{
    return Where(columnName, "IN", detail::PopulateSqlSetExpression(values));
}

template <typename Derived>
template <typename ColumnName>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::WhereNotNull(ColumnName const& columnName)
{
    return Where(columnName, "!=", "NULL");
}

template <typename Derived>
template <typename ColumnName>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::WhereNull(ColumnName const& columnName)
{
    return Where(columnName, "=", "NULL");
}

template <typename Derived>
template <typename ColumnName>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::WhereTrue(ColumnName const& columnName)
{
    return Where(columnName, "=", true);
}

template <typename Derived>
template <typename ColumnName>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::WhereFalse(ColumnName const& columnName)
{
    return Where(columnName, "=", false);
}

template <typename Derived>
template <typename LeftColumn, typename RightColumn>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::WhereColumn(LeftColumn const& left,
                                                                                     std::string_view binaryOp,
                                                                                     RightColumn const& right)
{
    AppendWhereJunctor();

    AppendColumnName(left);
    SearchCondition().condition += ' ';
    SearchCondition().condition += binaryOp;
    SearchCondition().condition += ' ';
    AppendColumnName(right);

    return static_cast<Derived&>(*this);
}

template <typename T>
struct WhereConditionLiteralType
{
    constexpr static bool needsQuotes = !std::is_integral_v<T> && !std::is_floating_point_v<T>;
};

template <typename Derived>
template <typename ColumnName, std::size_t N>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::Where(ColumnName const& columnName,
                                                                               std::string_view binaryOp,
                                                                               char const (&value)[N])
{
    return Where(columnName, binaryOp, std::string_view { value, N - 1 });
}

template <typename Derived>
template <typename ColumnName, typename T>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::Where(ColumnName const& columnName,
                                                                               std::string_view binaryOp,
                                                                               T const& value)
{
    auto& searchCondition = SearchCondition();

    AppendWhereJunctor();
    AppendColumnName(columnName);
    searchCondition.condition += ' ';
    searchCondition.condition += binaryOp;
    searchCondition.condition += ' ';

    if constexpr (std::is_same_v<T, SqlWildcardType>)
    {
        searchCondition.condition += '?';
    }
    else if constexpr (std::is_same_v<T, detail::RawSqlCondition>)
    {
        searchCondition.condition += value.condition;
    }
    else if (searchCondition.inputBindings)
    {
        searchCondition.condition += '?';
        searchCondition.inputBindings->emplace_back(value);
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        searchCondition.condition += Formatter().BooleanLiteral(value);
    }
    else if constexpr (!WhereConditionLiteralType<T>::needsQuotes)
    {
        searchCondition.condition += std::format("{}", value);
    }
    else
    {
        // TODO: Escape single quotes
        searchCondition.condition += '\'';
        searchCondition.condition += std::format("{}", value);
        searchCondition.condition += '\'';
    }

    return static_cast<Derived&>(*this);
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::InnerJoin(
    std::string_view joinTable, std::string_view joinColumnName, SqlQualifiedTableColumnName onOtherColumn)
{
    return Join(JoinType::INNER, joinTable, joinColumnName, onOtherColumn);
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::InnerJoin(std::string_view joinTable,
                                                                                   std::string_view joinColumnName,
                                                                                   std::string_view onMainTableColumn)
{
    return Join(JoinType::INNER, joinTable, joinColumnName, onMainTableColumn);
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::LeftOuterJoin(
    std::string_view joinTable, std::string_view joinColumnName, SqlQualifiedTableColumnName onOtherColumn)
{
    return Join(JoinType::LEFT, joinTable, joinColumnName, onOtherColumn);
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::LeftOuterJoin(
    std::string_view joinTable, std::string_view joinColumnName, std::string_view onMainTableColumn)
{
    return Join(JoinType::LEFT, joinTable, joinColumnName, onMainTableColumn);
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::RightOuterJoin(
    std::string_view joinTable, std::string_view joinColumnName, SqlQualifiedTableColumnName onOtherColumn)
{
    return Join(JoinType::RIGHT, joinTable, joinColumnName, onOtherColumn);
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::RightOuterJoin(
    std::string_view joinTable, std::string_view joinColumnName, std::string_view onMainTableColumn)
{
    return Join(JoinType::RIGHT, joinTable, joinColumnName, onMainTableColumn);
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::FullOuterJoin(
    std::string_view joinTable, std::string_view joinColumnName, SqlQualifiedTableColumnName onOtherColumn)
{
    return Join(JoinType::FULL, joinTable, joinColumnName, onOtherColumn);
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::FullOuterJoin(
    std::string_view joinTable, std::string_view joinColumnName, std::string_view onMainTableColumn)
{
    return Join(JoinType::FULL, joinTable, joinColumnName, onMainTableColumn);
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::WhereRaw(
    std::string_view sqlConditionExpression)
{
    AppendWhereJunctor();

    auto& condition = SearchCondition().condition;
    condition += sqlConditionExpression;

    return static_cast<Derived&>(*this);
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE SqlSearchCondition& SqlWhereClauseBuilder<Derived>::SearchCondition() noexcept
{
    return static_cast<Derived*>(this)->SearchCondition();
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE SqlQueryFormatter const& SqlWhereClauseBuilder<Derived>::Formatter() const noexcept
{
    return static_cast<Derived const*>(this)->Formatter();
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE void SqlWhereClauseBuilder<Derived>::AppendWhereJunctor()
{
    using namespace std::string_view_literals;

    auto& condition = SearchCondition().condition;

    switch (m_nextWhereJunctor)
    {
        case WhereJunctor::Null:
            break;
        case WhereJunctor::Where:
            condition += "\n WHERE "sv;
            break;
        case WhereJunctor::And:
            condition += " AND "sv;
            break;
        case WhereJunctor::Or:
            condition += " OR "sv;
            break;
    }

    if (m_nextIsNot)
    {
        condition += "NOT "sv;
        m_nextIsNot = false;
    }

    m_nextWhereJunctor = WhereJunctor::And;
}

template <typename Derived>
template <typename ColumnName>
    requires(std::is_same_v<ColumnName, SqlQualifiedTableColumnName>
             || std::is_convertible_v<ColumnName, std::string_view> || std::is_convertible_v<ColumnName, std::string>)
inline LIGHTWEIGHT_FORCE_INLINE void SqlWhereClauseBuilder<Derived>::AppendColumnName(ColumnName const& columnName)
{
    SearchCondition().condition += detail::MakeSqlColumnName(columnName);
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::Join(JoinType joinType,
                                                                              std::string_view joinTable,
                                                                              std::string_view joinColumnName,
                                                                              SqlQualifiedTableColumnName onOtherColumn)
{
    static constexpr std::array<std::string_view, 4> JoinTypeStrings = {
        "INNER",
        "LEFT OUTER",
        "RIGHT OUTER",
        "FULL OUTER",
    };

    SearchCondition().tableJoins += std::format("\n"
                                                R"( {0} JOIN "{1}" ON "{1}"."{2}" = "{3}"."{4}")",
                                                JoinTypeStrings[static_cast<std::size_t>(joinType)],
                                                joinTable,
                                                joinColumnName,
                                                onOtherColumn.tableName,
                                                onOtherColumn.columnName);
    return static_cast<Derived&>(*this);
}

template <typename Derived>
inline LIGHTWEIGHT_FORCE_INLINE Derived& SqlWhereClauseBuilder<Derived>::Join(JoinType joinType,
                                                                              std::string_view joinTable,
                                                                              std::string_view joinColumnName,
                                                                              std::string_view onMainTableColumn)
{
    return Join(
        joinType,
        joinTable,
        joinColumnName,
        SqlQualifiedTableColumnName { .tableName = SearchCondition().tableName, .columnName = onMainTableColumn });
}

} // namespace detail
