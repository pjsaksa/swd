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
class Unit;

//

namespace tools
{
    std::string conjurePath(Unit& unit);
    std::string conjureExec(Unit& unit);
    void loadScriptConfig(Master& master, Unit& unit);

    void saveScriptCache(Unit& unit);

    void execute(Master& master, int stepCount, bool showNext, bool interactive);
    void listSteps(Unit& unit, std::ostream& out);

    void undo(Master& master, const std::string& stepName);
    void force(Master& master, const std::string& stepName);

    unsigned int rebuildArtifact(Master& master, const std::string& artifactName);
}
