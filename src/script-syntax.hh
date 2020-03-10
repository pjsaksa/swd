/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#pragma once

#include <stdexcept>
#include <string>

#include "json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;

//

namespace syntax
{
    struct syntax_error : std::runtime_error {
        using runtime_error::runtime_error;
    };

    //

    void checkArtifact(const std::string& name, const json& j);
    void checkDependency(const json& j);
    void checkDependencyArray(const json& j);
    void checkRule(const std::string& name, const json& j);
    void checkStep(const json& j);

    void checkGroupFile(const json& j);
    void checkScriptFile(const json& j);
}
