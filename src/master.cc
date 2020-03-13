/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "master.hh"

#include "config.hh"
#include "scan.hh"
#include "script-tools.hh"
#include "utils/path.hh"

#include "json/single_include/nlohmann/json.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

using json = nlohmann::json;

//

namespace
{
    std::string artifactCacheFileName()
    {
        return Config::instance().cache_dir + "/artifacts.json";
    }
}

// ------------------------------------------------------------

Artifact& Master::artifact(const std::string& name)
{
    auto iter = artifacts.find(name);

    if (iter == artifacts.end())
        throw std::runtime_error("accessing unknown artifact '" + name + "'");

    return *iter->second;
}

Master& Master::instance()
{
    static Master s_master;
    return s_master;
}

void Master::loadArtifactCache()
{
    std::ifstream ifs(artifactCacheFileName());

    if (ifs) {
        json j;

        if (!(ifs >> j)) {
            throw std::runtime_error("failed to import JSON from artifact save file");
        }

        for (const auto& j_pair : j.items())
        {
            auto artIter = artifacts.find(j_pair.key());

            if (artIter != artifacts.end())
            {
                json& j_value = j_pair.value();

                if (!j_value.is_object()) {
                    throw std::runtime_error("malformed artifact save data: artifact value must be an object");
                }

                // "hash"

                if (j_value.count("hash") != 1) {
                    throw std::runtime_error("malformed artifact save data: artifact must contain hash");
                }

                if (!j_value["hash"].is_string()) {
                    throw std::runtime_error("malformed artifact save data: artifact hash must be a string");
                }

                artIter->second->storeHash(j_value["hash"].get<std::string>());

                // "marks"

                if (j_value.count("marks") == 1)
                {
                    if (!j_value["marks"].is_object()) {
                        throw std::runtime_error("malformed artifact save data: artifact marks must be an object");
                    }

                    for (auto& j_mark : j_value["marks"].items())
                    {
                        if (j_mark.value().is_string() == false) {
                            throw std::runtime_error("malformed artifact save data: artifact marks contain non-string link type");
                        }

                        artIter->second->restoreMark( j_mark.key(),
                                                      Artifact::Link::parse(j_mark.value().get<std::string>()) );
                    }
                }
            }
        }
    }
    else {
        std::cout << "Artifact cache file does not exist." << std::endl;
    }
}

void Master::saveArtifactCache() const
{
    utils::safeMkdir(Config::instance().cache_dir);

    //

    const std::string fileName = artifactCacheFileName();
    const std::string tmpName = fileName + ".tmp";

    std::ofstream ofs(tmpName);

    if (!ofs) {
        throw std::runtime_error("failed to open artifact cache file: " + tmpName);
    }

    //

    json j;

    for ( const auto& artPair : artifacts)
    {
        json& j_art = j[artPair.first];

        // "hash"

        j_art["hash"] = artPair.second->getHashSum();

        // "marks"

        bool first = true;

        for (const auto& mark : artPair.second->getMarks())
        {
            if (first) {
                j_art["marks"] = json::object();
                first = false;
            }

            j_art["marks"][mark.name] = Artifact::Link::typeToString( mark.type );
        }
    }

    //

    if (!(ofs << j.dump(1, '\t') << std::endl)) {
        throw std::runtime_error("failed to save artifact JSON data");
    }

    if (rename(tmpName.c_str(), fileName.c_str()) != 0) {
        throw std::runtime_error("failed to rename '" + tmpName + "' over '" + fileName + "'");
    }
}

Master::Master()
    : root(scanScripts())
{
    tools::loadScriptConfig(*this, *root);
    loadArtifactCache();
}

Master::~Master()
{
    try {
        saveArtifactCache();
        tools::saveScriptCache(*root);
    }
    catch (std::exception& e) {
        std::cerr << "exception during saving cache:\n    " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "unknown exception during saving cache" << std::endl;
    }
}
