// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdexcept>
#if defined(_WIN32) || defined(_WIN64)
    #include <Windows.h>
#endif

#include "Api.hpp"
#include "SqlConnection.hpp"
#include "SqlDataBinder.hpp"
#include "SqlQuery.hpp"
#include "Utils.hpp"

#include <cstring>
#include <optional>
#include <ranges>
#include <source_location>
#include <type_traits>
#include <vector>

#include <sql.h>
#include <sqlext.h>
#include <sqlspi.h>
#include <sqltypes.h>

// clang-format off
template <typename QueryObject>
concept SqlQueryObject = requires(QueryObject const& queryObject)
{
    { queryObject.ToSql() } -> std::convertible_to<std::string>;
};
// clang-format on

class SqlResultCursor;

// High level API for (prepared) raw SQL statements
//
// SQL prepared statement lifecycle:
// 1. Prepare the statement
// 2. Optionally bind output columns to local variables
// 3. Execute the statement (optionally with input parameters)
// 4. Fetch rows (if any)
// 5. Repeat steps 3 and 4 as needed
class SqlStatement final: public SqlDataBinderCallback
{
  public:
    // Construct a new SqlStatement object, using a new connection, and connect to the default database.
    LIGHTWEIGHT_API SqlStatement();

    LIGHTWEIGHT_API SqlStatement(SqlStatement&&) noexcept;
    LIGHTWEIGHT_API SqlStatement& operator=(SqlStatement&&) noexcept;

    SqlStatement(SqlStatement const&) noexcept = delete;
    SqlStatement& operator=(SqlStatement const&) noexcept = delete;

    // Construct a new SqlStatement object, using the given connection.
    LIGHTWEIGHT_API explicit SqlStatement(SqlConnection& relatedConnection);

    LIGHTWEIGHT_API ~SqlStatement() noexcept final;

    [[nodiscard]] LIGHTWEIGHT_API bool IsAlive() const noexcept;

    // Retrieves the connection associated with this statement.
    [[nodiscard]] LIGHTWEIGHT_API SqlConnection& Connection() noexcept;

    // Retrieves the connection associated with this statement.
    [[nodiscard]] LIGHTWEIGHT_API SqlConnection const& Connection() const noexcept;

    // Retrieves the last error information with respect to this SQL statement handle.
    [[nodiscard]] LIGHTWEIGHT_API SqlErrorInfo LastError() const;

    // Creates a new query builder for the given table, compatible with the SQL server being connected.
    LIGHTWEIGHT_API SqlQueryBuilder Query(std::string_view const& table = {}) const;

    // Creates a new query builder for the given table with an alias, compatible with the SQL server being connected.
    [[nodiscard]] LIGHTWEIGHT_API SqlQueryBuilder QueryAs(std::string_view const& table,
                                                          std::string_view const& tableAlias) const;

    // Retrieves the native handle of the statement.
    [[nodiscard]] LIGHTWEIGHT_API SQLHSTMT NativeHandle() const noexcept;

    // Prepares the statement for execution.
    //
    // @note When preparing a new SQL statement the previously executed statement, yielding a result set,
    //       must have been closed.
    LIGHTWEIGHT_API void Prepare(std::string_view query);

    // Prepares the statement for execution.
    //
    // @note When preparing a new SQL statement the previously executed statement, yielding a result set,
    //       must have been closed.
    void Prepare(SqlQueryObject auto const& queryObject);

    std::string const& PreparedQuery() const noexcept;

    template <SqlInputParameterBinder Arg>
    void BindInputParameter(SQLSMALLINT columnIndex, Arg const& arg);

    template <SqlInputParameterBinder Arg, typename ColumnName>
    void BindInputParameter(SQLSMALLINT columnIndex, Arg const& arg, ColumnName&& columnNameHint);

    // Binds the given arguments to the prepared statement to store the fetched data to.
    //
    // The statement must be prepared before calling this function.
    template <SqlOutputColumnBinder... Args>
    void BindOutputColumns(Args*... args);

    template <SqlOutputColumnBinder T>
    void BindOutputColumn(SQLUSMALLINT columnIndex, T* arg);

    // Binds the given arguments to the prepared statement and executes it.
    template <SqlInputParameterBinder... Args>
    void Execute(Args const&... args);

    // Binds the given arguments to the prepared statement and executes it.
    LIGHTWEIGHT_API void ExecuteWithVariants(std::vector<SqlVariant> const& args);

