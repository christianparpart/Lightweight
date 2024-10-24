// SPDX-License-Identifier: Apache-2.0

#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #include <Windows.h>
#endif

#include "../SqlTraits.hpp"

#include <concepts>
#include <functional>

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

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

// SqlDataTraits is a helper struct to provide information about the SQL data type.
//
// A template specialization must provide the following members:
// - Size: The size of the data type in bytes (unsigned).
// - Type: The SQL data type (SqlColumnType).
template <typename T>
struct SqlDataTraits
{
    static_assert(false, "No SQL data traits available for this type.");
};

// Default traits for output string parameters
// This needs to be implemented for each string type that should be used as output parameter via
// SqlDataBinder<>. An std::string specialization is provided below. Feel free to add more specializations for
// other string types, such as CString, etc.
template <typename>
struct SqlCommonStringBinder;

// -----------------------------------------------------------------------------------------------

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

// clang-format off
template <typename StringType, typename CharType>
concept SqlBasicStringBinderConcept = requires(StringType* str) {
    { SqlCommonStringBinder<StringType>::Data(str) } -> std::same_as<CharType*>;
    { SqlCommonStringBinder<StringType>::Size(str) } -> std::same_as<SQLULEN>;
    { SqlCommonStringBinder<StringType>::Reserve(str, size_t {}) } -> std::same_as<void>;
    { SqlCommonStringBinder<StringType>::Resize(str, SQLLEN {}) } -> std::same_as<void>;
    { SqlCommonStringBinder<StringType>::Clear(str) } -> std::same_as<void>;
};

template <typename StringType>
concept SqlCommonStringBinderConcept = SqlBasicStringBinderConcept<StringType, char>;

template <typename StringType>
concept SqlCommonWideStringBinderConcept = SqlBasicStringBinderConcept<StringType, wchar_t>
                                        || SqlBasicStringBinderConcept<StringType, char16_t>
                                        || SqlBasicStringBinderConcept<StringType, char32_t>;

// clang-format on
