#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #include <Windows.h>
#endif

#include <chrono>
#include <compare>
#include <cstring>
#include <ctime>
#include <functional>
#include <print>
#include <string>
#include <utility>
#include <variant>

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

// Helper struct to store a string that should be automatically trimmed when fetched from the database.
// This is only needed for compatibility with old columns that hard-code the length, like CHAR(50).
struct SqlTrimmedString
{
    std::string value;

    std::strong_ordering operator<=>(SqlTrimmedString const&) const noexcept = default;
};

// Helper struct to store a timestamp that should be automatically converted to/from a SQL_TIMESTAMP_STRUCT.
struct SqlTimestamp
{
    SqlTimestamp() noexcept = default;
    SqlTimestamp(SqlTimestamp&&) noexcept = default;
    SqlTimestamp& operator=(SqlTimestamp&&) noexcept = default;
    SqlTimestamp(SqlTimestamp const&) noexcept = default;
    SqlTimestamp& operator=(SqlTimestamp const& other) noexcept = default;
    ~SqlTimestamp() noexcept = default;

    bool operator==(SqlTimestamp const& other) const noexcept
    {
        return value == other.value;
    }
    bool operator!=(SqlTimestamp const& other) const noexcept
    {
        return !(*this == other);
    }

    SqlTimestamp(std::chrono::year_month_day ymd, std::chrono::hh_mm_ss<std::chrono::microseconds> time) noexcept:
        SqlTimestamp(std::chrono::system_clock::from_time_t(std::chrono::system_clock::to_time_t(
            std::chrono::sys_days { ymd } + time.hours() + time.minutes() + time.seconds())))
    {
    }

    SqlTimestamp(std::chrono::year year,
                 std::chrono::month month,
                 std::chrono::day day,
                 std::chrono::hours hour,
                 std::chrono::minutes minute,
                 std::chrono::seconds second,
                 std::chrono::microseconds microsecond = std::chrono::microseconds { 0 }) noexcept:
        SqlTimestamp(std::chrono::year_month_day { year, month, day },
                     std::chrono::hh_mm_ss<std::chrono::microseconds> { hour + minute + second + microsecond })
    {
    }

    SqlTimestamp(std::chrono::system_clock::time_point value) noexcept:
        value { value },
        sqlValue { SqlTimestamp::ConvertToSqlValue(value) }
    {
    }

    operator std::chrono::system_clock::time_point() const noexcept
    {
        return value;
    }

    static SQL_TIMESTAMP_STRUCT ConvertToSqlValue(std::chrono::system_clock::time_point value) noexcept
    {
        auto const ymd = std::chrono::year_month_day { std::chrono::floor<std::chrono::days>(value) };
        auto const hms = std::chrono::hh_mm_ss { value - std::chrono::floor<std::chrono::days>(value) };
        return SQL_TIMESTAMP_STRUCT {
            .year = (SQLSMALLINT) (int) ymd.year(),
            .month = (SQLUSMALLINT) (unsigned) ymd.month(),
            .day = (SQLUSMALLINT) (unsigned) ymd.day(),
            .hour = (SQLUSMALLINT) hms.hours().count(),
            .minute = (SQLUSMALLINT) hms.minutes().count(),
            .second = (SQLUSMALLINT) hms.seconds().count(),
            .fraction = (SQLUINTEGER) hms.subseconds().count() * 1000,
        };
    }

    static std::chrono::system_clock::time_point ConvertToNative(SQL_TIMESTAMP_STRUCT const& time) noexcept
    {
        // clang-format off
        using namespace std::chrono;
        auto timepoint = sys_days(year_month_day(year(time.year), month(time.month), day(time.day)))
                       + hours(time.hour)
                       + minutes(time.minute)
                       + seconds(time.second)
                       + microseconds(time.fraction / 1000);
        return timepoint;
        // clang-format on
    }

    std::chrono::system_clock::time_point value;
    SQL_TIMESTAMP_STRUCT sqlValue {};
    SQLLEN sqlIndicator {};
};

namespace detail
{

template <typename Integer = int>
constexpr Integer toInteger(std::string_view s, Integer fallback) noexcept
{
    Integer value {};
    auto const rc = std::from_chars(s.data(), s.data() + s.size(), value);
#if __cpp_lib_to_chars >= 202306L
    if (rc)
#else
    if (rc.ec == std::errc {})
#endif
        return value;
    else
        return fallback;
}

} // namespace detail

