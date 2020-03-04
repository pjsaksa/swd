/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "hash-tools.hh"

#include "config.hh"
#include "hash-cache.hh"
#include "master.hh"
#include "utils/ansi.hh"
#include "utils/exec.hh"
#include "utils/stream.hh"

#include <functional>
#include <iomanip>
#include <istream>
#include <ostream>

//

namespace
{
    std::string calculate_hash(std::function<void(std::ostream&)> feedInput)
    {
        const auto& conf = Config::instance();

        utils::Exec hashCmd(conf.hash_bin,
                            utils::Exec::Flag::NoShell);

        feedInput(hashCmd.write());
        hashCmd.close_write();

        //

        std::string hashSum;

        if (!getline(hashCmd.read(), hashSum, ' ')
            || hashSum.size() != conf.hashsum_size
            )
        {
            throw std::runtime_error{"Configured hash_bin (" + conf.hash_bin+ ") produces invalid hashes"};
        }

        hashSum.erase(conf.hashsum_size);

        return hashSum;
    }
}

//

std::string tools::hash(const std::string& input)
{
    if (input.empty()) {
        return HashCache::TargetDoesNotExist;
    }
    else {
        return calculate_hash([&input] (std::ostream& out)
                              {
                                  out << input;
                              });
    }
}

std::string tools::hash(std::istream& input)
{
    char firstByte;

    if (!input.get(firstByte)) {
        return HashCache::TargetDoesNotExist;
    }

    return calculate_hash([&input, firstByte] (std::ostream& out)
                          {
                              out.put(firstByte);
                              out << input.rdbuf();
                          });
}

// ------------------------------------------------------------

void tools::listArtifacts(const Master& master,
                          std::ostream& out)
{
    namespace col = utils::ansi;
    //

    unsigned int nameWidth = 20;

    for (const auto& artifactPair : master.artifacts)
    {
        if (artifactPair.first.size() > nameWidth)
            nameWidth = artifactPair.first.size();
    }

    //

    utils::restore_ios rios(out);

    for (const auto& artifactPair : master.artifacts)
    {
        out << std::left
            << std::setw(nameWidth)
            << artifactPair.first << " : ";

        const std::string hashSum = artifactPair.second->calculateHash();

        if (hashSum == HashCache::TargetDoesNotExist)
        {
            out << col::Bold << col::Black
                << "Does not exist"
                << col::Normal;
        }
        else if (artifactPair.second->compareHash(hashSum))
        {
            out << col::Bold << col::Green
                << "Up to date"
                << col::Normal;
        }
        else {
            out << col::Bold << col::Red
                << "Dirty"
                << col::Normal;
        }

        out << std::endl;
    }
}