    // Executes the prepared statement on a batch of data.
    //
    // Each parameter represents a column, to be bound as input parameter.
    // The element types of each column container must be explicitly supported.
    //
    // In order to support column value types, their underlying storage must be contiguous.
    // Also the input range itself must be contiguous.
    // If any of these conditions are not met, the function will not compile - use ExecuteBatch() instead.
    template <SqlInputParameterBatchBinder FirstColumnBatch, std::ranges::contiguous_range... MoreColumnBatches>
    void ExecuteBatchNative(FirstColumnBatch const& firstColumnBatch, MoreColumnBatches const&... moreColumnBatches);

    // Executes the prepared statement on a batch of data.
    //
    // Each parameter represents a column, to be bound as input parameter,
    // and the number of elements in these bound column containers will
    // mandate how many executions will happen.
    //
    // This function will bind and execute each row separately,
    // which is less efficient than ExecuteBatchNative(), but works non-contiguous input ranges.
    template <SqlInputParameterBatchBinder FirstColumnBatch, std::ranges::range... MoreColumnBatches>
    void ExecuteBatchSoft(FirstColumnBatch const& firstColumnBatch, MoreColumnBatches const&... moreColumnBatches);

    // Executes the prepared statement on a batch of data.
    //
    // Each parameter represents a column, to be bound as input parameter,
    // and the number of elements in these bound column containers will
    // mandate how many executions will happen.
    template <SqlInputParameterBatchBinder FirstColumnBatch, std::ranges::range... MoreColumnBatches>
    void ExecuteBatch(FirstColumnBatch const& firstColumnBatch, MoreColumnBatches const&... moreColumnBatches);

    // Executes the given query directly.
    LIGHTWEIGHT_API void ExecuteDirect(std::string_view const& query,
                                       std::source_location location = std::source_location::current());

    // Executes the given query directly.
    void ExecuteDirect(SqlQueryObject auto const& query,
                       std::source_location location = std::source_location::current());

    // Executes the given query, assuming that only one result row and column is affected, that one will be
    // returned.
    template <typename T>
        requires(!std::same_as<T, SqlVariant>)
    [[nodiscard]] std::optional<T> ExecuteDirectScalar(const std::string_view& query,
                                                       std::source_location location = std::source_location::current());

    template <typename T>
        requires(std::same_as<T, SqlVariant>)
    [[nodiscard]] T ExecuteDirectScalar(const std::string_view& query,
                                        std::source_location location = std::source_location::current());
    // Executes the given query, assuming that only one result row and column is affected, that one will be
    // returned.
    template <typename T>
        requires(!std::same_as<T, SqlVariant>)
    [[nodiscard]] std::optional<T> ExecuteDirectScalar(SqlQueryObject auto const& query,
                                                       std::source_location location = std::source_location::current());

    template <typename T>
        requires(std::same_as<T, SqlVariant>)
    [[nodiscard]] T ExecuteDirectScalar(SqlQueryObject auto const& query,
                                        std::source_location location = std::source_location::current());

    // Retrieves the number of rows affected by the last query.
    [[nodiscard]] LIGHTWEIGHT_API size_t NumRowsAffected() const;

    // Retrieves the number of columns affected by the last query.
    [[nodiscard]] LIGHTWEIGHT_API size_t NumColumnsAffected() const;

    // Retrieves the last insert ID of the last query's primary key.
    [[nodiscard]] LIGHTWEIGHT_API size_t LastInsertId();

    // Fetches the next row of the result set.
    //
    // @note Automatically closes the cursor at the end of the result set.
    //
    // @retval true The next result row was successfully fetched
    // @retval false No result row was fetched, because the end of the result set was reached.
    [[nodiscard]] LIGHTWEIGHT_API bool FetchRow();

    // Closes the result cursor on queries that yield a result set, e.g. SELECT statements.
    //
    // Call this function when done with fetching the results before the end of the result set is reached.
    void CloseCursor() noexcept;

    // Retrieves the result cursor for reading an SQL query result.
    SqlResultCursor GetResultCursor() noexcept;

    // Retrieves the value of the column at the given index for the currently selected row.
    //
    // Returns true if the value is not NULL, false otherwise.
    template <SqlGetColumnNativeType T>
    [[nodiscard]] bool GetColumn(SQLUSMALLINT column, T* result) const;

    // Retrieves the value of the column at the given index for the currently selected row.
    template <SqlGetColumnNativeType T>
    [[nodiscard]] T GetColumn(SQLUSMALLINT column) const;