// Helper struct to store a date (without time of the day) to write to or read from a database.
struct SqlDate
{
    std::chrono::year_month_day value {};
    SQL_DATE_STRUCT sqlValue {};

    SqlDate() noexcept = default;
    SqlDate(SqlDate&&) noexcept = default;
    SqlDate& operator=(SqlDate&&) noexcept = default;
    SqlDate(SqlDate const&) noexcept = default;
    SqlDate& operator=(SqlDate const&) noexcept = default;
    ~SqlDate() noexcept = default;

    bool operator==(SqlDate const& other) const noexcept
    {
        return value == other.value;
    }

    bool operator!=(SqlDate const& other) const noexcept
    {
        return !(*this == other);
    }

    SqlDate(std::chrono::year_month_day value) noexcept:
        value { value },
        sqlValue { SqlDate::ConvertToSqlValue(value) }
    {
    }

    static SqlDate Today() noexcept
    {
        return SqlDate { std::chrono::year_month_day { std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()) } };
    }

    static SQL_DATE_STRUCT ConvertToSqlValue(std::chrono::year_month_day value) noexcept
    {
        return SQL_DATE_STRUCT {
            .year = (SQLSMALLINT) (int) value.year(),
            .month = (SQLUSMALLINT) (unsigned) value.month(),
            .day = (SQLUSMALLINT) (unsigned) value.day(),
        };
    }

    static std::chrono::year_month_day ConvertToNative(SQL_DATE_STRUCT const& value) noexcept
    {
        return std::chrono::year_month_day { std::chrono::year { value.year },
                                             std::chrono::month { static_cast<unsigned>(value.month) },
                                             std::chrono::day { static_cast<unsigned>(value.day) } };
    }
};

// Helper struct to store a time (of the day) to write to or read from a database.
struct SqlTime
{
    std::chrono::hh_mm_ss<std::chrono::seconds> value {};
    SQL_TIME_STRUCT sqlValue {};

    SqlTime() noexcept = default;
    SqlTime(SqlTime&&) noexcept = default;
    SqlTime& operator=(SqlTime&&) noexcept = default;
    SqlTime(SqlTime const&) noexcept = default;
    SqlTime& operator=(SqlTime const&) noexcept = default;
    ~SqlTime() noexcept = default;

    bool operator==(SqlTime const& other) const noexcept
    {
        return value.to_duration().count() == other.value.to_duration().count();
    }

    bool operator!=(SqlTime const& other) const noexcept
    {
        return !(*this == other);
    }

    SqlTime(std::chrono::hh_mm_ss<std::chrono::seconds> value) noexcept:
        value { value },
        sqlValue { SqlTime::ConvertToSqlValue(value) }
    {
    }

    SqlTime(std::chrono::hours hour, std::chrono::minutes minute, std::chrono::seconds second) noexcept:
        SqlTime(std::chrono::hh_mm_ss { hour + minute + second })
    {
    }

    static SQL_TIME_STRUCT ConvertToSqlValue(std::chrono::hh_mm_ss<std::chrono::seconds> value) noexcept
    {
        return SQL_TIME_STRUCT {
            .hour = (SQLUSMALLINT) value.hours().count(),
            .minute = (SQLUSMALLINT) value.minutes().count(),
            .second = (SQLUSMALLINT) value.seconds().count(),
        };
    }

    static std::chrono::hh_mm_ss<std::chrono::seconds> ConvertToNative(SQL_TIME_STRUCT const& value) noexcept
    {
        return std::chrono::hh_mm_ss<std::chrono::seconds> { std::chrono::hours { (int) value.hour }
                                                             + std::chrono::minutes { (unsigned) value.minute }
                                                             + std::chrono::seconds { (unsigned) value.second } };
    }
};

// Helper struct to generically store and load a variant of different SQL types.
using SqlVariant = std::variant<std::monostate,
                                bool,
                                short,
                                unsigned short,
                                int,
                                unsigned int,
                                long long,
                                unsigned long long,
                                float,
                                double,
                                std::string,
                                SqlDate,
                                SqlTime,
                                SqlTimestamp>;

// Callback interface for SqlDataBinder to allow post-processing of output columns.
//
// This is needed because the SQLBindCol() function does not allow to specify a callback function to be called
// after the data has been fetched from the database. This is needed to trim strings to the correct size, for example.
class SqlDataBinderCallback
{
  public:
    virtual ~SqlDataBinderCallback() = default;

    virtual void PlanPostExecuteCallback(std::function<void()>&&) = 0;
    virtual void PlanPostProcessOutputColumn(std::function<void()>&&) = 0;
};

