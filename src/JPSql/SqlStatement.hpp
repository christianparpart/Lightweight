// SPDX-License-Identifier: MIT
#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #include <Windows.h>
#endif

#include "SqlConnection.hpp"
#include "SqlDataBinder.hpp"

#include <cstring>
#include <format>
#include <optional>
#include <ranges>
#include <source_location>
#include <type_traits>
#include <variant>
#include <vector>

#include <sql.h>
#include <sqlext.h>
#include <sqlspi.h>
#include <sqltypes.h>

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
    SqlStatement() noexcept;

    SqlStatement(SqlStatement&&) noexcept = default;

    // Construct a new SqlStatement object, using the given connection.
    SqlStatement(SqlConnection& relatedConnection) noexcept;

    ~SqlStatement() noexcept;

    // Retrieves the connection associated with this statement.
    SqlConnection& Connection() noexcept;

    // Retrieves the connection associated with this statement.
    SqlConnection const& Connection() const noexcept;

    // Retrieves the native handle of the statement.
    [[nodiscard]] SQLHSTMT NativeHandle() const noexcept;

    // Retrieves the last error code.
    [[nodiscard]] SqlError LastError() const noexcept;

    // Retrieves the last result code in form of a SqlResult<void>
    SqlResult<void> LastResult() const noexcept;

    // Prepares the statement for execution.
    [[nodiscard]] SqlResult<void> Prepare(std::string_view query) noexcept;

    template <SqlInputParameterBinder Arg>
    [[nodiscard]] SqlResult<void> BindInputParameter(SQLSMALLINT columnIndex, Arg const& arg) noexcept;

    // Binds the given arguments to the prepared statement to store the fetched data to.
    //
    // The statement must be prepared before calling this function.
    template <SqlOutputColumnBinder... Args>
    [[nodiscard]] SqlResult<void> BindOutputColumns(Args*... args);

    template <SqlOutputColumnBinder T>
    [[nodiscard]] SqlResult<void> BindOutputColumn(SQLUSMALLINT columnIndex, T* arg);

    // Binds the given arguments to the prepared statement and executes it.
    template <SqlInputParameterBinder... Args>
    [[nodiscard]] SqlResult<void> Execute(Args const&... args) noexcept;

    // Executes the prepared statement on a a batch of data.
    //
    // Each parameter represents a column, to be bound as input parameter.
    // The element types of each column container must be explicitly supported.
    //
    // In order to support column value types, their underlying storage must be contiguous.
    // Also the input range itself must be contiguous.
    // If any of these conditions are not met, the function will not compile - use ExecuteBatch() instead.
    template <SqlInputParameterBatchBinder FirstColumnBatch, std::ranges::contiguous_range... MoreColumnBatches>
    SqlResult<void> ExecuteBatchNative(FirstColumnBatch const& firstColumnBatch,
                                       MoreColumnBatches const&... moreColumnBatches) noexcept;

    // Executes the prepared statement on a a batch of data.
    //
    // Each parameter represents a column, to be bound as input parameter,
    // and the number of elements in these bound column containers will
    // mandate how many executions will happen.
    template <SqlInputParameterBatchBinder FirstColumnBatch, std::ranges::range... MoreColumnBatches>
    SqlResult<void> ExecuteBatch(FirstColumnBatch const& firstColumnBatch,
                                 MoreColumnBatches const&... moreColumnBatches) noexcept;

    // Executes the given query directly.
    [[nodiscard]] SqlResult<void> ExecuteDirect(
        const std::string_view& query, std::source_location location = std::source_location::current()) noexcept;

    // Executes the given query, assuming that only one result row and column is affected, that one will be returned.
    template <typename T>
    [[nodiscard]] SqlResult<T> ExecuteDirectScalar(
        const std::string_view& query, std::source_location location = std::source_location::current()) noexcept;

    // Retrieves the number of rows affected by the last query.
    [[nodiscard]] SqlResult<size_t> NumRowsAffected() const noexcept;

    // Retrieves the number of columns affected by the last query.
    [[nodiscard]] SqlResult<size_t> NumColumnsAffected() const noexcept;

    // Retrieves the last insert ID of the last query's primary key.
    [[nodiscard]] SqlResult<size_t> LastInsertId() noexcept;

    // Fetches the next row of the result set.
    [[nodiscard]] SqlResult<void> FetchRow() noexcept;

    // Retrieves the value of the column at the given index for the currently selected row.
    template <SqlGetColumnNativeType T>
    [[nodiscard]] SqlResult<void> GetColumn(SQLUSMALLINT column, T* result) const noexcept;

    // Retrieves the value of the column at the given index for the currently selected row.
    template <SqlGetColumnNativeType T>
    [[nodiscard]] SqlResult<T> GetColumn(SQLUSMALLINT column) const noexcept;

  private:
    SqlResult<void> UpdateLastError(SQLRETURN error,
                                    std::source_location location = std::source_location::current()) const noexcept;

    void PlanPostExecuteCallback(std::function<void()>&& cb) override;
    void PlanPostProcessOutputColumn(std::function<void()>&& cb) override;
    void ProcessPostExecuteCallbacks() noexcept;

    // private data members

    std::optional<SqlConnection> m_ownedConnection; // The connection object (if owned)
    SqlConnection* m_connection {};                 // Pointer to the connection object
    SQLHSTMT m_hStmt {};                            // The native oDBC statement handle
    mutable SqlError m_lastError {};                // The last error code
    SQLSMALLINT m_expectedParameterCount {};        // The number of parameters expected by the query
    std::vector<SQLLEN> m_indicators;               // Holds the indicators for the bound output columns
    std::vector<std::function<void()>> m_postExecuteCallbacks;
    std::vector<std::function<void()>> m_postProcessOutputColumnCallbacks;
};

