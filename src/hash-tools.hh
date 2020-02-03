/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#pragma once

#include <iosfwd>
#include <string>

// forward declarations

class Master;

//

namespace tools
{
    std::string hash(const std::string& input);
    std::string hash(std::istream& input);

    void listArtifacts(const Master& master, std::ostream& out);
}