template <typename>
struct SqlDataBinder;

// clang-format off
template <typename T, SQLSMALLINT TheCType, SQLINTEGER TheSqlType>
struct SqlSimpleDataBinder
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, T const& value) noexcept
    {
        return SQLBindParameter(stmt, column, SQL_PARAM_INPUT, TheCType, TheSqlType, 0, 0, (SQLPOINTER) &value, 0, nullptr);
    }

    static SQLRETURN OutputColumn(SQLHSTMT stmt, SQLUSMALLINT column, T* result, SQLLEN* indicator, SqlDataBinderCallback&) noexcept
    {
        return SQLBindCol(stmt, column, TheCType, result, 0, indicator);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, T* result, SQLLEN* indicator) noexcept
    {
        return SQLGetData(stmt, column, TheCType, result, 0, indicator);
    }
};

template <> struct SqlDataBinder<bool>: SqlSimpleDataBinder<bool, SQL_BIT, SQL_BIT> {};
template <> struct SqlDataBinder<short>: SqlSimpleDataBinder<short, SQL_C_SSHORT, SQL_SMALLINT> {};
template <> struct SqlDataBinder<unsigned short>: SqlSimpleDataBinder<unsigned short, SQL_C_USHORT, SQL_SMALLINT> {};
template <> struct SqlDataBinder<int>: SqlSimpleDataBinder<int, SQL_C_SLONG, SQL_INTEGER> {};
template <> struct SqlDataBinder<unsigned int>: SqlSimpleDataBinder<unsigned int, SQL_C_ULONG, SQL_INTEGER> {};
template <> struct SqlDataBinder<long long>: SqlSimpleDataBinder<long long, SQL_C_SBIGINT, SQL_BIGINT> {};
template <> struct SqlDataBinder<unsigned long long>: SqlSimpleDataBinder<unsigned long long, SQL_C_UBIGINT, SQL_BIGINT> {};
template <> struct SqlDataBinder<float>: SqlSimpleDataBinder<float, SQL_C_FLOAT, SQL_REAL> {};
template <> struct SqlDataBinder<double>: SqlSimpleDataBinder<double, SQL_C_DOUBLE, SQL_DOUBLE> {};
// clang-format on

// Default traits for output string parameters
// This needs to be implemented for each string type that should be used as output parameter via SqlDataBinder<>.
// An std::string specialization is provided below.
// Feel free to add more specializations for other string types, such as CString, etc.
template <typename>
struct SqlOutputStringTraits;

// Specialized traits for std::string as output string parameter
template <>
struct SqlOutputStringTraits<std::string>
{
    static char const* Data(std::string const* str) noexcept
    {
        return str->data();
    }

    static char* Data(std::string* str) noexcept
    {
        return str->data();
    }

    static SQLULEN Size(std::string const* str) noexcept
    {
        return str->size();
    }

    static void Reserve(std::string* str, size_t capacity) noexcept
    {
        // std::string tries to defer the allocation as long as possible.
        // So we first tell std::string how much to reserve and then resize it to the *actually* reserved size.
        str->reserve(capacity);
        str->resize(str->capacity());
    }

    static void Resize(std::string* str, SQLLEN indicator) noexcept
    {
        if (indicator > 0)
            str->resize(indicator);
    }

    static void Clear(std::string* str) noexcept
    {
        str->clear();
    }
};

// clang-format off
template <typename StringType>
concept SqlOutputStringTraitsConcept = requires(StringType* str) {
    { SqlOutputStringTraits<StringType>::Data(str) } -> std::same_as<char*>;
    { SqlOutputStringTraits<StringType>::Size(str) } -> std::same_as<SQLULEN>;
    { SqlOutputStringTraits<StringType>::Reserve(str, size_t {}) } -> std::same_as<void>;
    { SqlOutputStringTraits<StringType>::Resize(str, SQLLEN {}) } -> std::same_as<void>;
    { SqlOutputStringTraits<StringType>::Clear(str) } -> std::same_as<void>;
};
// clang-format on