    // Retrieves the value of the column at the given index for the currently selected row.
    //
    // If the value is NULL, std::nullopt is returned.
    template <SqlGetColumnNativeType T>
    [[nodiscard]] std::optional<T> GetNullableColumn(SQLUSMALLINT column) const;

  private:
    LIGHTWEIGHT_API void RequireSuccess(SQLRETURN error,
                                        std::source_location sourceLocation = std::source_location::current()) const;
    LIGHTWEIGHT_API void PlanPostExecuteCallback(std::function<void()>&& cb) override;
    LIGHTWEIGHT_API void PlanPostProcessOutputColumn(std::function<void()>&& cb) override;
    [[nodiscard]] LIGHTWEIGHT_API SqlServerType ServerType() const noexcept override;
    LIGHTWEIGHT_API void ProcessPostExecuteCallbacks();

    LIGHTWEIGHT_API void RequireIndicators();
    LIGHTWEIGHT_API SQLLEN* GetIndicatorForColumn(SQLUSMALLINT column) noexcept;

    // private data members
    struct Data;
    std::unique_ptr<Data, void (*)(Data*)> m_data; // The private data of the statement
    SqlConnection* m_connection {};                // Pointer to the connection object
    SQLHSTMT m_hStmt {};                           // The native oDBC statement handle
    std::string m_preparedQuery;                   // The last prepared query
    SQLSMALLINT m_expectedParameterCount {};       // The number of parameters expected by the query
};

// API for reading an SQL query result set.
class [[nodiscard]] SqlResultCursor
{
  public:
    explicit LIGHTWEIGHT_FORCE_INLINE SqlResultCursor(SqlStatement& stmt):
        m_stmt { &stmt }
    {
    }

    SqlResultCursor() = delete;
    SqlResultCursor(SqlResultCursor const&) = delete;
    SqlResultCursor& operator=(SqlResultCursor const&) = delete;
    SqlResultCursor(SqlResultCursor&&) = delete;
    SqlResultCursor& operator=(SqlResultCursor&&) = delete;

    LIGHTWEIGHT_FORCE_INLINE ~SqlResultCursor()
    {
        SQLCloseCursor(m_stmt->NativeHandle());
    }

    // Retrieves the number of rows affected by the last query.
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE size_t NumRowsAffected() const
    {
        return m_stmt->NumRowsAffected();
    }

    // Retrieves the number of columns affected by the last query.
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE size_t NumColumnsAffected() const
    {
        return m_stmt->NumColumnsAffected();
    }

    // Binds the given arguments to the prepared statement to store the fetched data to.
    //
    // The statement must be prepared before calling this function.
    template <SqlOutputColumnBinder... Args>
    LIGHTWEIGHT_FORCE_INLINE void BindOutputColumns(Args*... args)
    {
        m_stmt->BindOutputColumns(args...);
    }

    template <SqlOutputColumnBinder T>
    LIGHTWEIGHT_FORCE_INLINE void BindOutputColumn(SQLUSMALLINT columnIndex, T* arg)
    {
        m_stmt->BindOutputColumn(columnIndex, arg);
    }

    // Fetches the next row of the result set.
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE bool FetchRow()
    {
        return m_stmt->FetchRow();
    }

    // Retrieves the value of the column at the given index for the currently selected row.
    //
    // Returns true if the value is not NULL, false otherwise.
    template <SqlGetColumnNativeType T>
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE bool GetColumn(SQLUSMALLINT column, T* result) const
    {
        return m_stmt->GetColumn<T>(column, result);
    }

    // Retrieves the value of the column at the given index for the currently selected row.
    template <SqlGetColumnNativeType T>
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE T GetColumn(SQLUSMALLINT column) const
    {
        return m_stmt->GetColumn<T>(column);
    }

    // Retrieves the value of the column at the given index for the currently selected row.
    //
    // If the value is NULL, std::nullopt is returned.
    template <SqlGetColumnNativeType T>
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE std::optional<T> GetNullableColumn(SQLUSMALLINT column) const
    {
        return m_stmt->GetNullableColumn<T>(column);
    }

  private:
    SqlStatement* m_stmt;
};

// {{{ inline implementation
inline LIGHTWEIGHT_FORCE_INLINE bool SqlStatement::IsAlive() const noexcept
{
    return m_connection && m_connection->IsAlive() && m_hStmt != nullptr;
}

