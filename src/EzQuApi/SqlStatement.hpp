#pragma once

#include <EzQuApi/SqlConnection.hpp>

#if defined(_WIN32)
    #include <Windows.h>
#endif

#include <cstring>
#include <iostream>
#include <variant>
#include <vector>

#include <sql.h>
#include <sqlext.h>
#include <sqlspi.h>
#include <sqltypes.h>

// clang-format off
using SqlVariant = std::variant<
    bool,
    short,
    unsigned short,
    int,
    unsigned int,
    long long,
    unsigned long long,
    float,
    double,
    std::string
>;
// clang-format on

// High level API for (prepared) raw SQL statements
//
// TODO: Add support for transactions
class SqlStatement
{
  public:
    explicit SqlStatement(SqlConnection& connection) noexcept:
        m_connection { &connection }, m_hDbc { connection.NativeHandle() }
    {
        SetLastError(SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &m_hStmt));
    }

    ~SqlStatement() noexcept { SQLFreeHandle(SQL_HANDLE_STMT, m_hStmt); }

    void Prepare(std::string_view query) noexcept
    {
        // Closes the cursor if it is open
        SetLastError(SQLFreeStmt(m_hStmt, SQL_CLOSE));

        if (IsSuccess())
            SetLastError(
                SQLSetStmtAttr(m_hStmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_FORWARD_ONLY, SQL_IS_UINTEGER));

        // Prepares the statement
        if (IsSuccess())
            SetLastError(SQLPrepareA(m_hStmt, (SQLCHAR*) query.data(), (SQLINTEGER) query.size()));

        // Retrieves the number of parameters expected by the query
        if (IsSuccess())
        {
            SetLastError(SQLNumParams(m_hStmt, &m_expectedParameterCount));
            m_boundParameters.clear();
            m_boundParameters.reserve(m_expectedParameterCount);
        }
    }

    template <typename... Args>
    void Execute(Args&&... args) noexcept
    {
        if (m_expectedParameterCount != sizeof...(args))
            std::cerr << std::format(
                "Error: Expected {} parameters, but got {}\n", m_expectedParameterCount, sizeof...(args));

        m_boundParameterCount = 0;
        (BindParameter(std::forward<Args>(args)), ...);
        SetLastError(SQLExecute(m_hStmt));
    }

    void ExecuteDirect(const std::string& query) noexcept
    {
        SetLastError(SQLFreeStmt(m_hStmt, SQL_CLOSE));
        if (!IsSuccess())
            return;
        SetLastError(SQLExecDirectA(m_hStmt, (SQLCHAR*) query.c_str(), query.size()));
    }

    // Retrieves the number of rows affected by the last query.
    [[nodiscard]] size_t NumRowsAffected() const noexcept
    {
        SQLLEN numRowsAffected {};
        SetLastError(SQLRowCount(m_hStmt, &numRowsAffected)); // TODO: Seems to be the wrong function
        return numRowsAffected;
    }

    // Retrieves the number of columns affected by the last query.
    [[nodiscard]] size_t NumColumnsAffected() const noexcept
    {
        SQLSMALLINT numColumns {};
        SetLastError(SQLNumResultCols(m_hStmt, &numColumns));
        return numColumns;
    }

    unsigned long long LastInsertId() noexcept
    {
        switch (m_connection->ServerType())
        {
            case SqlServerType::MICROSOFT_SQL: ExecuteDirect("SELECT @@IDENTITY;"); break;
            case SqlServerType::POSTGRESQL: ExecuteDirect("SELECT lastval();"); break;
            case SqlServerType::UNKNOWN: return 0;
        }
        if (FetchRow())
            return GetColumn<unsigned long long>(1);
        return 0;
    }

    // Tests if the last query was successful.
    [[nodiscard]] bool IsSuccess() const noexcept
    {
        return m_lastError == SQL_SUCCESS || m_lastError == SQL_SUCCESS_WITH_INFO;
    }

    // Fetches the next row of the result set.
    [[nodiscard]] bool FetchRow() noexcept
    {
        m_lastError = SQLFetch(m_hStmt);
        CheckError();
        return m_lastError == SQL_SUCCESS || m_lastError == SQL_SUCCESS_WITH_INFO;
    }

    // Retrieves the diagnostic message for the last error.
    [[nodiscard]] std::string GetDiagnosticMessage() const
    {
        SQLCHAR sqlState[6] {};
        SQLINTEGER nativeError {};
        SQLCHAR errMsg[1024] {};
        SQLSMALLINT msgLen {};
        if (m_hStmt)
            SQLGetDiagRecA(SQL_HANDLE_STMT, m_hStmt, 1, sqlState, &nativeError, errMsg, sizeof(errMsg), &msgLen);
        else
            SQLGetDiagRecA(SQL_HANDLE_DBC, m_hDbc, 1, sqlState, &nativeError, errMsg, sizeof(errMsg), &msgLen);
        return std::string((char const*) errMsg, msgLen);
    }

    // {{{ GetColumn(...) overloads

    // Retrieves the value of the column at the given index for the currently selected row.
    void GetColumn(int column, std::string* result) const
    {
        result->clear();
        while (true)
        {
            SQLCHAR buffer[1024] {};
            SQLLEN len {};
            SetLastError(SQLGetData(m_hStmt, column, SQL_C_CHAR, buffer, sizeof(buffer), &len));
            switch (len)
            {
                case SQL_NO_TOTAL: result->append((char const*) buffer, sizeof(buffer) - 1); break;
                case SQL_NULL_DATA: return;
                default: result->append((char const*) buffer, len); return;
            }
        }
    }

    void GetColumn(int column, int* result) const
    {
        SetLastError(SQLGetData(m_hStmt, column, SQL_C_LONG, result, 0, nullptr));
    }

    void GetColumn(int column, long long* result) const
    {
        SetLastError(SQLGetData(m_hStmt, column, SQL_C_SBIGINT, result, 0, nullptr));
    }

    void GetColumn(int column, unsigned long long* result) const
    {
        SetLastError(SQLGetData(m_hStmt, column, SQL_C_UBIGINT, result, 0, nullptr));
    }

    void GetColumn(int column, double* result) const
    {
        SetLastError(SQLGetData(m_hStmt, column, SQL_C_DOUBLE, result, 0, nullptr));
    }

    void GetColumn(int column, float* result) const
    {
        SetLastError(SQLGetData(m_hStmt, column, SQL_C_FLOAT, result, 0, nullptr));
    }

    template <typename T>
    [[nodiscard]] T GetColumn(int column) const
    {
        T result {};
        GetColumn(column, &result);
        return result;
    }

    // }}}

  private:
    void SetLastError(SQLRETURN error) const noexcept
    {
        m_lastError = error;
        CheckError();
    }

    void CheckError() const noexcept
    {
        if (IsSuccess() || m_lastError == SQL_NO_DATA)
            return;

        auto const message = GetDiagnosticMessage();
        std::cerr << "SqlStatement error: " << message << '\n';
    }

    // {{{ BindParameter(...) overloads
    void BindParameter(bool arg) noexcept
    {
        m_boundParameters.emplace_back(arg);
        SetLastError(SQLBindParameter(m_hStmt,
                                      ++m_boundParameterCount,
                                      SQL_PARAM_INPUT,
                                      SQL_C_BIT,
                                      SQL_BIT,
                                      0,
                                      0,
                                      &std::get<decltype(arg)>(m_boundParameters.back()),
                                      0,
                                      nullptr));
    }

    void BindParameter(short arg) noexcept
    {
        m_boundParameters.emplace_back(arg);
        SetLastError(SQLBindParameter(m_hStmt,
                                      ++m_boundParameterCount,
                                      SQL_PARAM_INPUT,
                                      SQL_C_SSHORT,
                                      SQL_SMALLINT,
                                      0,
                                      0,
                                      (SQLPOINTER) &std::get<decltype(arg)>(m_boundParameters.back()),
                                      0,
                                      nullptr));
    }

    void BindParameter(unsigned short arg) noexcept
    {
        m_boundParameters.emplace_back(arg);
        SetLastError(SQLBindParameter(m_hStmt,
                                      ++m_boundParameterCount,
                                      SQL_PARAM_INPUT,
                                      SQL_C_USHORT,
                                      SQL_SMALLINT,
                                      0,
                                      0,
                                      (SQLPOINTER) &std::get<decltype(arg)>(m_boundParameters.back()),
                                      0,
                                      nullptr));
    }

    void BindParameter(int arg) noexcept
    {
        m_boundParameters.emplace_back(arg);
        SetLastError(SQLBindParameter(m_hStmt,
                                      ++m_boundParameterCount,
                                      SQL_PARAM_INPUT,
                                      SQL_C_SLONG,
                                      SQL_INTEGER,
                                      0,
                                      0,
                                      (SQLPOINTER) &std::get<int>(m_boundParameters.back()),
                                      0,
                                      nullptr));
    }

    void BindParameter(unsigned int arg) noexcept
    {
        m_boundParameters.emplace_back(arg);
        SetLastError(SQLBindParameter(m_hStmt,
                                      ++m_boundParameterCount,
                                      SQL_PARAM_INPUT,
                                      SQL_C_ULONG,
                                      SQL_NUMERIC,
                                      15,
                                      0,
                                      &std::get<decltype(arg)>(m_boundParameters.back()),
                                      0,
                                      nullptr));
    }

    void BindParameter(long long arg) noexcept
    {
        m_boundParameters.emplace_back(arg);
        SetLastError(SQLBindParameter(m_hStmt,
                                      ++m_boundParameterCount,
                                      SQL_PARAM_INPUT,
                                      SQL_C_SBIGINT,
                                      SQL_BIGINT,
                                      0,
                                      0,
                                      &std::get<decltype(arg)>(m_boundParameters.back()),
                                      0,
                                      nullptr));
    }

    void BindParameter(unsigned long long arg) noexcept
    {
        m_boundParameters.emplace_back(arg);
        SetLastError(SQLBindParameter(m_hStmt,
                                      ++m_boundParameterCount,
                                      SQL_PARAM_INPUT,
                                      SQL_C_UBIGINT,
                                      SQL_BIGINT,
                                      0,
                                      0,
                                      &std::get<decltype(arg)>(m_boundParameters.back()),
                                      0,
                                      nullptr));
    }

    void BindParameter(float arg) noexcept
    {
        m_boundParameters.emplace_back(arg);
        SetLastError(SQLBindParameter(m_hStmt,
                                      ++m_boundParameterCount,
                                      SQL_PARAM_INPUT,
                                      SQL_C_FLOAT,
                                      SQL_REAL,
                                      0,
                                      0,
                                      &std::get<decltype(arg)>(m_boundParameters.back()),
                                      0,
                                      nullptr));
    }

    void BindParameter(double arg) noexcept
    {
        m_boundParameters.emplace_back(arg);
        SetLastError(SQLBindParameter(m_hStmt,
                                      ++m_boundParameterCount,
                                      SQL_PARAM_INPUT,
                                      SQL_C_DOUBLE,
                                      SQL_DOUBLE,
                                      0,
                                      0,
                                      &std::get<decltype(arg)>(m_boundParameters.back()),
                                      0,
                                      nullptr));
    }

    void BindParameter(char const* arg) noexcept
    {
        SetLastError(SQLBindParameter(m_hStmt,
                                      ++m_boundParameterCount,
                                      SQL_PARAM_INPUT,
                                      SQL_C_CHAR,
                                      SQL_VARCHAR,
                                      std::strlen(arg) + 1,
                                      0,
                                      (SQLPOINTER) arg,
                                      0,
                                      nullptr));
    }

    void BindParameter(StdStringLike auto&& arg) noexcept
    {
        SetLastError(SQLBindParameter(m_hStmt,
                                      ++m_boundParameterCount,
                                      SQL_PARAM_INPUT,
                                      SQL_C_CHAR,
                                      SQL_VARCHAR,
                                      arg.size() + 1,
                                      0,
                                      (SQLPOINTER) arg.data(),
                                      arg.size(),
                                      nullptr));
    }

    void BindParameter(MFCStringLike auto&& arg) noexcept
    {
        SetLastError(SQLBindParameter(m_hStmt,
                                      ++m_boundParameterCount,
                                      SQL_PARAM_INPUT,
                                      SQL_C_CHAR,
                                      SQL_VARCHAR,
                                      arg.GetLength() + 1,
                                      0,
                                      (SQLPOINTER) arg.GetString(),
                                      arg.GetLength(),
                                      nullptr));
    }

    void BindParameter(RNStringLike auto&& arg) noexcept
    {
        SetLastError(SQLBindParameter(m_hStmt,
                                      ++m_boundParameterCount,
                                      SQL_PARAM_INPUT,
                                      SQL_C_CHAR,
                                      SQL_VARCHAR,
                                      arg.Length() + 1,
                                      0,
                                      (SQLPOINTER) arg.GetString(),
                                      arg.Length(),
                                      nullptr));
    }
    // }}}

    // private data members
    SqlConnection* m_connection;
    SQLHDBC m_hDbc;
    SQLHSTMT m_hStmt {};
    mutable SQLRETURN m_lastError { SQL_SUCCESS };
    SQLSMALLINT m_expectedParameterCount {};
    SQLUSMALLINT m_boundParameterCount {};
    std::vector<SqlVariant> m_boundParameters; // Holds the storage for the bound value parameters
};
