#pragma once

#include "RecordId.hpp"

#include <iterator>
#include <ranges>
#include <string_view>
#include <vector>

namespace Model
{

class AbstractField;
class Relation;

struct SqlColumnIndex
{
    size_t value;
};

// Base class for every SqlModel<T>.
struct AbstractRecord
{
  public:
    AbstractRecord(std::string_view tableName, std::string_view primaryKey, RecordId id):
        m_data { std::make_unique<Data>(tableName, primaryKey, id) }
    {
    }

    AbstractRecord() = delete;
    AbstractRecord(AbstractRecord const&) = delete;
    AbstractRecord(AbstractRecord&& other) noexcept:
        AbstractRecord { other.m_data->tableName, other.m_data->primaryKeyName, other.m_data->id }
    {
    }

    AbstractRecord& operator=(AbstractRecord const&) = delete;
    AbstractRecord& operator=(AbstractRecord&&) = delete;
    ~AbstractRecord() = default;

    // clang-format off
    std::string_view TableName() const noexcept { return m_data->tableName; }
    std::string_view PrimaryKeyName() const noexcept { return m_data->primaryKeyName; }
    RecordId Id() const noexcept { return m_data->id; }

    void RegisterField(AbstractField& field) noexcept { m_data->fields.push_back(&field); }

    void UnregisterField(AbstractField const& field) noexcept
    {
        if (!m_data)
            return;
        // remove field by rotating it to the end and then popping it
        auto it = std::ranges::find(m_data->fields, &field);
        if (it != m_data->fields.end())
        {
            std::rotate(it, std::next(it), m_data->fields.end());
            m_data->fields.pop_back();
        }
    }

    void RegisterRelation(Relation& relation) noexcept { m_data->relations.push_back(&relation); }

    AbstractField const& GetField(SqlColumnIndex index) const noexcept { return *m_data->fields[index.value]; }
    AbstractField& GetField(SqlColumnIndex index) noexcept { return *m_data->fields[index.value]; }
    // clang-format on

    void SetModified(bool value) noexcept;

    bool IsModified() const noexcept;

    void SortFieldsByIndex() noexcept;

    using FieldList = std::vector<AbstractField*>;

    [[nodiscard]] FieldList GetModifiedFields() const noexcept;

    [[nodiscard]] FieldList const& AllFields() const noexcept
    {
        return m_data->fields;
    }

  protected:
    struct Data
    {
        std::string_view tableName;      // Should be const, but we want to allow move semantics
        std::string_view primaryKeyName; // Should be const, but we want to allow move semantics
        RecordId id {};

        bool modified = false;
        FieldList fields;
        std::vector<Relation*> relations;
    };
    std::unique_ptr<Data> m_data;
};

} // namespace Model
