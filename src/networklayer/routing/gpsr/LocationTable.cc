//
// This program is property of its copyright holder. All rights reserved.
// 

#include "LocationTable.h"

LocationTable::LocationTable() {
}

LocationTable::~LocationTable() {
}

Coord LocationTable::getLocation(const Address & address) const {
    AddressToLocationMap::const_iterator it = addressToLocationMap.find(address);
    if (it == addressToLocationMap.end())
        return Coord();
    else
        return it->second;
}

void LocationTable::setLocation(const Coord & coord, const Address & address) {
    addressToLocationMap.insert(std::pair<const Address, const Coord>(address, coord));
}
