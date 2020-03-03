/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#pragma once

#include "script.hh"

//

namespace travelers
{
    struct ForEach : Unit::Traveler {
        using Traveler::Traveler;

        void travel(Unit& unit) const;

        void operator() (Group& group) const override;
        void operator() (Script& script) const override;
        void operator() (Step& step) const override;
    };

    //

    struct Path : Unit::Traveler {
        using Traveler::Traveler;

        void travel(Unit& unit) const;

        void operator() (Group& group) const override;
        void operator() (Script& script) const override;
        void operator() (Step& step) const override;
    };

    //

    struct FindUnit : Unit::Traveler {
    public:
        FindUnit(const std::string& path,
                 const Visitor& visitor);

        void operator() (Group& group) const override;
        void operator() (Script& script) const override;
        void operator() (Step& step) const override;

    private:
        const std::string& m_path;
        mutable std::string m_pathLeft;

        //

        void callIfMatch(Unit& unit) const;
    };
}
