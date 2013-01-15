//
// This program is property of its copyright holder. All rights reserved.
// 

#ifndef __INET_LOCATIONTABLE_H_
#define __INET_LOCATIONTABLE_H_

#include <map>
#include "INETDefs.h"
#include "Address.h"
#include "Coord.h"

class INET_API LocationTable {
    private:
        typedef std::map<const Address, const Coord> AddressToLocationMap;
        AddressToLocationMap addressToLocationMap;
    public:
        LocationTable();
        virtual ~LocationTable();

        virtual Coord getLocation(const Address & address) const;
        virtual void setLocation(const Coord & coord, const Address & address);
};

#endif
