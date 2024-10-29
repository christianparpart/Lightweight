// SPDX-License-Identifier: Apache-2.0

#include "BasicStringBinder.hpp"
#include "SqlVariant.hpp"

SQLRETURN SqlDataBinder<SqlVariant>::InputParameter(SQLHSTMT stmt,
                                                    SQLUSMALLINT column,
                                                    SqlVariant const& variantValue,
                                                    SqlDataBinderCallback& cb) noexcept
{
    return std::visit(detail::overloaded { [&]<typename T>(T const& value) {
                          return SqlDataBinder<T>::InputParameter(stmt, column, value, cb);
                      } },
                      variantValue.value);
}

SQLRETURN SqlDataBinder<SqlVariant>::GetColumn(
    SQLHSTMT stmt, SQLUSMALLINT column, SqlVariant* result, SQLLEN* indicator, SqlDataBinderCallback const& cb) noexcept
{
    SQLLEN columnType {};
    SQLRETURN returnCode =
        SQLColAttributeA(stmt, static_cast<SQLSMALLINT>(column), SQL_DESC_TYPE, nullptr, 0, nullptr, &columnType);
    if (!SQL_SUCCEEDED(returnCode))
        return returnCode;

    auto& variant = result->value;

    switch (columnType)
    {
        case SQL_BIT:
            returnCode = SqlDataBinder<bool>::GetColumn(stmt, column, &variant.emplace<bool>(), indicator, cb);
            break;
        case SQL_TINYINT:
            returnCode = SqlDataBinder<short>::GetColumn(stmt, column, &variant.emplace<short>(), indicator, cb);
            break;
        case SQL_SMALLINT:
            returnCode = SqlDataBinder<unsigned short>::GetColumn(
                stmt, column, &variant.emplace<unsigned short>(), indicator, cb);
            break;
        case SQL_INTEGER:
            returnCode = SqlDataBinder<int>::GetColumn(stmt, column, &variant.emplace<int>(), indicator, cb);
            break;
        case SQL_BIGINT:
            returnCode =
                SqlDataBinder<long long>::GetColumn(stmt, column, &variant.emplace<long long>(), indicator, cb);
            break;
        case SQL_REAL:
            returnCode = SqlDataBinder<float>::GetColumn(stmt, column, &variant.emplace<float>(), indicator, cb);
            break;
        case SQL_FLOAT:
        case SQL_DOUBLE:
            returnCode = SqlDataBinder<double>::GetColumn(stmt, column, &variant.emplace<double>(), indicator, cb);
            break;
        case SQL_CHAR:          // fixed-length string
        case SQL_VARCHAR:       // variable-length string
        case SQL_LONGVARCHAR:   // long string
        case SQL_WCHAR:         // fixed-length Unicode (UTF-16) string
        case SQL_WVARCHAR:      // variable-length Unicode (UTF-16) string
        case SQL_WLONGVARCHAR:  // long Unicode (UTF-16) string
        case SQL_BINARY:        // fixed-length binary
        case SQL_VARBINARY:     // variable-length binary
        case SQL_LONGVARBINARY: // long binary
            returnCode =
                SqlDataBinder<std::string>::GetColumn(stmt, column, &variant.emplace<std::string>(), indicator, cb);
            break;
        case SQL_DATE:
            SqlLogger::GetLogger().OnWarning(
                std::format("SQL_DATE is from ODBC 2. SQL_TYPE_DATE should have been received instead."));
            [[fallthrough]];
        case SQL_TYPE_DATE:
            returnCode = SqlDataBinder<SqlDate>::GetColumn(stmt, column, &variant.emplace<SqlDate>(), indicator, cb);
            break;
        case SQL_TIME:
            SqlLogger::GetLogger().OnWarning(
                std::format("SQL_TIME is from ODBC 2. SQL_TYPE_TIME should have been received instead."));
            [[fallthrough]];
        case SQL_TYPE_TIME:
        case SQL_SS_TIME2:
            returnCode = SqlDataBinder<SqlTime>::GetColumn(stmt, column, &variant.emplace<SqlTime>(), indicator, cb);
            break;
        case SQL_TYPE_TIMESTAMP:
            returnCode =
                SqlDataBinder<SqlDateTime>::GetColumn(stmt, column, &variant.emplace<SqlDateTime>(), indicator, cb);
            break;
        case SQL_TYPE_NULL:
        case SQL_DECIMAL:
        case SQL_NUMERIC:
        case SQL_GUID:
            // TODO: Get them implemented on demand
            [[fallthrough]];
        default:
            SqlLogger::GetLogger().OnError(SqlError::UNSUPPORTED_TYPE);
            returnCode = SQL_ERROR; // std::errc::invalid_argument;
    }
    if (indicator && *indicator == SQL_NULL_DATA)
        variant = SqlNullValue;
    return returnCode;
}