// {{{ inline implementation
inline SqlConnection& SqlStatement::Connection() noexcept
{
    return *m_connection;
}

inline SqlConnection const& SqlStatement::Connection() const noexcept
{
    return *m_connection;
}

[[nodiscard]] inline SQLHSTMT SqlStatement::NativeHandle() const noexcept
{
    return m_hStmt;
}

[[nodiscard]] inline SqlError SqlStatement::LastError() const noexcept
{
    return m_lastError;
}

[[nodiscard]] inline SqlResult<void> SqlStatement::LastResult() const noexcept
{
    if (m_lastError != SqlError::SUCCESS)
        return std::unexpected { m_lastError };

    return {};
}

template <SqlOutputColumnBinder... Args>
[[nodiscard]] SqlResult<void> SqlStatement::BindOutputColumns(Args*... args)
{
    return NumColumnsAffected().and_then([&, binderCallback = this](size_t numColumns) -> SqlResult<void> {
        m_indicators.resize(numColumns + 1);

        SQLUSMALLINT i = 0;
        (void) ((++i,
                 UpdateLastError(
                     SqlDataBinder<Args>::OutputColumn(m_hStmt, i, args, &m_indicators[i], *binderCallback)))
                && ...);
        if (m_lastError != SqlError::SUCCESS)
            return std::unexpected { m_lastError };

        return {};
    });
}

template <SqlOutputColumnBinder T>
[[nodiscard]] SqlResult<void> SqlStatement::BindOutputColumn(SQLUSMALLINT columnIndex, T* arg)
{
    if (m_indicators.size() <= columnIndex)
        m_indicators.resize(NumColumnsAffected().value_or(columnIndex) + 1);

    return UpdateLastError(
        SqlDataBinder<T>::OutputColumn(m_hStmt, columnIndex, arg, &m_indicators[columnIndex], *this));
}

template <SqlInputParameterBinder Arg>
[[nodiscard]] SqlResult<void> SqlStatement::BindInputParameter(SQLSMALLINT columnIndex, Arg const& arg) noexcept
{
    // tell Execute() that we don't know the expected count
    m_expectedParameterCount = (std::numeric_limits<decltype(m_expectedParameterCount)>::max)();
    return UpdateLastError(SqlDataBinder<Arg>::InputParameter(m_hStmt, columnIndex, arg));
}

