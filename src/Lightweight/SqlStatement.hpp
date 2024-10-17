// SPDX-License-Identifier: MIT
#pragma once

#include <stdexcept>
#if defined(_WIN32) || defined(_WIN64)
    #include <Windows.h>
#endif

#include "SqlConnection.hpp"
#include "SqlDataBinder.hpp"

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
    SqlStatement(SqlConnection& relatedConnection);

    ~SqlStatement() noexcept final;

    // Retrieves the connection associated with this statement.
    [[nodiscard]] SqlConnection& Connection() noexcept;

    // Retrieves the connection associated with this statement.
    [[nodiscard]] SqlConnection const& Connection() const noexcept;

    // Retrieves the native handle of the statement.
    [[nodiscard]] SQLHSTMT NativeHandle() const noexcept;

    // Prepares the statement for execution.
    void Prepare(std::string_view query);

    template <SqlInputParameterBinder Arg>
    void BindInputParameter(SQLSMALLINT columnIndex, Arg const& arg);

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

    // Executes the prepared statement on a a batch of data.
    //
    // Each parameter represents a column, to be bound as input parameter.
    // The element types of each column container must be explicitly supported.
    //
    // In order to support column value types, their underlying storage must be contiguous.
    // Also the input range itself must be contiguous.
    // If any of these conditions are not met, the function will not compile - use ExecuteBatch() instead.
    template <SqlInputParameterBatchBinder FirstColumnBatch, std::ranges::contiguous_range... MoreColumnBatches>
    void ExecuteBatchNative(FirstColumnBatch const& firstColumnBatch, MoreColumnBatches const&... moreColumnBatches);

    // Executes the prepared statement on a a batch of data.
    //
    // Each parameter represents a column, to be bound as input parameter,
    // and the number of elements in these bound column containers will
    // mandate how many executions will happen.
    template <SqlInputParameterBatchBinder FirstColumnBatch, std::ranges::range... MoreColumnBatches>
    void ExecuteBatch(FirstColumnBatch const& firstColumnBatch, MoreColumnBatches const&... moreColumnBatches);

    // Executes the given query directly.
    void ExecuteDirect(const std::string_view& query, std::source_location location = std::source_location::current());

    // Executes the given query, assuming that only one result row and column is affected, that one will be
    // returned.
    template <typename T>
    [[nodiscard]] std::optional<T> ExecuteDirectScalar(const std::string_view& query,
                                                       std::source_location location = std::source_location::current());

    // Retrieves the number of rows affected by the last query.
    [[nodiscard]] size_t NumRowsAffected() const;

    // Retrieves the number of columns affected by the last query.
    [[nodiscard]] size_t NumColumnsAffected() const;

    // Retrieves the last insert ID of the last query's primary key.
    [[nodiscard]] size_t LastInsertId();

    // Fetches the next row of the result set.
    [[nodiscard]] bool FetchRow();

    // Retrieves the value of the column at the given index for the currently selected row.
    template <SqlGetColumnNativeType T>
    void GetColumn(SQLUSMALLINT column, T* result) const;

    // Retrieves the value of the column at the given index for the currently selected row.
    template <SqlGetColumnNativeType T>
    [[nodiscard]] T GetColumn(SQLUSMALLINT column) const;

  private:
    void RequireSuccess(SQLRETURN error, std::source_location sourceLocation = std::source_location::current()) const;
    void PlanPostExecuteCallback(std::function<void()>&& cb) override;
    void PlanPostProcessOutputColumn(std::function<void()>&& cb) override;
    void ProcessPostExecuteCallbacks();

    // private data members

    std::optional<SqlConnection> m_ownedConnection; // The connection object (if owned)
    SqlConnection* m_connection {};                 // Pointer to the connection object
    SQLHSTMT m_hStmt {};                            // The native oDBC statement handle
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

template <SqlOutputColumnBinder... Args>
void SqlStatement::BindOutputColumns(Args*... args)
{
    auto const numColumns = NumColumnsAffected();
    m_indicators.resize(numColumns + 1);

    SQLUSMALLINT i = 0;
    ((++i, SqlDataBinder<Args>::OutputColumn(m_hStmt, i, args, &m_indicators[i], *this)), ...);
}

template <SqlOutputColumnBinder T>
void SqlStatement::BindOutputColumn(SQLUSMALLINT columnIndex, T* arg)
{
    if (m_indicators.size() <= columnIndex)
        m_indicators.resize(NumColumnsAffected() + 1);

    SqlDataBinder<T>::OutputColumn(m_hStmt, columnIndex, arg, &m_indicators[columnIndex], *this);
}