inline LIGHTWEIGHT_FORCE_INLINE SqlConnection& SqlStatement::Connection() noexcept
{
    return *m_connection;
}

inline LIGHTWEIGHT_FORCE_INLINE SqlConnection const& SqlStatement::Connection() const noexcept
{
    return *m_connection;
}

inline LIGHTWEIGHT_FORCE_INLINE SqlErrorInfo SqlStatement::LastError() const
{
    return SqlErrorInfo::fromStatementHandle(m_hStmt);
}

inline LIGHTWEIGHT_FORCE_INLINE SQLHSTMT SqlStatement::NativeHandle() const noexcept
{
    return m_hStmt;
}

inline LIGHTWEIGHT_FORCE_INLINE void SqlStatement::Prepare(SqlQueryObject auto const& queryObject)
{
    Prepare(queryObject.ToSql());
}

inline LIGHTWEIGHT_FORCE_INLINE std::string const& SqlStatement::PreparedQuery() const noexcept
{
    return m_preparedQuery;
}

template <SqlOutputColumnBinder... Args>
inline LIGHTWEIGHT_FORCE_INLINE void SqlStatement::BindOutputColumns(Args*... args)
{
    RequireIndicators();

    SQLUSMALLINT i = 0;
    ((++i, RequireSuccess(SqlDataBinder<Args>::OutputColumn(m_hStmt, i, args, GetIndicatorForColumn(i), *this))), ...);
}

template <SqlOutputColumnBinder T>
inline LIGHTWEIGHT_FORCE_INLINE void SqlStatement::BindOutputColumn(SQLUSMALLINT columnIndex, T* arg)
{
    RequireIndicators();

    RequireSuccess(
        SqlDataBinder<T>::OutputColumn(m_hStmt, columnIndex, arg, GetIndicatorForColumn(columnIndex), *this));
}

template <SqlInputParameterBinder Arg>
inline LIGHTWEIGHT_FORCE_INLINE void SqlStatement::BindInputParameter(SQLSMALLINT columnIndex, Arg const& arg)
{
    // tell Execute() that we don't know the expected count
    m_expectedParameterCount = (std::numeric_limits<decltype(m_expectedParameterCount)>::max)();
    RequireSuccess(SqlDataBinder<Arg>::InputParameter(m_hStmt, columnIndex, arg, *this));
}

template <SqlInputParameterBinder Arg, typename ColumnName>
inline LIGHTWEIGHT_FORCE_INLINE void SqlStatement::BindInputParameter(SQLSMALLINT columnIndex,
                                                                      Arg const& arg,
                                                                      ColumnName&& columnNameHint)
{
    SqlLogger::GetLogger().OnBindInputParameter(std::forward<ColumnName>(columnNameHint), arg);
    BindInputParameter(columnIndex, arg);
}

template <SqlInputParameterBinder... Args>
void SqlStatement::Execute(Args const&... args)
{
    // Each input parameter must have an address,
    // such that we can call SQLBindParameter() without needing to copy it.
    // The memory region behind the input parameter must exist until the SQLExecute() call.

    SqlLogger::GetLogger().OnExecute(m_preparedQuery);

    if (!(m_expectedParameterCount == (std::numeric_limits<decltype(m_expectedParameterCount)>::max)()
          && sizeof...(args) == 0)
        && !(m_expectedParameterCount == sizeof...(args)))
        throw std::invalid_argument { "Invalid argument count" };

    SQLUSMALLINT i = 0;
    ((++i,
      SqlLogger::GetLogger().OnBindInputParameter({}, args),
      RequireSuccess(SqlDataBinder<Args>::InputParameter(m_hStmt, i, args, *this))),
     ...);

    RequireSuccess(SQLExecute(m_hStmt));
    ProcessPostExecuteCallbacks();
}

// clang-format off
template <typename T>
concept SqlNativeContiguousValueConcept =
       std::same_as<T, bool>
    || std::same_as<T, char>
    || std::same_as<T, unsigned char>
    || std::same_as<T, wchar_t>
    || std::same_as<T, std::int16_t>
    || std::same_as<T, std::uint16_t>
    || std::same_as<T, std::int32_t>
    || std::same_as<T, std::uint32_t>
    || std::same_as<T, std::int64_t>
    || std::same_as<T, std::uint64_t>
    || std::same_as<T, float>
    || std::same_as<T, double>
    || std::same_as<T, SqlDate>
    || std::same_as<T, SqlTime>
    || std::same_as<T, SqlDateTime>
    || std::same_as<T, SqlFixedString<T::Capacity, typename T::value_type, T::PostRetrieveOperation>>;

