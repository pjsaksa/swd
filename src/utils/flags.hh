/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#pragma once

#include <cstdint>

namespace utils
{
    template< typename Flag >
    class FlagsT {
        using bits_t = uint32_t;
        using flag_t = Flag;

        //

        bits_t bits;

    public:
        FlagsT() : bits(0) {}
        FlagsT(flag_t f) : bits(1u << static_cast<bits_t>( f )) {}

        bool flag(flag_t f) const
        {
            return bits & (1u << static_cast<bits_t>( f ));
        }

    private:
        explicit FlagsT(bits_t b)
            : bits(b) {}

        //

        friend FlagsT<Flag> operator| (FlagsT<Flag> l, flag_t r)
        {
            return FlagsT<Flag>( l.bits | (1u << static_cast<bits_t>( r )) );
        }

        friend FlagsT<Flag> operator| (flag_t l, flag_t r)
        {
            return FlagsT<Flag>( (1u << static_cast<bits_t>( l )) | (1u << static_cast<bits_t>( r )) );
        }

        friend FlagsT<Flag>& operator|= (FlagsT<Flag>& l, flag_t r)
        {
            l.bits |= (1u << static_cast<bits_t>( r ));
            return l;
        }
    };
}