template <SqlInputParameterBinder... Args>
[[nodiscard]] SqlResult<void> SqlStatement::Execute(Args const&... args) noexcept
{
    // Each input parameter must have an address,
    // such that we can call SQLBindParameter() without needing to copy it.
    // The memory region behind the input parameter must exist until the SQLExecute() call.

    SqlLogger::GetLogger().OnExecute();

    if (!(m_expectedParameterCount == (std::numeric_limits<decltype(m_expectedParameterCount)>::max)()
          && sizeof...(args) == 0)
        && !(m_expectedParameterCount == sizeof...(args)))
        return std::unexpected { SqlError::INVALID_ARGUMENT };

    SQLUSMALLINT i = 0;
    ((++i, UpdateLastError(SqlDataBinder<Args>::InputParameter(m_hStmt, i, args))) && ...);
    if (m_lastError != SqlError::SUCCESS)
        return std::unexpected { m_lastError };

    return UpdateLastError(SQLExecute(m_hStmt)).and_then([&]() -> SqlResult<void> {
        ProcessPostExecuteCallbacks();
        return {};
    });
}

// clang-format off
template <typename T>
concept SqlNativeContiguousValueConcept =
       std::same_as<T, bool>
    || std::same_as<T, char>
    || std::same_as<T, unsigned char>
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
SqlResult<void> SqlStatement::ExecuteBatchNative(FirstColumnBatch const& firstColumnBatch,
                                                 MoreColumnBatches const&... moreColumnBatches) noexcept
{
    static_assert(SqlNativeBatchable<FirstColumnBatch, MoreColumnBatches...>,
                  "Must be a supported native contiguous element type.");

    if (m_expectedParameterCount != 1 + sizeof...(moreColumnBatches))
        // invalid number of columns
        return std::unexpected { SqlError::INVALID_ARGUMENT };

    const auto rowCount = std::ranges::size(firstColumnBatch);
    if (!((std::size(moreColumnBatches) == rowCount) && ...))
        // uneven number of rows
        return std::unexpected { SqlError::INVALID_ARGUMENT };

    size_t rowStart = 0;

    // clang-format off
    return UpdateLastError(SQLSetStmtAttr(m_hStmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER) rowCount, 0))
        .and_then([&] { return UpdateLastError(SQLSetStmtAttr(m_hStmt, SQL_ATTR_PARAM_BIND_OFFSET_PTR, &rowStart, 0)); })
        .and_then([&] { return UpdateLastError(SQLSetStmtAttr(m_hStmt, SQL_ATTR_PARAM_BIND_TYPE, SQL_PARAM_BIND_BY_COLUMN, 0)); })
        .and_then([&] { return UpdateLastError(SQLSetStmtAttr(m_hStmt, SQL_ATTR_PARAM_OPERATION_PTR, SQL_PARAM_PROCEED, 0)); })
        .and_then([&] {
            return UpdateLastError(SqlDataBinder<std::remove_cvref_t<decltype(*std::ranges::data(firstColumnBatch))>>::
                                   InputParameter(m_hStmt, 1, *std::ranges::data(firstColumnBatch)));
        })
        .and_then([&]() -> SqlResult<void> {
            SQLUSMALLINT column = 1;
            (UpdateLastError(SqlDataBinder<std::remove_cvref_t<decltype(*std::ranges::data(moreColumnBatches))>>::
                                InputParameter(m_hStmt, ++column, *std::ranges::data(moreColumnBatches))) && ...);
            return LastResult();
        })
        .and_then([&] {
            return UpdateLastError(SQLExecute(m_hStmt)).and_then([&]() -> SqlResult<void> {
                ProcessPostExecuteCallbacks();
                return {};
            });
        })
        .or_else([&](auto&& e) -> SqlResult<void> {
            SqlLogger::GetLogger().OnWarning(std::format("Batch execution failed at {}.", rowStart));
            return std::unexpected { e };
        });
    // clang-format on
}