template <typename FirstColumnBatch, typename... MoreColumnBatches>
concept SqlNativeBatchable =
        std::ranges::contiguous_range<FirstColumnBatch>
    && (std::ranges::contiguous_range<MoreColumnBatches> && ...)
    &&  SqlNativeContiguousValueConcept<std::ranges::range_value_t<FirstColumnBatch>>
    && (SqlNativeContiguousValueConcept<std::ranges::range_value_t<MoreColumnBatches>> && ...);

// clang-format on

template <SqlInputParameterBatchBinder FirstColumnBatch, std::ranges::contiguous_range... MoreColumnBatches>
void SqlStatement::ExecuteBatchNative(FirstColumnBatch const& firstColumnBatch,
                                      MoreColumnBatches const&... moreColumnBatches)
{
    static_assert(SqlNativeBatchable<FirstColumnBatch, MoreColumnBatches...>,
                  "Must be a supported native contiguous element type.");

    if (m_expectedParameterCount != 1 + sizeof...(moreColumnBatches))
        throw std::invalid_argument { "Invalid number of columns" };

    const auto rowCount = std::ranges::size(firstColumnBatch);
    if (!((std::size(moreColumnBatches) == rowCount) && ...))
        throw std::invalid_argument { "Uneven number of rows" };

    size_t rowStart = 0;

    // clang-format off
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    RequireSuccess(SQLSetStmtAttr(m_hStmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER) rowCount, 0));
    RequireSuccess(SQLSetStmtAttr(m_hStmt, SQL_ATTR_PARAM_BIND_OFFSET_PTR, &rowStart, 0));
    RequireSuccess(SQLSetStmtAttr(m_hStmt, SQL_ATTR_PARAM_BIND_TYPE, SQL_PARAM_BIND_BY_COLUMN, 0));
    RequireSuccess(SQLSetStmtAttr(m_hStmt, SQL_ATTR_PARAM_OPERATION_PTR, SQL_PARAM_PROCEED, 0));
    RequireSuccess(SqlDataBinder<std::remove_cvref_t<decltype(*std::ranges::data(firstColumnBatch))>>::
                                   InputParameter(m_hStmt, 1, *std::ranges::data(firstColumnBatch), *this));
    SQLUSMALLINT column = 1;
    (RequireSuccess(SqlDataBinder<std::remove_cvref_t<decltype(*std::ranges::data(moreColumnBatches))>>::
                        InputParameter(m_hStmt, ++column, *std::ranges::data(moreColumnBatches), *this)), ...);
    RequireSuccess(SQLExecute(m_hStmt));
    ProcessPostExecuteCallbacks();
    // clang-format on
}

template <SqlInputParameterBatchBinder FirstColumnBatch, std::ranges::range... MoreColumnBatches>
inline LIGHTWEIGHT_FORCE_INLINE void SqlStatement::ExecuteBatch(FirstColumnBatch const& firstColumnBatch,
                                                                MoreColumnBatches const&... moreColumnBatches)
{
    // If the input ranges are contiguous and their element types are contiguous and supported as well,
    // we can use the native batch execution.
    if constexpr (SqlNativeBatchable<FirstColumnBatch, MoreColumnBatches...>)
        ExecuteBatchNative(firstColumnBatch, moreColumnBatches...);
    else
        ExecuteBatchSoft(firstColumnBatch, moreColumnBatches...);
}

template <SqlInputParameterBatchBinder FirstColumnBatch, std::ranges::range... MoreColumnBatches>
void SqlStatement::ExecuteBatchSoft(FirstColumnBatch const& firstColumnBatch,
                                    MoreColumnBatches const&... moreColumnBatches)
{
    if (m_expectedParameterCount != 1 + sizeof...(moreColumnBatches))
        throw std::invalid_argument { "Invalid number of columns" };

    const auto rowCount = std::ranges::size(firstColumnBatch);
    if (!((std::size(moreColumnBatches) == rowCount) && ...))
        throw std::invalid_argument { "Uneven number of rows" };

    for (auto const rowIndex: std::views::iota(size_t { 0 }, rowCount))
    {
        std::apply(
            [&]<SqlInputParameterBinder... ColumnValues>(ColumnValues const&... columnsInRow) {
                SQLUSMALLINT column = 0;
                ((++column, SqlDataBinder<ColumnValues>::InputParameter(m_hStmt, column, columnsInRow, *this)), ...);
                RequireSuccess(SQLExecute(m_hStmt));
                ProcessPostExecuteCallbacks();
            },
            std::make_tuple(std::ref(*std::ranges::next(std::ranges::begin(firstColumnBatch), rowIndex)),
                            std::ref(*std::ranges::next(std::ranges::begin(moreColumnBatches), rowIndex))...));
    }
}

