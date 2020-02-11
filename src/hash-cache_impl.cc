/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "hash-cache_impl.hh"

#include "master.hh"
#include "hash-tools.hh"
#include "utils/exec.hh"
#include "utils/stream.hh"

#include <fstream>
#include <sstream>

#include <unistd.h>

ArtifactFile::ArtifactFile(const std::string& name,
                           const std::string& path)
    : Artifact(name),
      m_path(path)
{
}

std::string ArtifactFile::calculateHash() const
{
    std::ifstream ifs(m_path);

    if (ifs) {
        return tools::hash(ifs);
    }
    else {
        return TargetDoesNotExist;
    }
}

// ------------------------------------------------------------

ArtifactDir::ArtifactDir(const std::string& name,
                         const std::string& path,
                         std::vector<std::string>&& exclude)
    : Artifact(name),
      m_path(path),
      m_exclude(std::move( exclude ))
{
}

std::string ArtifactDir::calculateHash() const
{
    using std::flush;
    //

    if (access(m_path.c_str(), X_OK) != 0) {
        return TargetDoesNotExist;
    }

    const std::string command = ({
            std::ostringstream cmdOss;
            utils::escape_bash cmdEsc(cmdOss);

            cmdOss << "find " << flush;
            cmdEsc << m_path << flush;

            if (!m_exclude.empty())
            {
                auto iter = m_exclude.begin();

                cmdOss << " '(' -path " << flush;      // without -o
                cmdEsc << *iter << flush;
                cmdOss << " -prune" << flush;

                for (++iter;
                     iter != m_exclude.end();
                     ++iter)
                {
                    cmdOss << " -o -path " << flush;       // with -o
                    cmdEsc << *iter << flush;
                    cmdOss << " -prune" << flush;
                }

                cmdOss << " ')' -o" << flush;
            }

            cmdOss << " -type f -a -printf '%s\\t%t\\t%p\\n'";

            cmdOss.str();
        });

    utils::Exec dirListingProcess(command);

    return tools::hash(dirListingProcess.read());
}

// ------------------------------------------------------------

DependencyArtifact::DependencyArtifact(Master& master,
                                       const std::string& artifact)
    : Dependency(artifact),
      m_master(master)
{
}

std::string DependencyArtifact::calculateHash() const
{
    return m_master.artifact(m_id).calculateHash();
}

std::string DependencyArtifact::type() const
{
    return "artifact";
}

bool DependencyArtifact::isUpToDate() const
{
    return m_master.artifact(m_id).compareHash(calculateHash());
}

// ------------------------------------------------------------

DependencyData::DependencyData(const std::string& id,
                               const std::string& data)
    : Dependency(id),
      m_data(data)
{
}

std::string DependencyData::calculateHash() const
{
    return tools::hash(m_data);
}

std::string DependencyData::type() const
{
    return "data";
}

bool DependencyData::isUpToDate() const
{
    return compareHash(calculateHash());
}

// ------------------------------------------------------------

DependencyFile::DependencyFile(const std::string& id,
                               const std::string& path)
    : Dependency(id),
      m_path(path)
{
}

std::string DependencyFile::calculateHash() const
{
    std::ifstream ifs(m_path);

    if (ifs) {
        return tools::hash(ifs);
    }
    else {
        return TargetDoesNotExist;
    }
}

std::string DependencyFile::type() const
{
    return "file";
}

bool DependencyFile::isUpToDate() const
{
    return compareHash(calculateHash());
}
