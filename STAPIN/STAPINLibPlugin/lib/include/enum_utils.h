/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <type_traits>

namespace pinrt
{
    template< typename E > inline constexpr bool is_enum_flag = false;

#define MAKE_ENUM_FLAG(E) template<> inline constexpr bool ::pinrt::is_enum_flag< E > = true

    template< typename E > inline ::std::enable_if_t< ::std::is_enum< E >::v && is_enum_flag< E >, E > operator~(E r)
    {
        return E(~unsigned(r));
    }
    template< typename E > inline ::std::enable_if_t< ::std::is_enum< E >::v && is_enum_flag< E >, E > operator&(E l, E r)
    {
        return E(unsigned(l) & unsigned(r));
    }
    template< typename E > inline ::std::enable_if_t< ::std::is_enum< E >::v && is_enum_flag< E >, E > operator|(E l, E r)
    {
        return E(unsigned(l) | unsigned(r));
    }
    template< typename E > inline ::std::enable_if_t< ::std::is_enum< E >::v && is_enum_flag< E >, E > operator^(E l, E r)
    {
        return E(unsigned(l) ^ unsigned(r));
    }
    template< typename E > inline ::std::enable_if_t< ::std::is_enum< E >::v && is_enum_flag< E >, E >& operator&=(E& l, E r)
    {
        return l = l & r;
    }
    template< typename E > inline ::std::enable_if_t< ::std::is_enum< E >::v && is_enum_flag< E >, E >& operator|=(E& l, E r)
    {
        return l = l | r;
    }
    template< typename E > inline ::std::enable_if_t< ::std::is_enum< E >::v && is_enum_flag< E >, E >& operator^=(E& l, E r)
    {
        return l = l ^ r;
    }

}; // namespace pinrt
