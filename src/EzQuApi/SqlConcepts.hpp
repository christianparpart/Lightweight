#pragma once

#include <concepts>

template <typename T>
concept StdStringLike = requires(T const& t, T& u)
{
    { t.data() } -> std::convertible_to<char const*>;
    { t.size() } -> std::convertible_to<std::size_t>;
    { u.clear() };
    { u.append(std::declval<char const*>(), std::declval<std::size_t>()) };
};

template <typename T>
concept MFCStringLike = requires(T const& t)
{
    { t.GetLength() } -> std::convertible_to<int>;
    { t.GetString() } -> std::convertible_to<char const*>;
};

template <typename T>
concept RNStringLike = requires(T const& t)
{
    { t.Length() } -> std::convertible_to<int>;
    { t.GetString() } -> std::convertible_to<char const*>;
};

