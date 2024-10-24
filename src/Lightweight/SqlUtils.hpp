// SPDX-License-Identifier: Apache-2.0

#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #include <Windows.h>
#endif

#include "Api.hpp"
#include <optional>

namespace detail
{

template <class T>
struct LIGHTWEIGHT_API DefaultDeleter
{
    void operator()(T* object)
    {
        delete object;
    }
};

// We cannot use std::unique_ptr in Windows DLLs as struct/class members,
// because of how Windows handles DLLs and the potential of incompatibilities (See C4251)
template <class T, class Deleter = DefaultDeleter<T>>
class LIGHTWEIGHT_API UniquePtr
{
  public:
    explicit UniquePtr() noexcept = default;

    explicit UniquePtr(std::nullptr_t) noexcept:
        _value { nullptr }
    {
    }

    explicit UniquePtr(T* value, Deleter deleter):
        _value { value },
        _deleter { deleter }
    {
    }

    UniquePtr(UniquePtr const&) = delete;
    UniquePtr& operator=(UniquePtr const&) = delete;

    UniquePtr(UniquePtr&& source) noexcept:
        _value { source._value },
        _deleter { std::move(source._deleter) }
    {
    }

    UniquePtr& operator=(UniquePtr&& source) noexcept
    {
        _value = source._value;
        _deleter = std::move(source._deleter);
        return *this;
    }

    ~UniquePtr()
    {
        _deleter(_value);
    }

    T* release() noexcept
    {
        T* oldValue = _value;
        _value = nullptr;
        return oldValue;
    }

    void reset(T* newValue) noexcept
    {
        if (_value)
            delete _value;
        _value = newValue;
    }

    T* get() noexcept
    {
        return _value;
    }

    T const* get() const noexcept
    {
        return _value;
    }

    T& operator*() noexcept
    {
        return *_value;
    }

    T const& operator*() const noexcept
    {
        return *_value;
    }

    T& operator->() noexcept
    {
        return *_value;
    }

    T const& operator->() const noexcept
    {
        return *_value;
    }

    explicit operator bool() const noexcept
    {
        return _value != nullptr;
    }

  private:
    T* _value = nullptr;
    Deleter _deleter = Deleter {};
};

template <typename T, typename... Args>
UniquePtr<T> MakeUnique(Args&&... args)
{
    return UniquePtr<T>(new T(std::forward<Args>(args)...));
}

} // namespace detail
