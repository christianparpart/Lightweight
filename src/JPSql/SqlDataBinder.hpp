// SPDX-License-Identifier: MIT
#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #include <Windows.h>
#endif

#include "SqlLogger.hpp"

#include <charconv>
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

// clang-format off
#if !defined(SQL_SS_TIME2)
// This is a Microsoft-specific extension to ODBC.
// It is supported by at lesat the following drivers:
// - SQL Server 2008 and later
// - MariaDB and MySQL ODBC drivers

#define SQL_SS_TIME2 -154

struct SQL_SS_TIME2_STRUCT
{
    SQLUSMALLINT hour;
    SQLUSMALLINT minute;
    SQLUSMALLINT second;
    SQLUINTEGER fraction;
};

static_assert(
    sizeof(SQL_SS_TIME2_STRUCT) == 12,
    "SQL_SS_TIME2_STRUCT size must be padded 12 bytes, as per ODBC extension spec."
);

#endif
// clang-format on

// Helper struct to store a string that should be automatically trimmed when fetched from the database.
// This is only needed for compatibility with old columns that hard-code the length, like CHAR(50).
struct SqlTrimmedString
{
    std::string value;

    std::weak_ordering operator<=>(SqlTrimmedString const&) const noexcept = default;
};

template <>
struct std::formatter<SqlTrimmedString>: std::formatter<std::string>
{
    auto format(SqlTrimmedString const& text, format_context& ctx) const -> format_context::iterator
    {
        return std::formatter<std::string>::format(text.value, ctx);
    }
};

// Represents a TEXT field in a SQL database.
//
// This is used for large texts, e.g. up to 65k characters.
struct SqlText
{
    using value_type = std::string;

    value_type value;

    std::weak_ordering operator<=>(SqlText const&) const noexcept = default;
};

enum class SqlStringPostRetrieveOperation
{
    NOTHING,
    TRIM_RIGHT,
};

// SQL fixed-capacity string that mimmicks standard library string/string_view with a fixed-size underlying
// buffer.
//
// The underlying storage will not be guaranteed to be `\0`-terminated unless
// a call to mutable/const c_str() has been performed.
template <std::size_t N,
          typename T = char,
          SqlStringPostRetrieveOperation PostOp = SqlStringPostRetrieveOperation::NOTHING>
class SqlFixedString
{
  private:
    T _data[N + 1] {};
    std::size_t _size = 0;

  public:
    using value_type = T;
    using iterator = T*;
    using const_iterator = T const*;
    using pointer_type = T*;
    using const_pointer_type = T const*;

    static constexpr inline std::size_t Capacity = N;
    static constexpr inline SqlStringPostRetrieveOperation PostRetrieveOperation = PostOp;

    template <std::size_t SourceSize>
    SqlFixedString(T const (&text)[SourceSize]):
        _data {},
        _size { SourceSize - 1 }
    {
        static_assert(SourceSize <= N + 1, "RHS string size must not exceed target string's capacity.");
        std::copy_n(text, SourceSize, _data);
    }

    SqlFixedString() = default;
    SqlFixedString(SqlFixedString const&) = default;
    SqlFixedString& operator=(SqlFixedString const&) = default;
    SqlFixedString(SqlFixedString&&) = default;
    SqlFixedString& operator=(SqlFixedString&&) = default;
    ~SqlFixedString() = default;

    void reserve(std::size_t capacity)
    {
        if (capacity > N)
            throw std::length_error(
                std::format("SqlFixedString: capacity {} exceeds maximum capacity {}", capacity, N));
    }

    constexpr bool empty() const noexcept
    {
        return _size == 0;
    }

    constexpr std::size_t size() const noexcept
    {
        return _size;
    }

    constexpr void setsize(std::size_t n) noexcept
    {
        auto const newSize = (std::min)(n, N);
        _size = newSize;
    }

    constexpr void resize(std::size_t n, T c = T {}) noexcept
    {
        auto const newSize = (std::min)(n, N);
        if (newSize > _size)
            std::fill_n(end(), newSize - _size, c);
        _size = newSize;
    }

    constexpr std::size_t capacity() const noexcept
    {
        return N;
    }

    constexpr void clear() noexcept
    {
        _size = 0;
    }

