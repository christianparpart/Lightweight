// SPDX-License-Identifier: Apache-2.0
#include "AbstractField.hpp"
#include "AbstractRecord.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <string>

namespace Model
{

std::string AbstractRecord::Inspect() const noexcept
{
    if (!m_data)
        return "UNAVAILABLE";

    detail::StringBuilder result;

    // Reserve enough space for the output string (This is merely a guess, but it's better than nothing)
    result.output.reserve(TableName().size() + AllFields().size() * 32);

    result << "#<" << TableName() << ": id=" << Id().value;
    for (auto const* field: AllFields())
        result << ", " << field->Name() << "=" << field->InspectValue();
    result << ">";

    return *result;
}

void AbstractRecord::SetModified(bool value) noexcept
{
    for (AbstractField* field: m_data->fields)
        field->SetModified(value);
}

bool AbstractRecord::IsModified() const noexcept
{
    return std::ranges::any_of(m_data->fields, [](AbstractField* field) { return field->IsModified(); });
}

AbstractRecord::FieldList AbstractRecord::GetModifiedFields() const noexcept
{
    FieldList result;
    std::ranges::copy_if(m_data->fields, std::back_inserter(result), [](auto* field) { return field->IsModified(); });
    return result;
}

void AbstractRecord::SortFieldsByIndex() noexcept
{
    std::sort(m_data->fields.begin(), m_data->fields.end(), [](auto a, auto b) { return a->Index() < b->Index(); });
}

std::vector<std::string_view> AbstractRecord::AllFieldNames() const
{
    std::vector<std::string_view> columnNames;
    columnNames.resize(1 + m_data->fields.size());
    columnNames[0] = PrimaryKeyName();
    for (auto const* field: AllFields())
        columnNames[field->Index() - 1] = field->Name().name;
    return columnNames;
}

} // namespace Model
