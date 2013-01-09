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

/**
 * TODO
 */
class INET_API Address
{
    private:
        //XXX this is a simplistic temporary implementation; TODO implement in 128 bits, using magic (use a reserved IPv6 range to store non-IPv6 addresses)
        IPv4Address ipv4;
        IPv6Address ipv6;
        MACAddress mac;
    public:
        Address() {}
        Address(const IPv4Address& addr) {set(addr);}
        Address(const IPv6Address& addr) {set(addr);}
        Address(const MACAddress& addr) {set(addr);}

        void set(const IPv4Address& addr) {ipv4 = addr;}
        void set(const IPv6Address& addr) {ipv6 = addr;}
        void set(const MACAddress& addr) {mac = addr;}

        IPv4Address toIPv4() const {return ipv4;}   //XXX names are inconsistent with IPvXAddress (rename IPvXAddress methods?)
        IPv6Address toIPv6() const {return ipv6;}
        MACAddress toMAC() const {return mac;};  // IEU-48

        //TODO add more functions: getType(), prefix matching, etc
        int getPrefixLength() const { return 32; } //FIXME not good, remove!!!!!! IT DOES NOT DO WHAT YOU EXPECT
        bool isUnspecified() const { return ipv4.isUnspecified(); }
        bool isUnicast() const { return !ipv4.isMulticast() && !ipv4.isLimitedBroadcastAddress(); }
        bool isMulticast() const { return ipv4.isMulticast(); }
        bool isBroadcast() const { return ipv4.isLimitedBroadcastAddress(); }
        bool operator<(const Address& address) const { return ipv4 < address.ipv4; }
        bool operator==(const Address& address) const { return ipv4 == address.ipv4; }
        bool operator!=(const Address& address) const { return ipv4 != address.ipv4; }

        bool matches(const Address& other, int prefixLength) const;

        std::string str() { return ipv4.str(); }
};

inline std::ostream& operator<<(std::ostream& os, const Address& address)
{
    return os << address.toIPv4().str();
}

inline bool Address::matches(const Address& other, int prefixLength) const
{
    return IPv4Address::maskedAddrAreEqual(ipv4, other.ipv4, IPv4Address::makeNetmask(prefixLength)); //FIXME !!!!!
}

#endif
