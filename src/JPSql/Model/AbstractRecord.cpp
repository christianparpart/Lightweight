#include "AbstractField.hpp"
#include "AbstractRecord.hpp"

#include <algorithm>
#include <ranges>

namespace Model
{

void AbstractRecord::SetModified(bool value) noexcept
{
    for (auto* field: m_fields)
        field->SetModified(value);
}

bool AbstractRecord::IsModified() const noexcept
{
    return std::ranges::any_of(m_fields, [](AbstractField* field) { return field->IsModified(); });
}

AbstractRecord::FieldList AbstractRecord::GetModifiedFields() const noexcept
{
    FieldList result;
    std::ranges::copy_if(m_fields, std::back_inserter(result), [](auto* field) { return field->IsModified(); });
    return result;
}

void AbstractRecord::SortFieldsByIndex() noexcept
{
    std::sort(m_fields.begin(), m_fields.end(), [](auto a, auto b) { return a->Index() < b->Index(); });
}

} // namespace Model
