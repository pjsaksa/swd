/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "string.hh"

#include <algorithm>
#include <iterator>

std::string utils::tolower(const std::string& orig)
{
    std::string s;

    s.reserve(orig.size());

    std::transform(orig.begin(), orig.end(),
                   std::back_inserter(s),
                   [] (unsigned char c)
                   {
                       return std::tolower(c);
                   });

    return s;
}

void utils::tolower(std::string& s)
{
    std::transform(s.begin(), s.end(),
                   s.begin(),
                   [] (unsigned char c)
                   {
                       return std::tolower(c);
                   });
}