    template <std::size_t SourceSize>
    constexpr void assign(T const (&source)[SourceSize]) noexcept
    {
        static_assert(SourceSize <= N + 1, "Source string must not overflow the target string's capacity.");
        _size = SourceSize - 1;
        std::copy_n(source, SourceSize, _data);
    }

    constexpr void assign(std::string_view s) noexcept
    {
        _size = (std::min)(N, s.size());
        std::copy_n(s.data(), _size, _data);
    }

    constexpr void push_back(T c) noexcept
    {
        if (_size < N)
        {
            _data[_size] = c;
            ++_size;
        }
    }

    constexpr void pop_back() noexcept
    {
        if (_size > 0)
            --_size;
    }

    constexpr std::basic_string_view<T> substr(
        std::size_t offset = 0, std::size_t count = (std::numeric_limits<std::size_t>::max)()) const noexcept
    {
        if (offset >= _size)
            return {};
        if (count == (std::numeric_limits<std::size_t>::max)())
            return std::basic_string_view<T>(_data + offset, _size - offset);
        if (offset + count > _size)
            return std::basic_string_view<T>(_data + offset, _size - offset);
        return std::basic_string_view<T>(_data + offset, count);
    }

    // clang-format off
    constexpr pointer_type c_str() noexcept { _data[_size] = '\0'; return _data; }
    constexpr pointer_type data() noexcept { return _data; }
    constexpr iterator begin() noexcept { return _data; }
    constexpr iterator end() noexcept { return _data + size(); }
    constexpr T& at(std::size_t i) noexcept { return _data[i]; }
    constexpr T& operator[](std::size_t i) noexcept { return _data[i]; }

    constexpr const_pointer_type c_str() const noexcept { const_cast<T*>(_data)[_size] = '\0'; return _data; }
    constexpr const_pointer_type data() const noexcept { return _data; }
    constexpr const_iterator begin() const noexcept { return _data; }
    constexpr const_iterator end() const noexcept { return _data + size(); }
    constexpr T const& at(std::size_t i) const noexcept { return _data[i]; }
    constexpr T const& operator[](std::size_t i) const noexcept { return _data[i]; }
    // clang-format on

    template <std::size_t OtherSize, SqlStringPostRetrieveOperation OtherPostOp>
    std::weak_ordering operator<=>(SqlFixedString<OtherSize, T, OtherPostOp> const& other) const noexcept
    {
        if ((void*) this != (void*) &other)
        {
            for (std::size_t i = 0; i < (std::min)(N, OtherSize); ++i)
                if (auto const cmp = _data[i] <=> other._data[i]; cmp != std::weak_ordering::equivalent)
                    return cmp;
            if constexpr (N != OtherSize)
                return N <=> OtherSize;
        }
        return std::weak_ordering::equivalent;
    }

    template <std::size_t OtherSize, SqlStringPostRetrieveOperation OtherPostOp>
    constexpr bool operator==(SqlFixedString<OtherSize, T, OtherPostOp> const& other) const noexcept
    {
        return (*this <=> other) == std::weak_ordering::equivalent;
    }

    template <std::size_t OtherSize, SqlStringPostRetrieveOperation OtherPostOp>
    constexpr bool operator!=(SqlFixedString<OtherSize, T, OtherPostOp> const& other) const noexcept
    {
        return !(*this == other);
    }

    constexpr bool operator==(std::string_view other) const noexcept
    {
        return (substr() <=> other) == std::weak_ordering::equivalent;
    }

    constexpr bool operator!=(std::string_view other) const noexcept
    {
        return !(*this == other);
    }
};

template <std::size_t N, typename T = char>
using SqlTrimmedFixedString = SqlFixedString<N, T, SqlStringPostRetrieveOperation::TRIM_RIGHT>;

template <std::size_t N, typename T, SqlStringPostRetrieveOperation P>
struct std::formatter<SqlFixedString<N, T, P>>: std::formatter<std::string>
{
    using value_type = SqlFixedString<N, T, P>;
    auto format(value_type const& text, format_context& ctx) const -> format_context::iterator
    {
        return std::formatter<std::string>::format(text.c_str(), ctx);
    }
};

template <>
struct std::formatter<SqlText>: std::formatter<std::string>
{
    auto format(SqlText const& text, format_context& ctx) const -> format_context::iterator
    {
        return std::formatter<std::string>::format(text.value, ctx);
    }
};

// Helper struct to store a date (without time of the day) to write to or read from a database.
struct SqlDate
{
    SQL_DATE_STRUCT sqlValue {};

