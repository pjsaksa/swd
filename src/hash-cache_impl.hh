/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#pragma once

#include "hash-cache.hh"

#include <string>
#include <vector>

// forward declarations

class Master;

//

class ArtifactFile : public Artifact {
public:
    ArtifactFile(const std::string& name,
                 const std::string& path);

    std::string calculateHash() const override;

private:
    std::string m_path;
};

// -----

class ArtifactDir : public Artifact {
public:
    ArtifactDir(const std::string& name,
                const std::string& path,
                std::vector<std::string>&& exclude);

    std::string calculateHash() const override;

private:
    std::string m_path;
    std::vector<std::string> m_exclude;
};

// ------------------------------------------------------------

class DependencyArtifact : public Dependency {
public:
    DependencyArtifact(Master& master,
                       const std::string& artifact);

    std::string calculateHash() const override;

    std::string type() const override;

private:
    Master& m_master;
};

// -----

class DependencyData : public Dependency {
public:
    DependencyData(const std::string& id,
                   const std::string& data);

    std::string calculateHash() const override;

    std::string type() const override;

private:
    std::string m_data;
};

// -----

class DependencyFile : public Dependency {
public:
    DependencyFile(const std::string& id,
                   const std::string& path);

    std::string calculateHash() const override;

    std::string type() const override;

private:
    std::string m_path;
};
