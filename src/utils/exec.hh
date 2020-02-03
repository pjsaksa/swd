/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#pragma once

#include <istream>
#include <ostream>
#include <string>

#include <ext/stdio_filebuf.h>

namespace utils
{
    class Exec {
    public:
        enum class Flag {
            ASync,
            DevNullRead,
            DevNullWrite,
            NoShell,
            RedirectErrToOut,
        };

        class Flags {
            using bits_t = uint32_t;

            //

            bits_t bits;

        public:
            Flags() : bits(0) {}
            Flags(Flag f) : bits(1 << static_cast<int>( f )) {}

            bool flag(Flag f) const
            {
                return bits & (1 << static_cast<int>( f ));
            }

        private:
            explicit Flags(bits_t b)
                : bits(b) {}

            //

            friend Exec::Flags operator| (Exec::Flags l, Exec::Flag r);
            friend Exec::Flags operator| (Exec::Flag l,  Exec::Flag r);
        };

        // -----

        Exec(const std::string& argv0, Flags flags);
        Exec(const std::string& argv0);
        ~Exec();

        bool flag(Flag f) const { return m_flags.flag(f); }

        std::istream& read() { return input_stream; }
        std::ostream& write() { return output_stream; }

        void close_read();
        void close_write();

        bool wait();

    private:
        Flags m_flags;
        pid_t m_pid;
        const std::pair<int, int> fds;
        __gnu_cxx::stdio_filebuf<char> input_buf;
        __gnu_cxx::stdio_filebuf<char> output_buf;
        std::istream input_stream;
        std::ostream output_stream;
    };

    // -----

    inline Exec::Flags operator| (Exec::Flags l, Exec::Flag r)
    {
        return Exec::Flags( l.bits | (1 << static_cast<int>( r )) );
    }

    inline Exec::Flags operator| (Exec::Flag l, Exec::Flag r)
    {
        return Exec::Flags( (1 << static_cast<int>( l )) | (1 << static_cast<int>( r )) );
    }
}