    SqlDate() noexcept = default;
    SqlDate(SqlDate&&) noexcept = default;
    SqlDate& operator=(SqlDate&&) noexcept = default;
    SqlDate(SqlDate const&) noexcept = default;
    SqlDate& operator=(SqlDate const&) noexcept = default;
    ~SqlDate() noexcept = default;

    std::chrono::year_month_day value() const noexcept
    {
        return ConvertToNative(sqlValue);
    }

    bool operator==(SqlDate const& other) const noexcept
    {
        return sqlValue.year == other.sqlValue.year && sqlValue.month == other.sqlValue.month
               && sqlValue.day == other.sqlValue.day;
    }

    bool operator!=(SqlDate const& other) const noexcept
    {
        return !(*this == other);
    }

    SqlDate(std::chrono::year_month_day value) noexcept:
        sqlValue { SqlDate::ConvertToSqlValue(value) }
    {
    }

    SqlDate(std::chrono::year year, std::chrono::month month, std::chrono::day day) noexcept:
        SqlDate(std::chrono::year_month_day { year, month, day })
    {
    }

    static SqlDate Today() noexcept
    {
        return SqlDate { std::chrono::year_month_day {
            std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()),
        } };
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
    using native_type = std::chrono::hh_mm_ss<std::chrono::microseconds>;

#if defined(SQL_SS_TIME2)
    using sql_type = SQL_SS_TIME2_STRUCT;
#else
    using sql_type = SQL_TIME_STRUCT;
#endif

    sql_type sqlValue {};

    SqlTime() noexcept = default;
    SqlTime(SqlTime&&) noexcept = default;
    SqlTime& operator=(SqlTime&&) noexcept = default;
    SqlTime(SqlTime const&) noexcept = default;
    SqlTime& operator=(SqlTime const&) noexcept = default;
    ~SqlTime() noexcept = default;

    native_type value() const noexcept
    {
        return ConvertToNative(sqlValue);
    }

    bool operator==(SqlTime const& other) const noexcept
    {
        return value().to_duration().count() == other.value().to_duration().count();
    }

    bool operator!=(SqlTime const& other) const noexcept
    {
        return !(*this == other);
    }

    SqlTime(native_type value) noexcept:
        sqlValue { SqlTime::ConvertToSqlValue(value) }
    {
    }

    SqlTime(std::chrono::hours hour,
            std::chrono::minutes minute,
            std::chrono::seconds second,
            std::chrono::microseconds micros = {}) noexcept:
        SqlTime(native_type { hour + minute + second + micros })
    {
    }

    static sql_type ConvertToSqlValue(native_type value) noexcept
    {
        return sql_type {
            .hour = (SQLUSMALLINT) value.hours().count(),
            .minute = (SQLUSMALLINT) value.minutes().count(),
            .second = (SQLUSMALLINT) value.seconds().count(),
#if defined(SQL_SS_TIME2)
            .fraction = (SQLUINTEGER) value.subseconds().count(),
#endif
        };
    }

    static native_type ConvertToNative(sql_type const& value) noexcept
    {
        // clang-format off
        return native_type { std::chrono::hours { (int) value.hour }
                             + std::chrono::minutes { (unsigned) value.minute }
                             + std::chrono::seconds { (unsigned) value.second }
#if defined(SQL_SS_TIME2)
                             + std::chrono::microseconds { value.fraction }
#endif

        };
        // clang-format on
    }
};

struct SqlDateTime
{
    using native_type = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

    static SqlDateTime Now() noexcept
    {
        return SqlDateTime { std::chrono::system_clock::now() };
    }

    SqlDateTime() noexcept = default;
    SqlDateTime(SqlDateTime&&) noexcept = default;
    SqlDateTime& operator=(SqlDateTime&&) noexcept = default;
    SqlDateTime(SqlDateTime const&) noexcept = default;
    SqlDateTime& operator=(SqlDateTime const& other) noexcept = default;
    ~SqlDateTime() noexcept = default;

    bool operator==(SqlDateTime const& other) const noexcept
    {
        return value() == other.value();
    }

    bool operator!=(SqlDateTime const& other) const noexcept
    {
        return !(*this == other);
    }

