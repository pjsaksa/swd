/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "scan.hh"

#include "config.hh"
#include "script.hh"
#include "utils/path.hh"

#include <algorithm>
#include <cstring>
#include <regex>
#include <stdexcept>

#include <sys/stat.h>

using namespace std;

//

namespace
{
    bool isValidScriptName(const string& name)
    {
        static const regex s_exp("[0-9]([0-9][a-z]?)?-[a-zA-Z][a-zA-Z0-9_-]*\\.swd");

        return regex_match(name,
                           s_exp);
    }

    bool isValidGroupName(const string& name)
    {
        static const regex s_exp("[0-9]([0-9][a-z]?)?-[a-zA-Z][a-zA-Z0-9_-]*");

        return regex_match(name,
                           s_exp);
    }

    string removeScriptFileExt(const string& fileName)
    {
        const string& Ext = Script::FileExt;

        if (fileName.compare(fileName.size() - Ext.size(), Ext.size(), Ext) == 0)
        {
            return fileName.substr(0, fileName.size() - Ext.size());
        }

        return fileName;
    }

    void scanDirectory(Group& group,
                       const string& path)
    {
        utils::OpenDir dir(path);

        for (struct dirent* entry;
             (entry =dir.readdir());
             )
        {
            if (entry->d_name[0] == '.') {
                continue;
            }

            const string fileName = entry->d_name;
            const string nameAbs = path + '/' + fileName;

            //

            struct stat st;

            if (stat(nameAbs.c_str(), &st) != 0) {
                throw runtime_error("stat(\"" + nameAbs + "\"): " + strerror(errno));
            }

            if (S_ISDIR(st.st_mode))        // directory
            {
                if (isValidGroupName(fileName)) {
                    Group& subGroup = *new Group(fileName,
                                                 group);

                    group.add(unique_unit_t(&subGroup));

                    scanDirectory(subGroup,
                                  nameAbs);
                }
            }
            else if (st.st_mode & S_IXUSR)      // owner-executable
            {
                if (isValidScriptName(fileName))
                {
                    group.add(std::make_unique<Script>(removeScriptFileExt(fileName),
                                                       group));
                }
            }
        }
    }
}

std::unique_ptr<Group> scanScripts()
{
    auto& conf = Config::instance();
    auto group = Group::newRoot(conf.root);

    scanDirectory(*group,
                  conf.root);

    return group;
}
