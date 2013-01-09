//
// Copyright (C) 2005 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "Address.h"
#include "IPv4AddressPolicy.h"

IAddressPolicy * Address::getAddressPolicy() const {
    return &IPv4AddressPolicy::INSTANCE;
}

bool Address::isUnspecified() const
{
    switch (type) {
        case Address::IPV4:
            return ipv4.isUnspecified();
        case Address::MODULEID:
            return moduleId.isUnspecified();
        default:
            throw cRuntimeError("Unknown type");
    }
}

bool Address::isUnicast() const
{
    switch (type) {
        case Address::IPV4:
            return !ipv4.isMulticast() && !ipv4.isLimitedBroadcastAddress();
        case Address::MODULEID:
            return moduleId.isUnicast();
        default:
            throw cRuntimeError("Unknown type");
    }
}

bool Address::isMulticast() const
{
    switch (type) {
        case Address::IPV4:
            return ipv4.isMulticast();
        case Address::MODULEID:
            return moduleId.isMulticast();
        default:
            throw cRuntimeError("Unknown type");
    }
}

bool Address::isBroadcast() const
{
    switch (type) {
        case Address::IPV4:
            return ipv4.isLimitedBroadcastAddress();
        case Address::MODULEID:
            return moduleId.isBroadcast();
        default:
            throw cRuntimeError("Unknown type");
    }
}

bool Address::operator<(const Address& address) const
{
    switch (type) {
        case Address::IPV4:
            return ipv4 < address.ipv4;
        case Address::MODULEID:
            return moduleId < address.moduleId;
        default:
            throw cRuntimeError("Unknown type");
    }
}

bool Address::operator==(const Address& address) const
{
    switch (type) {
        case Address::IPV4:
            return ipv4 == address.ipv4;
        case Address::MODULEID:
            return moduleId == address.moduleId;
        default:
            throw cRuntimeError("Unknown type");
    }
}

bool Address::operator!=(const Address& address) const
{
    switch (type) {
        case Address::IPV4:
            return ipv4 != address.ipv4;
        case Address::MODULEID:
            return moduleId != address.moduleId;
        default:
            throw cRuntimeError("Unknown type");
    }
}

bool Address::matches(const Address& other, int prefixLength) const
{
    switch (type) {
        case Address::IPV4:
            return IPv4Address::maskedAddrAreEqual(ipv4, other.ipv4, IPv4Address::makeNetmask(prefixLength)); //FIXME !!!!!
        case Address::MODULEID:
            return ModuleIdAddress::maskedAddrAreEqual(moduleId, other.moduleId, prefixLength);
        default:
            throw cRuntimeError("Unknown type");
    }
}