    SqlDateTime(std::chrono::year_month_day ymd, std::chrono::hh_mm_ss<std::chrono::nanoseconds> time) noexcept
    {
        sqlValue.year = (SQLSMALLINT) (int) ymd.year();
        sqlValue.month = (SQLUSMALLINT) (unsigned) ymd.month();
        sqlValue.day = (SQLUSMALLINT) (unsigned) ymd.day();
        sqlValue.hour = (SQLUSMALLINT) time.hours().count();
        sqlValue.minute = (SQLUSMALLINT) time.minutes().count();
        sqlValue.second = (SQLUSMALLINT) time.seconds().count();
        sqlValue.fraction = (SQLUINTEGER) (time.subseconds().count() / 100) * 100;
    }

    SqlDateTime(std::chrono::year year,
                std::chrono::month month,
                std::chrono::day day,
                std::chrono::hours hour,
                std::chrono::minutes minute,
                std::chrono::seconds second,
                std::chrono::nanoseconds nanosecond = std::chrono::nanoseconds { 0 }) noexcept
    {
        sqlValue.year = (SQLSMALLINT) (int) year;
        sqlValue.month = (SQLUSMALLINT) (unsigned) month;
        sqlValue.day = (SQLUSMALLINT) (unsigned) day;
        sqlValue.hour = (SQLUSMALLINT) hour.count();
        sqlValue.minute = (SQLUSMALLINT) minute.count();
        sqlValue.second = (SQLUSMALLINT) second.count();
        sqlValue.fraction = (SQLUINTEGER) (nanosecond.count() / 100) * 100;
    }

    SqlDateTime(std::chrono::system_clock::time_point value) noexcept:
        sqlValue { SqlDateTime::ConvertToSqlValue(value) }
    {
    }

    operator native_type() const noexcept
    {
        return value();
    }

    static SQL_TIMESTAMP_STRUCT ConvertToSqlValue(native_type value) noexcept
    {
        using namespace std::chrono;
        auto const totalDays = floor<days>(value);
        auto const ymd = year_month_day { totalDays };
        auto const hms = hh_mm_ss<nanoseconds> { floor<nanoseconds>(value - totalDays) };

        return SQL_TIMESTAMP_STRUCT {
            .year = (SQLSMALLINT) (int) ymd.year(),
            .month = (SQLUSMALLINT) (unsigned) ymd.month(),
            .day = (SQLUSMALLINT) (unsigned) ymd.day(),
            .hour = (SQLUSMALLINT) hms.hours().count(),
            .minute = (SQLUSMALLINT) hms.minutes().count(),
            .second = (SQLUSMALLINT) hms.seconds().count(),
            .fraction = (SQLUINTEGER) (hms.subseconds().count() / 100) * 100,
        };
    }

    static native_type ConvertToNative(SQL_TIMESTAMP_STRUCT const& time) noexcept
    {
        // clang-format off
        using namespace std::chrono;
        auto timepoint = sys_days(year_month_day(year(time.year), month(time.month), day(time.day)))
                       + hours(time.hour)
                       + minutes(time.minute)
                       + seconds(time.second)
                       + nanoseconds(time.fraction);
        return timepoint;
        // clang-format on
    }

    native_type value() const noexcept
    {
        return ConvertToNative(sqlValue);
    }

    SQL_TIMESTAMP_STRUCT sqlValue {};
};

// Helper struct to store a timestamp that should be automatically converted to/from a SQL_TIMESTAMP_STRUCT.
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
                                SqlText,
                                SqlDate,
                                SqlTime,
                                SqlDateTime>;

// Callback interface for SqlDataBinder to allow post-processing of output columns.
//
// This is needed because the SQLBindCol() function does not allow to specify a callback function to be called
// after the data has been fetched from the database. This is needed to trim strings to the correct size, for
// example.
class SqlDataBinderCallback
{
  public:
    virtual ~SqlDataBinderCallback() = default;

    virtual void PlanPostExecuteCallback(std::function<void()>&&) = 0;
    virtual void PlanPostProcessOutputColumn(std::function<void()>&&) = 0;
};