template <SqlInputParameterBatchBinder FirstColumnBatch, std::ranges::range... MoreColumnBatches>
SqlResult<void> SqlStatement::ExecuteBatch(FirstColumnBatch const& firstColumnBatch,
                                           MoreColumnBatches const&... moreColumnBatches) noexcept
{
    // If the input ranges are contiguous and their element types are contiguous and supported as well,
    // we can use the native batch execution.
    if constexpr (SqlNativeBatchable<FirstColumnBatch, MoreColumnBatches...>)
        return ExecuteBatchNative(firstColumnBatch, moreColumnBatches...);

    if (m_expectedParameterCount != 1 + sizeof...(moreColumnBatches))
        // invalid number of columns
        return std::unexpected { SqlError::INVALID_ARGUMENT };

    const auto rowCount = std::ranges::size(firstColumnBatch);
    if (!((std::size(moreColumnBatches) == rowCount) && ...))
        // uneven number of rows
        return std::unexpected { SqlError::INVALID_ARGUMENT };

    m_lastError = SqlError::SUCCESS;
    for (auto const rowIndex: std::views::iota(size_t { 0 }, rowCount))
    {
        std::apply(
            [&]<SqlInputParameterBinder... ColumnValues>(ColumnValues const&... columnsInRow) {
                SQLUSMALLINT column = 0;
                ((++column, UpdateLastError(SqlDataBinder<ColumnValues>::InputParameter(m_hStmt, column, columnsInRow)))
                 && ...);
                UpdateLastError(SQLExecute(m_hStmt)).and_then([&]() -> SqlResult<void> {
                    ProcessPostExecuteCallbacks();
                    return {};
                });
            },
            std::make_tuple(std::ref(*std::ranges::next(std::ranges::begin(firstColumnBatch), rowIndex)),
                            std::ref(*std::ranges::next(std::ranges::begin(moreColumnBatches), rowIndex))...));
        if (m_lastError != SqlError::SUCCESS)
            return std::unexpected { m_lastError };
    }
    return {};
}

template <SqlGetColumnNativeType T>
[[nodiscard]] inline SqlResult<void> SqlStatement::GetColumn(SQLUSMALLINT column, T* result) const noexcept
{
    SQLLEN indicator {}; // TODO: Handle NULL values if we find out that we need them for our use-cases.
    return UpdateLastError(SqlDataBinder<T>::GetColumn(m_hStmt, column, result, &indicator));
}

template <SqlGetColumnNativeType T>
[[nodiscard]] SqlResult<T> SqlStatement::GetColumn(SQLUSMALLINT column) const noexcept
{
    T result {};
    SQLLEN indicator {}; // TODO: Handle NULL values if we find out that we need them for our use-cases.
    return UpdateLastError(SqlDataBinder<T>::GetColumn(m_hStmt, column, &result, &indicator)).transform([&] {
        return std::move(result);
    });
}

inline void SqlStatement::PlanPostExecuteCallback(std::function<void()>&& cb)
{
    m_postExecuteCallbacks.emplace_back(std::move(cb));
}

inline void SqlStatement::ProcessPostExecuteCallbacks() noexcept
{
    for (auto& cb: m_postExecuteCallbacks)
        cb();
    m_postExecuteCallbacks.clear();
}

inline void SqlStatement::PlanPostProcessOutputColumn(std::function<void()>&& cb)
{
    m_postProcessOutputColumnCallbacks.emplace_back(std::move(cb));
}

template <typename T>
SqlResult<T> SqlStatement::ExecuteDirectScalar(const std::string_view& query, std::source_location location) noexcept
{
    // clang-format off
    return ExecuteDirect(query, location)
        .and_then([&] { return FetchRow(); })
        .and_then([&] { return GetColumn<T>(1); });
    // clang-format on
}

// }}}
