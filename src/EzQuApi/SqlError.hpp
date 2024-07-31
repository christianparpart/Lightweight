#pragma once

#include <system_error>

#include <sql.h>
#include <sqlext.h>
#include <sqlspi.h>
#include <sqltypes.h>

// NOTE: This is a simple wrapper around the SQL return codes. It is not meant to be
// comprehensive, but rather to provide a simple way to convert SQL return codes to
// std::error_code.
//
// The code below is DRAFT and may be subject to change.

enum class SqlError
{
    SUCCESS = SQL_SUCCESS,
    SUCCESS_WITH_INFO = SQL_SUCCESS_WITH_INFO,
    NO_DATA = SQL_NO_DATA,
    ERROR = SQL_ERROR,
    INVALID_HANDLE = SQL_INVALID_HANDLE,
    STILL_EXECUTING = SQL_STILL_EXECUTING,
    NEED_DATA = SQL_NEED_DATA,
    PARAM_DATA_AVAILABLE = SQL_PARAM_DATA_AVAILABLE,
    NO_DATA_FOUND = SQL_NO_DATA_FOUND,
};

struct SqlErrorCategory: std::error_category
{
    const char* name() const noexcept override { return "sql"; }

    std::string message(int code) const override
    {
        switch (code)
        {
            case SQL_SUCCESS: return "SQL_SUCCESS";
            case SQL_SUCCESS_WITH_INFO: return "SQL_SUCCESS_WITH_INFO";
            case SQL_NO_DATA: return "SQL_NO_DATA";
            case SQL_ERROR: return "SQL_ERROR";
            case SQL_INVALID_HANDLE: return "SQL_INVALID_HANDLE";
            case SQL_STILL_EXECUTING: return "SQL_STILL_EXECUTING";
            case SQL_NEED_DATA: return "SQL_NEED_DATA";
            case SQL_PARAM_DATA_AVAILABLE: return "SQL_PARAM_DATA_AVAILABLE";
        }
        return "unknown";
    }
};

inline std::error_code make_error_code(SqlError e)
{
    static SqlErrorCategory const category;
    return { static_cast<int>(e), category };
}
