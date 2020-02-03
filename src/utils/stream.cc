/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "stream.hh"

utils::escape_bash::escape_bash_streambuf::escape_bash_streambuf(std::ostream& o)
    : m_out(o)
{
}

bool utils::escape_bash::escape_bash_streambuf::dangerous(char_type ch)
{
    switch (ch) {
    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '0' ... '9':
    case ',':
    case '.':
    case '_':
    case '+':
    case ':':
    case '@':
    case '%':
    case '/':
    case '-':
        return false;

    default:
        return true;
    }
}

std::streamsize utils::escape_bash::escape_bash_streambuf::xsputn(const char_type* s, std::streamsize n)
{
    const char_type* const end = &s[n];

    while (s < end) {
        {
            const char_type* ptr = s;

            for (ptr = s; ptr < end; ++ptr) {
                if (dangerous(*ptr))
                    break;
            }

            if (ptr > s) {
                m_out.write(s, ptr - s);
                s = ptr;
            }
        }

        if (s >= end)
            break;

        do {
            m_out.put('\\');
            m_out.put(*s++);

        } while (s < end
                 && dangerous(*s));
    }

    return n;
}

std::streambuf::int_type utils::escape_bash::escape_bash_streambuf::overflow(int_type c)
{
    if (dangerous(c))
        m_out.put('\\');

    m_out.put(c);
    return c;
}

// ------------------------------------------------------------

utils::escape_bash::escape_bash(std::ostream& out)
    : std::ostream(&m_buf),
      m_buf(out)
{
}
