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

#ifndef __INET_IGENERICROUTE_H
#define __INET_IGENERICROUTE_H

#include "INETDefs.h"
#include "Address.h"
#include "InterfaceEntry.h"

class IRoutingTable;

/**
 * C++ interface for accessing unicast routing table entries of various protocols (IPv4, IPv6, etc)
 * in a uniform way.
 *
 * @see IRoutingTable, IPv4Route, IPv6Route
 */
//TODO the "Generic" can be dropped from the name, once IRoutingTable is renamed to IPv4RoutingTable
class INET_API IRoute
{
    public:
//TODO maybe:
//    virtual std::string info() const;
//    virtual std::string detailedInfo() const;
//
//    bool operator==(const IGenericRoute& route) const { return equals(route); }
//    bool operator!=(const IGenericRoute& route) const { return !equals(route); }
//    bool equals(const IGenericRoute& route) const;

        virtual ~IRoute() {}

        /** The routing table in which this route is inserted, or NULL. */
        virtual IRoutingTable *getRoutingTable() const = 0;

        virtual void setEnabled(bool enabled) = 0;
        virtual void setDestination(const Address& dest) = 0;
        virtual void setPrefixLength(int l) = 0;
        virtual void setNextHop(const Address& nextHop) = 0;
        virtual void setInterface(InterfaceEntry *ie) = 0;
        virtual void setSource(cObject *source) = 0;
        virtual void setMetric(int metric) = 0;  //XXX double?

        /** Disabled entries are ignored by routing until the became enabled again. */
        virtual bool isEnabled() const = 0;

        /** Expired entries are ignored by routing, and may be periodically purged. */
        virtual bool isExpired() const = 0;

        /** Destination address prefix to match */
        virtual Address getDestination() const = 0;

        /** Represents length of prefix to match */
        virtual int getPrefixLength() const = 0;

        /** Next hop address */
        virtual Address getNextHop() const = 0;

        /** Next hop interface */
        virtual InterfaceEntry *getInterface() const = 0;

        /** Source of route */
        virtual cObject *getSource() const = 0;

        /** Cost to reach the destination */
        virtual int getMetric() const = 0;

        virtual cObject *getProtocolData() const = 0;
        virtual void setProtocolData(cObject *protocolData) = 0;
};

// TODO: move into info()?
inline std::ostream& operator<<(std::ostream& out, const IRoute * route)
{
    out << "destination = " << route->getDestination();
    out << ", prefixLength = " << route->getPrefixLength();
    out << ", nextHop = " << route->getNextHop();
    out << ", metric = " << route->getMetric();
    if (route->getInterface())
        out << ", interface = " << route->getInterface()->getName();
    return out;
};

/**
 * Generic multicast route in an IRoutingTable.
 *
 * Multicast datagrams are forwarded along the edges of a multicast tree.
 * The tree might depend on the multicast group and the source (origin) of
 * the multicast datagram.
 *
 * The forwarding algorithm chooses the route according the to origin
 * (source address) and multicast group (destination address) of the received
 * datagram. The route might specify a prefix of the origin and a multicast
 * group to be matched.
 *
 * Then the forwarding algorithm copies the datagrams arrived on the parent
 * (upstream) interface to the child interfaces (downstream). If there are no
 * downstream routers on a child interface (i.e. it is a leaf in the multicast
 * routing tree), then the datagram is forwarded only if there are listeners
 * of the multicast group on that link (TRPB routing).
 *
 * @see IRoutingTable, IPv4MulticastRoute, IPv6MulticastRoute
 */
//TODO the "Generic" can be dropped from the name, once IRoutingTable is renamed to IPv4RoutingTable
//TODO decide whether IPv4Route should implement this or rather have a wrapper?
class INET_API IMulticastRoute
{
  public:
//TODO maybe:
//    virtual std::string info() const;
//    virtual std::string detailedInfo() const;

        virtual ~IMulticastRoute() {}

        /** The routing table in which this route is inserted, or NULL. */
        virtual IRoutingTable *getRoutingTable() const = 0;

        virtual void setEnabled(bool enabled) = 0;
        virtual void setOrigin(const Address& origin) = 0;
        virtual void setPrefixLength(int len) = 0;
        virtual void setMulticastGroup(const Address& group) = 0;
        virtual void setParent(InterfaceEntry *ie) = 0;
        virtual bool addChild(InterfaceEntry *ie, bool isLeaf) = 0;
        virtual bool removeChild(InterfaceEntry *ie) = 0;
        virtual void setSource(cObject *source) = 0;
        virtual void setMetric(int metric) = 0;

        /** Disabled entries are ignored by routing until the became enabled again. */
        virtual bool isEnabled() const = 0;

        /** Expired entries are ignored by routing, and may be periodically purged. */
        virtual bool isExpired() const = 0;

        /** Source address prefix to match */
        virtual Address getOrigin() const = 0;

        /** Prefix length to match */
        virtual int getPrefixLength() const = 0;

        /** Multicast group address */
        virtual Address getMulticastGroup() const = 0;

        /** Parent interface */
        virtual InterfaceEntry *getParent() const = 0;

        /** Child interfaces */
        virtual int getNumChildren() const = 0;

        /** Returns the ith child interface */
        virtual InterfaceEntry *getChild(int i) const = 0;

        /** Returns true if the ith child interface is a leaf */
        virtual bool getChildIsLeaf(int i) const = 0;

        /** Source of route */
        virtual cObject *getSource() const = 0;

        /** Cost to reach the destination */
        virtual int getMetric() const = 0;
};

#endif
