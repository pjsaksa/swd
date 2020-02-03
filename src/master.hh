/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#pragma once

#include "hash-cache.hh"
#include "script.hh"

#include <map>
#include <string>

struct Master {
    unique_group_t root;
    std::map<std::string, unique_artifact_t> artifacts;

    //

    Artifact& artifact(const std::string& name);

    //

    static Master& instance();

private:
    void loadArtifactCache();
    void saveArtifactCache() const;

    //

    Master();
    ~Master();
};