template <typename>
struct SqlDataBinder
{
    static_assert(false, "No SQL data binder available for this type.");
};

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
template <> struct SqlDataBinder<char>: SqlSimpleDataBinder<char, SQL_C_CHAR, SQL_CHAR> {};
template <> struct SqlDataBinder<int16_t>: SqlSimpleDataBinder<int16_t, SQL_C_SSHORT, SQL_SMALLINT> {};
template <> struct SqlDataBinder<uint16_t>: SqlSimpleDataBinder<uint16_t, SQL_C_USHORT, SQL_SMALLINT> {};
template <> struct SqlDataBinder<int32_t>: SqlSimpleDataBinder<int32_t, SQL_C_SLONG, SQL_INTEGER> {};
template <> struct SqlDataBinder<uint32_t>: SqlSimpleDataBinder<uint32_t, SQL_C_ULONG, SQL_INTEGER> {};
template <> struct SqlDataBinder<int64_t>: SqlSimpleDataBinder<int64_t, SQL_C_SBIGINT, SQL_BIGINT> {};
template <> struct SqlDataBinder<uint64_t>: SqlSimpleDataBinder<uint64_t, SQL_C_UBIGINT, SQL_BIGINT> {};
template <> struct SqlDataBinder<float>: SqlSimpleDataBinder<float, SQL_C_FLOAT, SQL_REAL> {};
template <> struct SqlDataBinder<double>: SqlSimpleDataBinder<double, SQL_C_DOUBLE, SQL_DOUBLE> {};
#if !defined(_WIN32) && !defined(__APPLE__)
template <> struct SqlDataBinder<long long>: SqlSimpleDataBinder<long long, SQL_C_SBIGINT, SQL_BIGINT> {};
template <> struct SqlDataBinder<unsigned long long>: SqlSimpleDataBinder<unsigned long long, SQL_C_UBIGINT, SQL_BIGINT> {};
#endif
#if defined(__APPLE__) // size_t is a different type on macOS
template <> struct SqlDataBinder<std::size_t>: SqlSimpleDataBinder<std::size_t, SQL_C_SBIGINT, SQL_BIGINT> {};
#endif
// clang-format on

// Default traits for output string parameters
// This needs to be implemented for each string type that should be used as output parameter via
// SqlDataBinder<>. An std::string specialization is provided below. Feel free to add more specializations for
// other string types, such as CString, etc.
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
    static void Clear(std::string* str) noexcept
    {
        str->clear();
    }

    static void Reserve(std::string* str, size_t capacity) noexcept
    {
        // std::string tries to defer the allocation as long as possible.
        // So we first tell std::string how much to reserve and then resize it to the *actually* reserved
        // size.
        str->reserve(capacity);
        str->resize(str->capacity());
    }

    static void Resize(std::string* str, SQLLEN indicator) noexcept
    {
        if (indicator > 0)
            str->resize(indicator);
    }
};

template <>
struct SqlOutputStringTraits<SqlText>
{
    using Traits = SqlOutputStringTraits<typename SqlText::value_type>;

    // clang-format off
    static char const* Data(SqlText const* str) noexcept { return Traits::Data(&str->value); }
    static char* Data(SqlText* str) noexcept { return Traits::Data(&str->value); }
    static SQLULEN Size(SqlText const* str) noexcept { return Traits::Size(&str->value); }
    static void Clear(SqlText* str) noexcept { Traits::Clear(&str->value); }
    static void Reserve(SqlText* str, size_t capacity) noexcept { Traits::Reserve(&str->value, capacity); }
    static void Resize(SqlText* str, SQLLEN indicator) noexcept { Traits::Resize(&str->value, indicator); }
    // clang-format on
};

template <std::size_t N, typename T, SqlStringPostRetrieveOperation PostOp>
struct SqlOutputStringTraits<SqlFixedString<N, T, PostOp>>
{
    using ValueType = SqlFixedString<N, T, PostOp>;
    // clang-format off
    static char const* Data(ValueType const* str) noexcept { return str->data(); }
    static char* Data(ValueType* str) noexcept { return str->data(); }
    static SQLULEN Size(ValueType const* str) noexcept { return str->size(); }
    static void Clear(ValueType* str) noexcept { str->clear(); }
    static void Reserve(ValueType* str, size_t capacity) noexcept { str->reserve(capacity); }
    static void Resize(ValueType* str, SQLLEN indicator) noexcept { str->resize(indicator); }
    // clang-format on
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
        // Ensure we're having sufficient space to store the worst-case scenario of bytes in this column
        SQLULEN columnSize {};
        auto const describeResult = SQLDescribeCol(stmt,
                                                   column,
                                                   nullptr /*colName*/,
                                                   0 /*sizeof(colName)*/,
                                                   nullptr /*&colNameLen*/,
                                                   nullptr /*&dataType*/,
                                                   &columnSize,
                                                   nullptr /*&decimalDigits*/,
                                                   nullptr /*&nullable*/);
        if (!SQL_SUCCEEDED(describeResult))
            return describeResult;

