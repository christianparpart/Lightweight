// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../DataBinder/Core.hpp"
#include "../DataBinder/SqlNullValue.hpp"
#include "../SqlStatement.hpp"
#include "BelongsTo.hpp"
#include "Field.hpp"

#include <reflection-cpp/reflection.hpp>

#include <compare>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

// This HasMany<OtherRecord> represents a simple one-to-many relationship between two records.
//
// The HasMany<OtherRecord> is a member of the "one" side of the relationship.
//
// This implemenation of `HasMany<OtherRecord>` must have only one `BelongsTo` member
// that points back to this "one" side.
template <typename OtherRecord>
class HasMany
{
  public:
    using ReferencedRecord = OtherRecord;
    using ReferencedRecordList = std::vector<std::shared_ptr<OtherRecord>>;

    // Record type of the "many" side of the relationship.
    using value_type = OtherRecord;
    using iterator = typename ReferencedRecordList::iterator;
    using const_iterator = typename ReferencedRecordList::const_iterator;

    // Retrieves the list of loaded records.
    [[nodiscard]] ReferencedRecordList const& All() const noexcept;

    // Retrieves the list of records as mutable reference.
    [[nodiscard]] ReferencedRecordList& All() noexcept;

    template <typename Callable>
    void Each(Callable const& callable);

    // Emplaces the given list of records.
    ReferencedRecordList& Emplace(ReferencedRecordList&& records) noexcept;

    [[nodiscard]] std::size_t Count() const noexcept;
    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] OtherRecord const& At(std::size_t index) const;
    [[nodiscard]] OtherRecord& At(std::size_t index);
    [[nodiscard]] OtherRecord const& operator[](std::size_t index) const;
    [[nodiscard]] OtherRecord& operator[](std::size_t index);

    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;

    constexpr std::weak_ordering operator<=>(HasMany<OtherRecord> const& other) const noexcept = default;

    constexpr bool operator==(HasMany<OtherRecord> const& other) const noexcept = default;
    constexpr bool operator!=(HasMany<OtherRecord> const& other) const noexcept = default;

    struct Loader
    {
        std::function<size_t()> count {};
        std::function<void()> all {};
        std::function<void(std::function<void(ReferencedRecord const&)>)> each {};
    };

    void SetAutoLoader(Loader loader) noexcept;

  private:
    void RequireLoaded();

    Loader _loader;
    std::optional<ReferencedRecordList> _records;
    std::optional<size_t> _count;
};

template <typename T>
constexpr bool IsHasMany = IsSpecializationOf<HasMany, T>;

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE void HasMany<OtherRecord>::SetAutoLoader(Loader loader) noexcept
{
    _loader = std::move(loader);
}

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE void HasMany<OtherRecord>::RequireLoaded()
{
    if (!_records)
        _loader.all();
}

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE HasMany<OtherRecord>::ReferencedRecordList& HasMany<OtherRecord>::Emplace(
    ReferencedRecordList&& records) noexcept
{
    _records = { std::move(records) };
    return *_records;
}

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE HasMany<OtherRecord>::ReferencedRecordList& HasMany<OtherRecord>::All() noexcept
{
    RequireLoaded();
    return *_records;
}

template <typename OtherRecord>
template <typename Callable>
void HasMany<OtherRecord>::Each(Callable const& callable)
{
    if (!_records && _loader.each)
    {
        _loader.each(callable);
        return;
    }

    for (auto const& record: All())
        callable(*record);
}

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE HasMany<OtherRecord>::ReferencedRecordList const& HasMany<OtherRecord>::All()
    const noexcept
{
    RequireLoaded();
    return *_records;
}

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE std::size_t HasMany<OtherRecord>::Count() const noexcept
{
    if (_records)
        return _records->size();

    if (!_count)
        const_cast<HasMany<OtherRecord>*>(this)->_count = _loader.count();

    return *_count;
}

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE bool HasMany<OtherRecord>::IsEmpty() const noexcept
{
    return Count() == 0;
}

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE OtherRecord const& HasMany<OtherRecord>::At(std::size_t index) const
{
    RequireLoaded();
    return *_records->at(index);
}

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE OtherRecord& HasMany<OtherRecord>::At(std::size_t index)
{
    RequireLoaded();
    return *_records->at(index);
}

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE OtherRecord const& HasMany<OtherRecord>::operator[](std::size_t index) const
{
    RequireLoaded();
    return *(*_records)[index];
}

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE OtherRecord& HasMany<OtherRecord>::operator[](std::size_t index)
{
    RequireLoaded();
    return *(*_records)[index];
}

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE HasMany<OtherRecord>::iterator HasMany<OtherRecord>::begin() noexcept
{
    RequireLoaded();
    return _records->begin();
}

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE HasMany<OtherRecord>::iterator HasMany<OtherRecord>::end() noexcept
{
    RequireLoaded();
    return _records->end();
}

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE HasMany<OtherRecord>::const_iterator HasMany<OtherRecord>::begin() const noexcept
{
    RequireLoaded();
    return _records->begin();
}

template <typename OtherRecord>
inline LIGHTWEIGHT_FORCE_INLINE HasMany<OtherRecord>::const_iterator HasMany<OtherRecord>::end() const noexcept
{
    RequireLoaded();
    return _records->end();
}
