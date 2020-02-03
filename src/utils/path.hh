/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#pragma once

#include <string>

#include <dirent.h>

namespace utils
{
    class OpenDir {
    public:
        OpenDir(const std::string& path);
        ~OpenDir();

        struct dirent* readdir();

    private:
        DIR* m_dir;
    };

    // -----

    void safeMkdir(const std::string& path);
}