        StringTraits::Reserve(result,
                              columnSize); // Must be called now, because otherwise std::string won't do anything

        cb.PlanPostProcessOutputColumn([indicator, result]() {
            // Now resize the string to the actual length of the data
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
                    if (*indicator == SQL_NO_TOTAL)
                    {
                        // We have a truncation and the server does not know how much data is left.
                        writeIndex += bufferSize - 1;
                        StringTraits::Resize(result, 2 * writeIndex + 1);
                    }
                    else if (std::cmp_greater_equal(*indicator, bufferSize))
                    {
                        // We have a truncation and the server knows how much data is left.
                        writeIndex += bufferSize - 1;
                        StringTraits::Resize(result, writeIndex + *indicator);
                    }
                    else
                    {
                        // We have no truncation and the server knows how much data is left.
                        StringTraits::Resize(result, writeIndex + *indicator - 1);
                        return SQL_SUCCESS;
                    }
                    break;
                }
                default:
                    return rv;
            }
        }
    }
};

template <std::size_t N, typename T, SqlStringPostRetrieveOperation PostOp>
struct SqlDataBinder<SqlFixedString<N, T, PostOp>>
{
    using ValueType = SqlFixedString<N, T, PostOp>;
    using StringTraits = SqlOutputStringTraits<ValueType>;