template <SqlInputParameterBinder Arg>
void SqlStatement::BindInputParameter(SQLSMALLINT columnIndex, Arg const& arg)
{
    // tell Execute() that we don't know the expected count
    m_expectedParameterCount = (std::numeric_limits<decltype(m_expectedParameterCount)>::max)();
    RequireSuccess(SqlDataBinder<Arg>::InputParameter(m_hStmt, columnIndex, arg));
}

template <SqlInputParameterBinder... Args>
void SqlStatement::Execute(Args const&... args)
{
    // Each input parameter must have an address,
    // such that we can call SQLBindParameter() without needing to copy it.
    // The memory region behind the input parameter must exist until the SQLExecute() call.

    SqlLogger::GetLogger().OnExecute();

    if (!(m_expectedParameterCount == (std::numeric_limits<decltype(m_expectedParameterCount)>::max)()
          && sizeof...(args) == 0)
        && !(m_expectedParameterCount == sizeof...(args)))
        throw std::invalid_argument { "Invalid argument count" };

    SQLUSMALLINT i = 0;
    ((++i, SqlDataBinder<Args>::InputParameter(m_hStmt, i, args)), ...);

    RequireSuccess(SQLExecute(m_hStmt));
    ProcessPostExecuteCallbacks();
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
    RequireSuccess(SQLSetStmtAttr(m_hStmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER) rowCount, 0));
    RequireSuccess(SQLSetStmtAttr(m_hStmt, SQL_ATTR_PARAM_BIND_OFFSET_PTR, &rowStart, 0));
    RequireSuccess(SQLSetStmtAttr(m_hStmt, SQL_ATTR_PARAM_BIND_TYPE, SQL_PARAM_BIND_BY_COLUMN, 0));
    RequireSuccess(SQLSetStmtAttr(m_hStmt, SQL_ATTR_PARAM_OPERATION_PTR, SQL_PARAM_PROCEED, 0));
    RequireSuccess(SqlDataBinder<std::remove_cvref_t<decltype(*std::ranges::data(firstColumnBatch))>>::
                                   InputParameter(m_hStmt, 1, *std::ranges::data(firstColumnBatch)));
    SQLUSMALLINT column = 1;
    (RequireSuccess(SqlDataBinder<std::remove_cvref_t<decltype(*std::ranges::data(moreColumnBatches))>>::
                        InputParameter(m_hStmt, ++column, *std::ranges::data(moreColumnBatches))), ...);
    RequireSuccess(SQLExecute(m_hStmt));
    ProcessPostExecuteCallbacks();
    // clang-format on
}

template <SqlInputParameterBatchBinder FirstColumnBatch, std::ranges::range... MoreColumnBatches>
void SqlStatement::ExecuteBatch(FirstColumnBatch const& firstColumnBatch, MoreColumnBatches const&... moreColumnBatches)
{
    // If the input ranges are contiguous and their element types are contiguous and supported as well,
    // we can use the native batch execution.
    if constexpr (SqlNativeBatchable<FirstColumnBatch, MoreColumnBatches...>)
    {
        ExecuteBatchNative(firstColumnBatch, moreColumnBatches...);
        return;
    }

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
                ((++column, SqlDataBinder<ColumnValues>::InputParameter(m_hStmt, column, columnsInRow)), ...);
                RequireSuccess(SQLExecute(m_hStmt));
                ProcessPostExecuteCallbacks();
            },
            std::make_tuple(std::ref(*std::ranges::next(std::ranges::begin(firstColumnBatch), rowIndex)),
                            std::ref(*std::ranges::next(std::ranges::begin(moreColumnBatches), rowIndex))...));
    }
}

template <SqlGetColumnNativeType T>
inline void SqlStatement::GetColumn(SQLUSMALLINT column, T* result) const
{
    SQLLEN indicator {}; // TODO: Handle NULL values if we find out that we need them for our use-cases.
    RequireSuccess(SqlDataBinder<T>::GetColumn(m_hStmt, column, result, &indicator));
}

template <SqlGetColumnNativeType T>
[[nodiscard]] T SqlStatement::GetColumn(SQLUSMALLINT column) const
{
    T result {};
    SQLLEN indicator {}; // TODO: Handle NULL values if we find out that we need them for our use-cases.
    RequireSuccess(SqlDataBinder<T>::GetColumn(m_hStmt, column, &result, &indicator));
    return result;
}

inline void SqlStatement::PlanPostExecuteCallback(std::function<void()>&& cb)
{
    m_postExecuteCallbacks.emplace_back(std::move(cb));
}

inline void SqlStatement::ProcessPostExecuteCallbacks()
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
std::optional<T> SqlStatement::ExecuteDirectScalar(const std::string_view& query, std::source_location location)
{
    ExecuteDirect(query, location);
    if (FetchRow())
        return { GetColumn<T>(1) };
    return std::nullopt;
}

// }}}
