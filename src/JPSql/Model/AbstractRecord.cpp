#include "AbstractField.hpp"
#include "AbstractRecord.hpp"

#include <algorithm>
#include <ranges>

namespace Model
{

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

} // namespace Model