    static void TrimRight(ValueType* boundOutputString, SQLLEN indicator) noexcept
    {
        size_t n = indicator;
        while (n > 0 && std::isspace((*boundOutputString)[n - 1]))
            --n;
        boundOutputString->setsize(n);
    }

    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, ValueType const& value) noexcept
    {
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_CHAR,
                                SQL_VARCHAR,
                                value.size(),
                                0,
                                (SQLPOINTER) value.c_str(), // Ensure Null-termination.
                                sizeof(value),
                                nullptr);
    }

    static SQLRETURN OutputColumn(
        SQLHSTMT stmt, SQLUSMALLINT column, ValueType* result, SQLLEN* indicator, SqlDataBinderCallback& cb) noexcept
    {
        if constexpr (PostOp == SqlStringPostRetrieveOperation::TRIM_RIGHT)
        {
            ValueType* boundOutputString = result;
            cb.PlanPostProcessOutputColumn([indicator, boundOutputString]() {
                // NB: If the indicator is greater than the buffer size, we have a truncation.
                auto const len =
                    std::cmp_greater_equal(*indicator, N + 1) || *indicator == SQL_NO_TOTAL ? N : *indicator;
                if constexpr (PostOp == SqlStringPostRetrieveOperation::TRIM_RIGHT)
                    TrimRight(boundOutputString, len);
                else
                    boundOutputString->setsize(len);
            });
        }
        return SQLBindCol(
            stmt, column, SQL_C_CHAR, (SQLPOINTER) result->data(), (SQLLEN) result->capacity(), indicator);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, ValueType* result, SQLLEN* indicator) noexcept
    {
        *indicator = 0;
        SQLRETURN rv = SQLGetData(stmt, column, SQL_C_CHAR, result->data(), result->capacity(), indicator);
        switch (rv)
        {
            case SQL_SUCCESS:
            case SQL_NO_DATA:
                // last successive call
                result->setsize(*indicator);
                if constexpr (PostOp == SqlStringPostRetrieveOperation::TRIM_RIGHT)
                    TrimRight(result, *indicator);
                return SQL_SUCCESS;
            case SQL_SUCCESS_WITH_INFO: {
                // more data pending
                // Truncating. This case should never happen.
                result->setsize(result->capacity() - 1);
                if constexpr (PostOp == SqlStringPostRetrieveOperation::TRIM_RIGHT)
                    TrimRight(result, *indicator);
                return SQL_SUCCESS;
            }
            default:
                return rv;
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

    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, SqlTrimmedString const& value) noexcept
    {
        return SqlDataBinder<InnerStringType>::InputParameter(stmt, column, value.value);
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

    static SQLRETURN OutputColumn(SQLHSTMT stmt,
                                  SQLUSMALLINT column,
                                  SqlDate* result,
                                  SQLLEN* /*indicator*/,
                                  SqlDataBinderCallback& /*cb*/) noexcept
    {
        // TODO: handle indicator to check for NULL values
        return SQLBindCol(stmt, column, SQL_C_TYPE_DATE, &result->sqlValue, sizeof(result->sqlValue), nullptr);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, SqlDate* result, SQLLEN* indicator) noexcept
    {
        return SQLGetData(stmt, column, SQL_C_TYPE_DATE, &result->sqlValue, sizeof(result->sqlValue), indicator);
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

    static SQLRETURN OutputColumn(SQLHSTMT stmt,
                                  SQLUSMALLINT column,
                                  SqlTime* result,
                                  SQLLEN* /*indicator*/,
                                  SqlDataBinderCallback& /*cb*/) noexcept
    {
        // TODO: handle indicator to check for NULL values
        return SQLBindCol(stmt, column, SQL_C_TYPE_TIME, &result->sqlValue, sizeof(result->sqlValue), nullptr);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, SqlTime* result, SQLLEN* indicator) noexcept
    {
        return SQLGetData(stmt, column, SQL_C_TYPE_TIME, &result->sqlValue, sizeof(result->sqlValue), indicator);
    }
};

template <>
struct SqlDataBinder<SqlDateTime::native_type>
{
    static SQLRETURN GetColumn(SQLHSTMT stmt,
                               SQLUSMALLINT column,
                               SqlDateTime::native_type* result,
                               SQLLEN* indicator) noexcept
    {
        SQL_TIMESTAMP_STRUCT sqlValue {};
        auto const rc = SQLGetData(stmt, column, SQL_C_TYPE_TIMESTAMP, &sqlValue, sizeof(sqlValue), indicator);
        if (SQL_SUCCEEDED(rc))
            *result = SqlDateTime::ConvertToNative(sqlValue);
        return rc;
    }
};

template <>
struct SqlDataBinder<SqlDateTime>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, SqlDateTime const& value) noexcept
    {
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_TIMESTAMP,
                                SQL_TYPE_TIMESTAMP,
                                27,
                                7,
                                (SQLPOINTER) &value.sqlValue,
                                sizeof(value),
                                nullptr);
    }

    static SQLRETURN OutputColumn(SQLHSTMT stmt,
                                  SQLUSMALLINT column,
                                  SqlDateTime* result,
                                  SQLLEN* indicator,
                                  SqlDataBinderCallback& /*cb*/) noexcept
    {
        // TODO: handle indicator to check for NULL values
        *indicator = sizeof(result->sqlValue);
        return SQLBindCol(stmt, column, SQL_C_TYPE_TIMESTAMP, &result->sqlValue, 0, indicator);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, SqlDateTime* result, SQLLEN* indicator) noexcept
    {
        return SQLGetData(stmt, column, SQL_C_TYPE_TIMESTAMP, &result->sqlValue, sizeof(result->sqlValue), indicator);
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
            case SQL_DATE:
                SqlLogger::GetLogger().OnWarning(
                    std::format("SQL_DATE is from ODBC 2. SQL_TYPE_DATE should have been received instead."));
                [[fallthrough]];
            case SQL_TYPE_DATE:
                result->emplace<SqlDate>();
                returnCode = SqlDataBinder<SqlDate>::GetColumn(stmt, column, &std::get<SqlDate>(*result), indicator);
                break;
            case SQL_TIME:
                SqlLogger::GetLogger().OnWarning(
                    std::format("SQL_TIME is from ODBC 2. SQL_TYPE_TIME should have been received instead."));
                [[fallthrough]];
            case SQL_TYPE_TIME:
            case SQL_SS_TIME2:
                result->emplace<SqlTime>();
                returnCode = SqlDataBinder<SqlTime>::GetColumn(stmt, column, &std::get<SqlTime>(*result), indicator);
                break;
            case SQL_TYPE_TIMESTAMP:
                result->emplace<SqlDateTime>();
                returnCode =
                    SqlDataBinder<SqlDateTime>::GetColumn(stmt, column, &std::get<SqlDateTime>(*result), indicator);
                break;
            case SQL_TYPE_NULL:
            case SQL_DECIMAL:
            case SQL_NUMERIC:
            case SQL_GUID:
                // TODO: Get them implemented on demand
                [[fallthrough]];
            default:
                std::println("Unsupported column type: {}", columnType);
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
