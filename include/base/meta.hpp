// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4;
// indent-tabs-mode: nil; -*-
////////////////////////////////////////////////////////////////////
//
// Filename : meta.hpp (base)
//
// Provides type predicates for metaprogramming purposes.
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 2019, Uppsala University and Aalborg University.
// All right reserved.
//
///////////////////////////////////////////////////////////////////

#ifndef META_HPP
#define META_HPP

#include <iterator>
#include <memory>
#include <optional>
#include <type_traits>

namespace meta
{
    /** true if T is bool (or a reference to it) */
    template <typename T>
    struct is_bool : std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, bool>
    {};

    template <typename T>
    constexpr auto is_bool_v = is_bool<T>::value;

    /** true if T is a number or a reference to it */
    template <typename T>
    constexpr auto is_number_v = std::is_arithmetic<std::remove_reference_t<T>>();

    /** true if T is a character or a reference to it */
    template <typename T,
              typename C = std::remove_cv_t<std::remove_reference_t<T>>>  // meta-computation
                                                                          // step
    struct is_character
        : std::conditional<std::is_same<C, char>::value || std::is_same<C, wchar_t>::value,
                           std::true_type, std::false_type>::type
    {};

    template <typename T>  // short hand variable:
    constexpr auto is_character_v = is_character<T>::value;

    /** The element type of a container if T is a container */
    template <typename T>
    using element_type =
        typename std::iterator_traits<decltype(std::begin(std::declval<T&>()))>::value_type;

    /** true if T is a container or a reference to it */
    template <typename,         // primary "default" template with one argument
              typename = void>  // extra argument is used in SFINAE
    struct is_container : std::false_type
    {};

    template <typename T>  // specialization for the iterable types
    struct is_container<T, std::void_t<element_type<T>>> : std::true_type
    {};

    template <typename T>
    constexpr auto is_container_v = is_container<T>::value;

    /** helper meta-function to extract the iterator type from container */
    template <typename T>
    using iterator_type = decltype(std::begin(std::declval<T&>()));

    /** true if T can be treated as a string */
    template <typename T, typename = void>  // primary template
    struct is_string : std::false_type
    {};

    template <typename T>  // specialization for pointer types (like c-strings)
    struct is_string<T, std::enable_if<std::is_pointer<std::remove_reference_t<T>>::value>>
        : is_character<std::remove_pointer_t<std::remove_reference_t<T>>>
    {};

    template <typename T>  // specialization for containers (like std::string and
                           // std::array<char>)
    struct is_string<T, std::enable_if_t<is_container_v<T>>>
        : is_character<typename std::iterator_traits<iterator_type<T>>::value_type>
    {};

    template <typename T>
    constexpr auto is_string_v = is_string<T>::value;

    /** true if T is a tuple */
    template <typename T>
    struct is_tuple : std::false_type
    {};

    template <typename... T>
    struct is_tuple<std::tuple<T...>> : std::true_type
    {};

    template <typename T1, typename T2>
    struct is_tuple<std::pair<T1, T2>> : std::true_type
    {};

    template <typename T>
    struct is_optional : std::false_type
    {};

    template <typename T>
    struct is_optional<std::optional<T>> : std::true_type
    {};

    template <typename T>
    constexpr auto is_optional_v = is_optional<T>::value;

    template <typename T>
    struct is_shared_ptr : std::false_type
    {};

    template <typename T>
    struct is_shared_ptr<std::shared_ptr<T>> : std::true_type
    {};

    template <typename T>
    struct is_unique_ptr : std::false_type
    {};

    template <typename T>
    struct is_unique_ptr<std::unique_ptr<T>> : std::true_type
    {};

    template <typename T>
    struct is_smart_ptr : std::conditional<is_shared_ptr<T>::value || is_unique_ptr<T>::value,
                                           std::true_type, std::false_type>::type
    {};

    template <typename T>
    constexpr auto is_smart_ptr_v = is_smart_ptr<T>::value;

    /** undefined struct to expose the type T */
    template <typename T>
    struct TD;
}  // namespace meta
#endif /* META_HPP */
