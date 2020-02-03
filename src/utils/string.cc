/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "string.hh"

#include <algorithm>

void utils::tolower(std::string& s)
{
    std::transform(s.begin(), s.end(),
                   s.begin(),
                   [] (unsigned char c)
                   {
                       return std::tolower(c);
                   });
}
