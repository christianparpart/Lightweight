// SPDX-License-Identifier: MIT
#pragma once

#include "Detail.hpp"
#include "Field.hpp"
#include "Logger.hpp"
#include "RecordId.hpp"

#include <Lightweight/SqlComposedQuery.hpp>
#include <Lightweight/SqlConnection.hpp>
#include <Lightweight/SqlDataBinder.hpp>
#include <Lightweight/SqlStatement.hpp>

#include <reflection-cpp/reflection.hpp>

namespace DataMapper
{

template <typename Record>
std::string CreateTableString(SqlServerType serverType);

template <typename FirstRecord, typename... MoreRecords>
std::string CreateTablesString(SqlServerType serverType);

// Creates the table for the given record type.
template <typename Record>
void CreateTable();

template <typename FirstRecord, typename... MoreRecords>
void CreateTables();

// Creates a new record in the database.
template <typename Record>
void Create(Record& record);

template <typename Record>
std::optional<Record> Load(RecordId id);

template <typename Record>
void Reload(Record& record);

template <typename Record, typename T>
bool Load(Record& record, std::string_view const& columnName, T const& value);

template <typename Record>
void Update(Record& record);

template <typename Record>
void Save(Record& record);

template <typename Record>
void Destroy(Record const& record);

template <typename Record>
std::size_t Count();

template <typename Record>
std::vector<Record> All();

template <typename Record>
std::optional<Record> Find(RecordId id);

template <typename Record, typename ColumnName, typename T>
std::optional<Record> FindBy(ColumnName const& columnName, T const& value);

} // namespace DataMapper

// ------------------------------------------------------------------------------------------------

template <typename Record>
std::string DataMapper::CreateTableString(SqlServerType serverType)
{
    SqlTraits const& traits = GetSqlTraits(serverType);

    auto const defaultRecord = Record {};
    auto const defaultRecordTuple = Reflection::ToTuple(defaultRecord);

    std::vector<std::string> columnsSql;
    columnsSql.resize(Reflection::CountMembers<Record>);

    Reflection::template_for<0, Reflection::CountMembers<Record>>([&]<auto I>() {
        using FieldType = std::remove_cvref_t<decltype(std::get<I>(defaultRecordTuple))>;
        static_assert(FieldType::TableColumnIndex > 0);
        static_assert(FieldType::TableColumnIndex <= Reflection::CountMembers<Record>);

        auto constexpr fieldName = FieldType::ColumnName;

        if (fieldName == Record::PrimaryKey)
            return;

        detail::StringBuilder sql;
        sql << fieldName;
        sql << " " << traits.ColumnTypeName(FieldType::Type);

        if constexpr (FieldType::Size)
            sql << "(" << FieldType::Size << ")";

        if constexpr (FieldType::Requirement == FieldValueRequirement::NOT_NULL)
            sql << " NOT NULL";
        else if constexpr (FieldType::Requirement == FieldValueRequirement::NULLABLE)
            sql << " NULL";

        columnsSql[FieldType::TableColumnIndex - 1] = sql.output;
    });

    detail::StringBuilder sql;
    sql << "CREATE TABLE " << Record::TableName << " (\n";
    sql << "    " << Record::PrimaryKey << " " << traits.PrimaryKeyAutoIncrement << ",\n";

    for (auto const&& [i, columnSql]: columnsSql | std::views::enumerate)
    {
        if (columnSql.empty())
            // i.e. primary key
            continue;
        sql << "    ";
        sql << columnSql;
        if (static_cast<size_t>(i + 1) < columnsSql.size())
            sql << ",";
        sql << '\n';
    }

    sql << ");\n";

    return sql.output;
}

template <typename FirstRecord, typename... MoreRecords>
std::string DataMapper::CreateTablesString(SqlServerType serverType)
{
    detail::StringBuilder sql;
    sql << CreateTableString<FirstRecord>(serverType) << (CreateTableString<MoreRecords>(serverType) << ...);
    return sql.output;
}

template <typename Record>
void DataMapper::CreateTable()
{
    auto stmt = SqlStatement {};
    auto const sqlQueryString = CreateTableString<Record>(stmt.Connection().ServerType());
    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});
    stmt.ExecuteDirect(sqlQueryString);
}

template <typename FirstRecord, typename... MoreRecords>
void DataMapper::CreateTables()
{
    CreateTable<FirstRecord>();
    (CreateTable<MoreRecords>(), ...);
}

template <typename Record>
void DataMapper::Save(Record& record)
{
    std::printf("TODO(pr) Save: %s\n", Reflection::Inspect(record).c_str()); // TODO(pr)
}
