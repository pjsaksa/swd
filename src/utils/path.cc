/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "path.hh"

#include <cstring>
#include <stdexcept>

#include <sys/stat.h>
#include <sys/types.h>

using namespace std;

//

utils::OpenDir::OpenDir(const string &path)
    : m_dir(opendir(path.c_str()))
{
    if (!m_dir) {
        throw runtime_error("opendir(" + path + "): " + strerror(errno));
    }
}

utils::OpenDir::~OpenDir()
{
    closedir(m_dir);
}

struct dirent *utils::OpenDir::readdir()
{
    return ::readdir(m_dir);
}

// ------------------------------------------------------------

void utils::safeMkdir(const std::string& path)
{
    if (mkdir(path.c_str(), 0777) != 0)
    {
        switch (errno) {
        case EEXIST:
            {
                struct stat st;

                if (stat(path.c_str(), &st) != 0) {
                    throw runtime_error("mkdir(" + path + "): stat: " + strerror(errno));
                }

                if (!S_ISDIR(st.st_mode)) {
                    throw runtime_error("mkdir(" + path + "): path already exists but is not a directory");
                }
            }
            break;

        default:
            throw std::runtime_error("mkdir(" + path + ") failed: " + strerror(errno));
        }
    }
}
