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
    // Each parameter represents a column, in to be bound as input parameter,
    // and the number of elements in these bound column containers will
    // mandate how many executions will happen.
    template <SqlInputParameterBatchBinder FirstColumnBatch, std::ranges::range... ColumnBatches>
    SqlResult<void> ExecuteBatch(FirstColumnBatch const& firstColumnBatch,
                                 ColumnBatches const&... moreColumnBatches) noexcept;

    // Executes the given query directly.
    [[nodiscard]] SqlResult<void> ExecuteDirect(
        const std::string& query, std::source_location location = std::source_location::current()) noexcept;

    // Retrieves the number of rows affected by the last query.
    [[nodiscard]] SqlResult<size_t> NumRowsAffected() const noexcept;

    // Retrieves the number of columns affected by the last query.
    [[nodiscard]] SqlResult<size_t> NumColumnsAffected() const noexcept;

    // Retrieves the last insert ID of the last query's primary key.
    [[nodiscard]] SqlResult<unsigned long long> LastInsertId() noexcept;

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
    m_expectedParameterCount = (std::numeric_limits<size_t>::max)();
    return UpdateLastError(SqlDataBinder<Arg>::InputParameter(m_hStmt, columnIndex, arg));
}

template <SqlInputParameterBinder... Args>
[[nodiscard]] SqlResult<void> SqlStatement::Execute(Args const&... args) noexcept
{
    // Each input parameter must have an address,
    // such that we can call SQLBindParameter() without needing to copy it.
    // The memory region behind the input parameter must exist until the SQLExecute() call.

    if (!(m_expectedParameterCount == std::numeric_limits<size_t>::max() && sizeof...(args) == 0)
        && !(m_expectedParameterCount == sizeof...(args)))
        return std::unexpected { SqlError::INVALID_ARGUMENT };

    SqlLogger::GetLogger().OnExecute();

    SQLUSMALLINT i = 0;
    ((++i, UpdateLastError(SqlDataBinder<Args>::InputParameter(m_hStmt, i, args))) && ...);
    if (m_lastError != SqlError::SUCCESS)
        return std::unexpected { m_lastError };

    return UpdateLastError(SQLExecute(m_hStmt)).and_then([&]() -> SqlResult<void> {
        ProcessPostExecuteCallbacks();
        return {};
    });
}

template <SqlInputParameterBatchBinder FirstColumnBatch, std::ranges::range... ColumnBatches>
SqlResult<void> SqlStatement::ExecuteBatch(FirstColumnBatch const& firstColumnBatch,
                                           ColumnBatches const&... moreColumnBatches) noexcept
{
    if (m_expectedParameterCount != 1 + sizeof...(moreColumnBatches))
        // invalid number of columns
        return std::unexpected { SqlError::INVALID_ARGUMENT };

    const auto rowCount = std::ranges::size(firstColumnBatch);
    if (!((std::size(moreColumnBatches) == rowCount) && ...))
        // uneven number of rows
        return std::unexpected { SqlError::INVALID_ARGUMENT };

    m_lastError = SqlError::SUCCESS;
    for (auto const rowIndex: std::views::iota(size_t(0), rowCount))
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

// }}}