template <SqlGetColumnNativeType T>
inline bool SqlStatement::GetColumn(SQLUSMALLINT column, T* result) const
{
    SQLLEN indicator {}; // TODO: Handle NULL values if we find out that we need them for our use-cases.
    RequireSuccess(SqlDataBinder<T>::GetColumn(m_hStmt, column, result, &indicator, *this));
    return indicator != SQL_NULL_DATA;
}

namespace detail
{

// is_specialization_of<> is inspired by:
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p2098r1.pdf

template <typename T>
concept SqlNullableType = (std::same_as<T, SqlVariant> || is_specialization_of<std::optional, T>::value);

} // end namespace detail

template <SqlGetColumnNativeType T>
inline T SqlStatement::GetColumn(SQLUSMALLINT column) const
{
    T result {};
    SQLLEN indicator {};
    RequireSuccess(SqlDataBinder<T>::GetColumn(m_hStmt, column, &result, &indicator, *this));
    if constexpr (!detail::SqlNullableType<T>)
        if (indicator == SQL_NULL_DATA)
            throw std::runtime_error { "Column value is NULL" };
    return result;
}

template <SqlGetColumnNativeType T>
inline std::optional<T> SqlStatement::GetNullableColumn(SQLUSMALLINT column) const
{
    T result {};
    SQLLEN indicator {}; // TODO: Handle NULL values if we find out that we need them for our use-cases.
    RequireSuccess(SqlDataBinder<T>::GetColumn(m_hStmt, column, &result, &indicator, *this));
    if (indicator == SQL_NULL_DATA)
        return std::nullopt;
    return { std::move(result) };
}

inline LIGHTWEIGHT_FORCE_INLINE void SqlStatement::ExecuteDirect(SqlQueryObject auto const& query,
                                                                 std::source_location location)
{
    return ExecuteDirect(query.ToSql(), location);
}

template <typename T>
    requires(!std::same_as<T, SqlVariant>)
inline LIGHTWEIGHT_FORCE_INLINE std::optional<T> SqlStatement::ExecuteDirectScalar(const std::string_view& query,
                                                                                   std::source_location location)
{
    auto const _ = detail::Finally([this] { CloseCursor(); });
    ExecuteDirect(query, location);
    RequireSuccess(FetchRow());
    return GetNullableColumn<T>(1);
}

template <typename T>
    requires(std::same_as<T, SqlVariant>)
inline LIGHTWEIGHT_FORCE_INLINE T SqlStatement::ExecuteDirectScalar(const std::string_view& query,
                                                                    std::source_location location)
{
    auto const _ = detail::Finally([this] { CloseCursor(); });
    ExecuteDirect(query, location);
    RequireSuccess(FetchRow());
    if (auto result = GetNullableColumn<T>(1); result.has_value())
        return *result;
    return SqlVariant { SqlNullValue };
}

template <typename T>
    requires(!std::same_as<T, SqlVariant>)
inline LIGHTWEIGHT_FORCE_INLINE std::optional<T> SqlStatement::ExecuteDirectScalar(SqlQueryObject auto const& query,
                                                                                   std::source_location location)
{
    return ExecuteDirectScalar<T>(query.ToSql(), location);
}

template <typename T>
    requires(std::same_as<T, SqlVariant>)
inline LIGHTWEIGHT_FORCE_INLINE T SqlStatement::ExecuteDirectScalar(SqlQueryObject auto const& query,
                                                                    std::source_location location)
{
    return ExecuteDirectScalar<T>(query.ToSql(), location);
}

inline LIGHTWEIGHT_FORCE_INLINE void SqlStatement::CloseCursor() noexcept
{
    // SQLCloseCursor(m_hStmt);
    SQLFreeStmt(m_hStmt, SQL_CLOSE);
    SqlLogger::GetLogger().OnFetchEnd();
}

inline LIGHTWEIGHT_FORCE_INLINE SqlResultCursor SqlStatement::GetResultCursor() noexcept
{
    return SqlResultCursor { *this };
}

// }}}
