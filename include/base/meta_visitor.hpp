/**
 * Type traits for static visitor pattern support.
 * Author: Marius Mikucionis <marius@cs.aau.dk>
 */
#ifndef VISITOR_HPP
#define VISITOR_HPP

#include <type_traits>

namespace visitor
{
    /* Primary declaration for visitor trait (specialize to enable support): */
    template <typename Visitor>
    struct is_visitor : std::false_type
    {};

    /* Primary declaration for types accepting visitor trait (specialize to enable
     * support): */
    template <typename Data, typename Visitor>
    struct accepts_reader : std::false_type
    {};

    template <typename Data, typename Visitor>
    constexpr auto accepts_reader_v = accepts_reader<Data, Visitor>::value;

    template <typename Data, typename Visitor>
    struct accepts_writer : std::false_type
    {};

    template <typename Data, typename Visitor>
    constexpr auto accepts_writer_v = accepts_writer<Data, Visitor>::value;

    /* Helper output operator for any visitor visiting any const data: */
    template <typename V, typename D,
              typename = std::enable_if_t<is_visitor<std::remove_reference_t<V>>::value>>
    V& operator<<(V&& v, const D& d)
    {
        v.visit(d);
        return v;
    }
}  // namespace visitor

#endif /* VISITOR_HPP */
