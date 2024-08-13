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
        m_tableName { tableName },
        m_primaryKeyName { primaryKey },
        m_id { id }
    {
    }

    AbstractRecord() = delete;
    AbstractRecord(AbstractRecord const&) = default;
    AbstractRecord(AbstractRecord&& other) = delete;
    AbstractRecord& operator=(AbstractRecord const&) = delete;
    AbstractRecord& operator=(AbstractRecord&&) = delete;
    ~AbstractRecord() = default;

    // clang-format off
    std::string_view TableName() const noexcept { return m_tableName; }
    std::string_view PrimaryKeyName() const noexcept { return m_primaryKeyName; }
    RecordId Id() const noexcept { return m_id; }

    void RegisterField(AbstractField& field) noexcept { m_fields.push_back(&field); }

    void UnregisterField(AbstractField const& field) noexcept
    {
        // remove field by rotating it to the end and then popping it
        auto it = std::ranges::find(m_fields, &field);
        if (it != m_fields.end())
        {
            std::rotate(it, std::next(it), m_fields.end());
            m_fields.pop_back();
        }
    }

    void RegisterRelation(Relation& relation) noexcept { m_relations.push_back(&relation); }

    AbstractField const& GetField(SqlColumnIndex index) const noexcept { return *m_fields[index.value]; }
    AbstractField& GetField(SqlColumnIndex index) noexcept { return *m_fields[index.value]; }
    // clang-format on

    void SetModified(bool value) noexcept;

    bool IsModified() const noexcept;

    void SortFieldsByIndex() noexcept;

    using FieldList = std::vector<AbstractField*>;

    FieldList GetModifiedFields() const noexcept;

  protected:
    std::string_view m_tableName;      // Should be const, but we want to allow move semantics
    std::string_view m_primaryKeyName; // Should be const, but we want to allow move semantics
    RecordId m_id {};

    bool m_modified = false;
    FieldList m_fields;
    std::vector<Relation*> m_relations;
};

} // namespace Model