template <SqlOutputStringTraitsConcept StringType>
struct SqlDataBinder<StringType>
{
    using ValueType = StringType;
    using StringTraits = SqlOutputStringTraits<ValueType>;

    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, ValueType const& value) noexcept
    {
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_CHAR,
                                SQL_VARCHAR,
                                StringTraits::Size(&value),
                                0,
                                (SQLPOINTER) StringTraits::Data(&value),
                                0,
                                nullptr);
    }

    static SQLRETURN OutputColumn(
        SQLHSTMT stmt, SQLUSMALLINT column, ValueType* result, SQLLEN* indicator, SqlDataBinderCallback& cb) noexcept
    {
        cb.PlanPostProcessOutputColumn([indicator, result]() {
            // NB: If the indicator is greater than the buffer size, we have a truncation.
            auto const bufferSize = StringTraits::Size(result);
            auto const len = std::cmp_greater_equal(*indicator, bufferSize) || *indicator == SQL_NO_TOTAL
                                 ? bufferSize - 1
                                 : *indicator;
            StringTraits::Resize(result, len);
        });
        return SQLBindCol(stmt,
                          column,
                          SQL_C_CHAR,
                          (SQLPOINTER) StringTraits::Data(result),
                          (SQLLEN) StringTraits::Size(result),
                          indicator);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, ValueType* result, SQLLEN* indicator) noexcept
    {
        StringTraits::Reserve(result, 15);
        size_t writeIndex = 0;
        *indicator = 0;
        while (true)
        {
            char* const bufferStart = StringTraits::Data(result) + writeIndex;
            size_t const bufferSize = StringTraits::Size(result) - writeIndex;
            SQLRETURN rv = SQLGetData(stmt, column, SQL_C_CHAR, bufferStart, bufferSize, indicator);
            switch (rv)
            {
                case SQL_SUCCESS:
                case SQL_NO_DATA:
                    // last successive call
                    StringTraits::Resize(result, writeIndex + *indicator);
                    *indicator = StringTraits::Size(result);
                    return SQL_SUCCESS;
                case SQL_SUCCESS_WITH_INFO: {
                    // more data pending
                    auto const len = std::cmp_greater_equal(*indicator, bufferSize) || *indicator == SQL_NO_TOTAL
                                         ? bufferSize - 1
                                         : *indicator;
                    writeIndex += len;
                    StringTraits::Resize(result, writeIndex + *indicator + 1);
                    break;
                }
                default:
                    return rv;
            }
        }
    }
};

template <std::size_t N>
struct SqlDataBinder<char[N]>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, char const* value) noexcept
    {
        static_assert(N > 0, "N must be greater than 0"); // I cannot imagine that N is 0, ever.
        return SQLBindParameter(
            stmt, column, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, N - 1, 0, (SQLPOINTER) value, 0, nullptr);
    }
};

template <>
struct SqlDataBinder<std::string_view>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, std::string_view value) noexcept
    {
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_CHAR,
                                SQL_VARCHAR,
                                value.size(),
                                0,
                                (SQLPOINTER) value.data(),
                                0,
                                nullptr);
    }
};

template <>
struct SqlDataBinder<SqlTrimmedString>
{
    using InnerStringType = decltype(std::declval<SqlTrimmedString>().value);
    using StringTraits = SqlOutputStringTraits<InnerStringType>;

    static void TrimRight(InnerStringType* boundOutputString, SQLLEN indicator) noexcept
    {
        size_t n = indicator;
        while (n > 0 && std::isspace((*boundOutputString)[n - 1]))
            --n;
        StringTraits::Resize(boundOutputString, n);
    }

    static SQLRETURN OutputColumn(SQLHSTMT stmt,
                                  SQLUSMALLINT column,
                                  SqlTrimmedString* result,
                                  SQLLEN* indicator,
                                  SqlDataBinderCallback& cb) noexcept
    {
        auto* boundOutputString = &result->value;
        cb.PlanPostProcessOutputColumn([indicator, boundOutputString]() {
            // NB: If the indicator is greater than the buffer size, we have a truncation.
            auto const bufferSize = StringTraits::Size(boundOutputString);
            auto const len = std::cmp_greater_equal(*indicator, bufferSize) || *indicator == SQL_NO_TOTAL
                                 ? bufferSize - 1
                                 : *indicator;
            TrimRight(boundOutputString, len);
        });
        return SQLBindCol(stmt,
                          column,
                          SQL_C_CHAR,
                          (SQLPOINTER) StringTraits::Data(boundOutputString),
                          (SQLLEN) StringTraits::Size(boundOutputString),
                          indicator);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, SqlTrimmedString* result, SQLLEN* indicator) noexcept
    {
        auto const returnCode = SqlDataBinder<InnerStringType>::GetColumn(stmt, column, &result->value, indicator);
        TrimRight(&result->value, *indicator);
        return returnCode;
    }
};

