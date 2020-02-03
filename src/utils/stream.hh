/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#pragma once

#include <ostream>
#include <streambuf>

namespace utils
{
    class restore_ios {
    public:
        restore_ios(std::ios &io)
            : m_io(io),
              m_flags(m_io.flags()),
              m_fill(m_io.fill()),
              m_width(m_io.width()),
              m_precision(m_io.precision())
        {
        }

        ~restore_ios(void)
        {
            m_io.flags(m_flags);
            m_io.fill(m_fill);
            m_io.width(m_width);
            m_io.precision(m_precision);
        }

    private:
        std::ios &m_io;

        std::ios::fmtflags m_flags;
        std::ios::char_type m_fill;
        std::streamsize m_width;
        std::streamsize m_precision;
    };

    // ------------------------------------------------------------

    class escape_bash : public std::ostream {
        class escape_bash_streambuf : public std::streambuf {
        public:
            escape_bash_streambuf(std::ostream&);

            static bool dangerous(char_type);

        protected:
            virtual std::streamsize xsputn(const char_type*, std::streamsize);
            virtual int_type overflow(int_type);

        private:
            std::ostream& m_out;
        };

        //

        escape_bash_streambuf m_buf;

    public:
        escape_bash(std::ostream&);
    };
}
