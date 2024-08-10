#pragma once
#if defined(_WIN32) || defined(_WIN64)
    #include <Windows.h>
#endif

#include "SqlConnection.hpp"
#include "SqlError.hpp"
#include "SqlStatement.hpp"

#include <sql.h>
#include <sqlext.h>
#include <sqlspi.h>
#include <sqltypes.h>

class SqlInfo
{
  public:
    explicit SqlInfo(SqlConnection* connection):
        m_connection(&connection)
    {
        detail::UpdateSqlError(&m_errorCode, SQLAllocHandle(SQL_HANDLE_STMT, m_connection.NativeHandle(), &m_hStmt));
    }

    ~SqlInfo()
    {
        SQLFreeHandle(SQL_HANDLE_STMT, m_hStmt);
    }

    SqlResult<std::string> columnName(int column) const
    {
        std::string name(128, '\0');
        SQLSMALLINT nameLength {};
        detail::UpdateSqlError(&m_errorCode,
                               SQLColAttributeA(m_hStmt,
                                                column,
                                                SQL_DESC_NAME,
                                                name.data(),
                                                static_cast<SQLSMALLINT>(name.size()),
                                                &nameLength,
                                                nullptr));
        if (m_errorCode)
            return std::unexpected { m_errorCode };
        name.resize(nameLength);
        return { std::move(name) };
    }

    SqlResult<std::vector<std::string>> columnNames(StdStringViewLike auto&& tableName) const
    {
        auto stmt = SqlStatement { m_connection };
        SQLColumnsA(stmt.NativeHandle(),
                    nullptr, // cataloge name
                    0,
                    nullptr, // schema name
                    0,
                    (SQLCHAR*) tableName.data(),
                    static_cast<SQLSMALLINT>(tableName.size()),
                    nullptr, // column name
                    0);
        stmt.Execute();
    }

  private:
    SqlConnection& m_connection;
    SQLHSTMT m_hStmt {};
    mutable std::error_code m_errorCode;
};