template <>
struct SqlDataBinder<SqlDate>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, SqlDate const& value) noexcept
    {
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_TYPE_DATE,
                                SQL_TYPE_DATE,
                                0,
                                0,
                                (SQLPOINTER) &value.sqlValue,
                                0,
                                nullptr);
    }

    static SQLRETURN OutputColumn(
        SQLHSTMT stmt, SQLUSMALLINT column, SqlDate* result, SQLLEN* indicator, SqlDataBinderCallback& cb) noexcept
    {
        cb.PlanPostProcessOutputColumn(
            [indicator, result]() { result->value = SqlDate::ConvertToNative(result->sqlValue); });
        return SQLBindCol(stmt, column, SQL_C_TYPE_DATE, &result->sqlValue, sizeof(result->sqlValue), nullptr);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, SqlDate* result, SQLLEN* indicator) noexcept
    {
        SQLRETURN const sqlReturn =
            SQLGetData(stmt, column, SQL_C_TYPE_DATE, &result->sqlValue, sizeof(result->sqlValue), indicator);
        if (SQL_SUCCEEDED(sqlReturn))
            result->value = SqlDate::ConvertToNative(result->sqlValue);
        return sqlReturn;
    }
};

template <>
struct SqlDataBinder<SqlTime>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, SqlTime const& value) noexcept
    {
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_TYPE_TIME,
                                SQL_TYPE_TIME,
                                0,
                                0,
                                (SQLPOINTER) &value.sqlValue,
                                0,
                                nullptr);
    }

    static SQLRETURN OutputColumn(
        SQLHSTMT stmt, SQLUSMALLINT column, SqlTime* result, SQLLEN* indicator, SqlDataBinderCallback& cb) noexcept
    {
        cb.PlanPostProcessOutputColumn(
            [indicator, result]() { result->value = SqlTime::ConvertToNative(result->sqlValue); });
        return SQLBindCol(stmt, column, SQL_C_TYPE_TIME, &result->sqlValue, sizeof(result->sqlValue), nullptr);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, SqlTime* result, SQLLEN* indicator) noexcept
    {
        SQLRETURN const sqlReturn =
            SQLGetData(stmt, column, SQL_C_TYPE_TIME, &result->sqlValue, sizeof(result->sqlValue), indicator);
        if (SQL_SUCCEEDED(sqlReturn))
            result->value = SqlTime::ConvertToNative(result->sqlValue);
        return sqlReturn;
    }
};

template <>
struct SqlDataBinder<std::chrono::system_clock::time_point>
{
    static SQLRETURN GetColumn(SQLHSTMT stmt,
                               SQLUSMALLINT column,
                               std::chrono::system_clock::time_point* result,
                               SQLLEN* indicator) noexcept
    {
        SQL_TIMESTAMP_STRUCT sqlValue {};
        auto const rc = SQLGetData(stmt, column, SQL_C_TYPE_TIMESTAMP, &sqlValue, sizeof(sqlValue), indicator);
        if (SQL_SUCCEEDED(rc))
            *result = SqlTimestamp::ConvertToNative(sqlValue);
        return rc;
    }
};

template <>
struct SqlDataBinder<SqlTimestamp>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, SqlTimestamp const& value) noexcept
    {
        const_cast<SqlTimestamp&>(value).sqlIndicator = sizeof(value.sqlValue);

        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_TYPE_TIMESTAMP,
                                SQL_TYPE_TIMESTAMP,
                                27,
                                7,
                                (SQLPOINTER) &value.sqlValue,
                                0,
                                &const_cast<SqlTimestamp&>(value).sqlIndicator);
    }

    static SQLRETURN OutputColumn(
        SQLHSTMT stmt, SQLUSMALLINT column, SqlTimestamp* result, SQLLEN* indicator, SqlDataBinderCallback& cb) noexcept
    {
        *indicator = sizeof(result->sqlValue);
        cb.PlanPostProcessOutputColumn(
            [indicator, result]() { result->value = SqlTimestamp::ConvertToNative(result->sqlValue); });
        return SQLBindCol(stmt, column, SQL_C_TYPE_TIMESTAMP, &result->sqlValue, 0, indicator);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, SqlTimestamp* result, SQLLEN* indicator) noexcept
    {
        return SqlDataBinder<std::chrono::system_clock::time_point>::GetColumn(stmt, column, &result->value, indicator);
    }
};

