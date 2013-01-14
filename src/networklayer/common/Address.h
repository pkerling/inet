//
// Copyright (C) 2012 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#ifndef __INET_ADDRESS_H
#define __INET_ADDRESS_H

#include "INETDefs.h"
#include "uint128.h"

#include "IPv4Address.h"
#include "IPv6Address.h"
#include "MACAddress.h"
#include "ModuleIdAddress.h"
#include "ModulePathAddress.h"

class IAddressPolicy;

/**
 * TODO
 */
class INET_API Address
{
    public:
        enum AddressType {
            IPv4,
            IPv6,
            MAC,
            MODULEID,
            MODULEPATH
        };
    private:
        AddressType type;
        //XXX this is a simplistic temporary implementation; TODO implement in 128 bits, using magic (use a reserved IPv6 range to store non-IPv6 addresses)
        IPv4Address ipv4;
        IPv6Address ipv6;
        MACAddress mac;
        ModuleIdAddress moduleId;
        ModulePathAddress modulePath;
    public:
        Address() : type(IPv4) {}
        Address(const char *str) { tryParse(str); }
        Address(const IPv4Address& addr) {set(addr);}
        Address(const IPv6Address& addr) {set(addr);}
        Address(const MACAddress& addr) {set(addr);}
        Address(const ModuleIdAddress& addr) {set(addr);}
        Address(const ModulePathAddress& addr) {set(addr);}

        void set(const IPv4Address& addr) { type = IPv4; ipv4 = addr; }
        void set(const IPv6Address& addr) { type = IPv6; ipv6 = addr; }
        void set(const MACAddress& addr) { type = MAC; mac = addr; }
        void set(const ModuleIdAddress& addr) { type = MODULEID;  moduleId = addr; }
        void set(const ModulePathAddress& addr) { type = MODULEPATH;  modulePath = addr; }

        IPv4Address toIPv4() const {return ipv4;}   //XXX names are inconsistent with Address (rename Address methods?)
        IPv6Address toIPv6() const {return ipv6;}
        MACAddress toMAC() const {return mac;}  // IEU-48
        ModuleIdAddress toModuleId() const {return moduleId;}
        ModulePathAddress toModulePath() const {return modulePath;}

        //TODO add more functions: getType(), prefix matching, etc
        AddressType getType() const { return type; }
        IAddressPolicy * getAddressPolicy() const;
        bool tryParse(const char *addr);

        int getPrefixLength() const { return 32; } //FIXME not good, remove!!!!!! IT DOES NOT DO WHAT YOU EXPECT
        bool isUnspecified() const;
        bool isUnicast() const;
        bool isMulticast() const;
        bool isBroadcast() const;
        bool operator<(const Address& address) const;
        bool operator==(const Address& address) const;
        bool operator!=(const Address& address) const;

        bool matches(const Address& other, int prefixLength) const;

        std::string str() const { return ipv4.str(); }
};

inline std::ostream& operator<<(std::ostream& os, const Address& address)
{
    switch (address.getType()) {
        case Address::IPv4:
            return os << address.toIPv4().str();
        case Address::IPv6:
            return os << address.toIPv6().str();
        case Address::MAC:
            return os << address.toMAC().str();
        case Address::MODULEID:
            return os << address.toModuleId().str();
        case Address::MODULEPATH:
            return os << address.toModulePath().str();
        default:
            throw cRuntimeError("Unknown type");
    }
}

#endif