template <>
struct SqlDataBinder<SqlVariant>
{
    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, SqlVariant* result, SQLLEN* indicator) noexcept
    {
        SQLLEN columnType {};
        SQLRETURN returnCode = SQLColAttributeA(stmt, column, SQL_DESC_TYPE, nullptr, 0, nullptr, &columnType);
        if (!SQL_SUCCEEDED(returnCode))
            return returnCode;

        switch (columnType)
        {
            case SQL_BIT:
                result->emplace<bool>();
                returnCode = SqlDataBinder<bool>::GetColumn(stmt, column, &std::get<bool>(*result), indicator);
                break;
            case SQL_TINYINT:
                result->emplace<short>();
                returnCode = SqlDataBinder<short>::GetColumn(stmt, column, &std::get<short>(*result), indicator);
                break;
            case SQL_SMALLINT:
                result->emplace<unsigned short>();
                returnCode = SqlDataBinder<unsigned short>::GetColumn(
                    stmt, column, &std::get<unsigned short>(*result), indicator);
                break;
            case SQL_INTEGER:
                result->emplace<int>();
                returnCode = SqlDataBinder<int>::GetColumn(stmt, column, &std::get<int>(*result), indicator);
                break;
            case SQL_BIGINT:
                result->emplace<long long>();
                returnCode =
                    SqlDataBinder<long long>::GetColumn(stmt, column, &std::get<long long>(*result), indicator);
                break;
            case SQL_REAL:
                result->emplace<float>();
                returnCode = SqlDataBinder<float>::GetColumn(stmt, column, &std::get<float>(*result), indicator);
                break;
            case SQL_FLOAT:
            case SQL_DOUBLE:
                result->emplace<double>();
                returnCode = SqlDataBinder<double>::GetColumn(stmt, column, &std::get<double>(*result), indicator);
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
                result->emplace<std::string>();
                returnCode =
                    SqlDataBinder<std::string>::GetColumn(stmt, column, &std::get<std::string>(*result), indicator);
                break;
            case SQL_TYPE_DATE:
                result->emplace<SqlDate>();
                returnCode = SqlDataBinder<SqlDate>::GetColumn(stmt, column, &std::get<SqlDate>(*result), indicator);
                break;
            case SQL_TYPE_TIME:
                result->emplace<SqlTime>();
                returnCode = SqlDataBinder<SqlTime>::GetColumn(stmt, column, &std::get<SqlTime>(*result), indicator);
                break;
            case SQL_TYPE_TIMESTAMP:
                result->emplace<SqlTimestamp>();
                returnCode =
                    SqlDataBinder<SqlTimestamp>::GetColumn(stmt, column, &std::get<SqlTimestamp>(*result), indicator);
                break;
            case SQL_TYPE_NULL:
            case SQL_DECIMAL:
            case SQL_NUMERIC:
            case SQL_GUID:
                // TODO: Get them implemented on demand
                [[fallthrough]];
            default:
                SqlLogger::GetLogger().OnError(SqlError::UNSUPPORTED_TYPE, SqlErrorInfo::fromStatementHandle(stmt));
                returnCode = SQL_ERROR; // std::errc::invalid_argument;
        }
        if (*indicator == SQL_NULL_DATA)
            *result = std::monostate {};
        return returnCode;
    }
};

template <typename T>
concept SqlInputParameterBinder = requires(SQLHSTMT hStmt, SQLUSMALLINT column, T const& value) {
    { SqlDataBinder<T>::InputParameter(hStmt, column, value) } -> std::same_as<SQLRETURN>;
};

template <typename T>
concept SqlOutputColumnBinder =
    requires(SQLHSTMT hStmt, SQLUSMALLINT column, T* result, SQLLEN* indicator, SqlDataBinderCallback& cb) {
        { SqlDataBinder<T>::OutputColumn(hStmt, column, result, indicator, cb) } -> std::same_as<SQLRETURN>;
    };

template <typename T>
concept SqlInputParameterBatchBinder =
    requires(SQLHSTMT hStmt, SQLUSMALLINT column, std::ranges::range_value_t<T>* result) {
        {
            SqlDataBinder<std::ranges::range_value_t<T>>::InputParameter(
                hStmt, column, std::declval<std::ranges::range_value_t<T>>())
        } -> std::same_as<SQLRETURN>;
    };

template <typename T>
concept SqlGetColumnNativeType = requires(SQLHSTMT hStmt, SQLUSMALLINT column, T* result, SQLLEN* indicator) {
    { SqlDataBinder<T>::GetColumn(hStmt, column, result, indicator) } -> std::same_as<SQLRETURN>;
};
