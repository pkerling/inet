//
// Copyright (C) 2008 Andras Varga
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

#ifndef __INET_IPv4ROUTE_H
#define __INET_IPv4ROUTE_H


#include "INETDefs.h"

#include "IPv4Address.h"

class InterfaceEntry;
class IRoutingTable;

/**
 * Generic unicast route
 *
 * @see IRoutingTable, RoutingTable
 */
class INET_API IGenericRoute : public cObject
{
  public:
// ehelyett: cObject*
//    /** Specifies where the route comes from */
//    enum RouteSource
//    {
//        MANUAL,       ///< manually added static route
//        IFACENETMASK, ///< comes from an interface's netmask
//        RIP,          ///< managed by the given routing protocol
//        OSPF,         ///< managed by the given routing protocol
//        BGP,          ///< managed by the given routing protocol
//        ZEBRA,        ///< managed by the Quagga/Zebra based model
//        MANET,        ///< managed by manet, search exact address
//        MANET2,       ///< managed by manet, search approximate address
//    };

//  private:
//    IRoutingTable *rt;    ///< the routing table in which this route is inserted, or NULL
//    IPv4Address dest;     ///< Destination
//    IPv4Address netmask;  ///< Route mask
//    IPv4Address gateway;  ///< Next hop
//    InterfaceEntry *interfacePtr; ///< interface
//    RouteSource source;   ///< manual, routing prot, etc.
//    int metric;           ///< Metric ("cost" to reach the destination)

//  public:
//    enum {F_DESTINATION, F_NETMASK, F_GATEWAY, F_IFACE, F_TYPE, F_SOURCE, F_METRIC, F_LAST}; // field codes for changed()

  private:
    // copying not supported: following are private and also left undefined
    IGenericRoute(const IGenericRoute& obj);
    IGenericRoute& operator=(const IGenericRoute& obj);

  protected:
    void changed(int fieldCode);

  public:
//    IGenericRoute() : rt(NULL), interfacePtr(NULL), source(MANUAL), metric(0) {}
//    virtual ~IGenericRoute() {}
//    virtual std::string info() const;
//    virtual std::string detailedInfo() const;
//
//    bool operator==(const IGenericRoute& route) const { return equals(route); }
//    bool operator!=(const IGenericRoute& route) const { return !equals(route); }
//    bool equals(const IGenericRoute& route) const;
//
//    /** To be called by the routing table when this route is added or removed from it */
//    virtual void setRoutingTable(IRoutingTable *rt) {this->rt = rt;}
//    IRoutingTable *getRoutingTable() const {return rt;}

    /** test validity of route entry, e.g. check expiry */
    virtual bool isValid() const { return true; }  //XXX isExpired

    virtual void setDestination(IPv4Address _dest)  { if (dest != _dest) {dest = _dest; changed(F_DESTINATION);} }
    virtual void setNetmask(IPv4Address _netmask) /*XXX prefixLength */ { if (netmask != _netmask) {netmask = _netmask; changed(F_NETMASK);} }
    virtual void setGateway(IPv4Address _gateway) /*XXX nextHopAddress */ { if (gateway != _gateway) {gateway = _gateway; changed(F_GATEWAY);} }
    virtual void setInterface(InterfaceEntry *_interfacePtr)  { if (interfacePtr != _interfacePtr) {interfacePtr = _interfacePtr; changed(F_IFACE);} }
    virtual void setSource(RouteSource _source) /*XXX cObject */ { if (source != _source) {source = _source; changed(F_SOURCE);} }
    virtual void setMetric(int _metric) /*XXX double?*/ { if (metric != _metric) {metric = _metric; changed(F_METRIC);} }

    /** Destination address prefix to match */
    Address getDestination() const {return dest;}

    /** Represents length of prefix to match */
//    IPv4Address getNetmask() const {return netmask;}
    int getPrefixLength() = 0;

    /** Next hop address */
    Address getNextHop() const {return gateway;}

    /** Next hop interface */
    InterfaceEntry *getInterface() const {return interfacePtr;}

//    /** Convenience method */
//    const char *getInterfaceName() const;  //????

    /** Source of route. MANUAL (read from file), from routing protocol, etc */
    cObject *getSource() const {return source;}

    /** "Cost" to reach the destination */
    int getMetric() const {return metric;}
};

/**
 * IPv4 multicast route in IRoutingTable.
 * Multicast routing protocols may extend this class to store protocol
 * specific fields.
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
 * @see IRoutingTable, RoutingTable
 */
class INET_API IGenericMulticastRoute : public cObject
{
//  public:
//    class ChildInterface
//    {
//      protected:
//        InterfaceEntry *ie;
//        bool _isLeaf;        // for TRPB support
//      public:
//        ChildInterface(InterfaceEntry *ie, bool isLeaf = false) : ie(ie), _isLeaf(isLeaf) {}
//        virtual ~ChildInterface() {}
//
//        InterfaceEntry *getInterface() { return ie; }
//        bool isLeaf() { return _isLeaf; }
//    };
//
//    typedef std::vector<ChildInterface*> ChildInterfaceVector;
//
//    /** Specifies where the route comes from */
//    enum RouteSource
//    {
//        MANUAL,       ///< manually added static route
//        DVMRP,        ///< managed by DVMRP router
//        PIM_SM,       ///< managed by PIM-SM router
//    };
//
//  private:
//    IRoutingTable *rt;             ///< the routing table in which this route is inserted, or NULL
//    IPv4Address origin;            ///< Source network
//    IPv4Address originNetmask;     ///< Source network mask
//    IPv4Address group;             ///< Multicast group, if unspecified then matches any
//    InterfaceEntry *parent;        ///< Parent interface
//    ChildInterfaceVector children; ///< Child interfaces
//    RouteSource source;            ///< manual, routing prot, etc.
//    int metric;                    ///< Metric ("cost" to reach the source)

  public:
    // field codes for changed()
    enum {F_ORIGIN, F_ORIGINMASK, F_MULTICASTGROUP, F_PARENT, F_CHILDREN, F_SOURCE, F_METRIC, F_LAST};

  protected:
    void changed(int fieldCode);

  private:
    // copying not supported: following are private and also left undefined
    IGenericMulticastRoute(const IGenericMulticastRoute& obj);
    IGenericMulticastRoute& operator=(const IGenericMulticastRoute& obj);

  public:
//    IGenericMulticastRoute() : rt(NULL), parent(NULL), source(MANUAL), metric(0) {}
//    virtual ~IGenericMulticastRoute();
//    virtual std::string info() const;
//    virtual std::string detailedInfo() const;
//
//    /** To be called by the routing table when this route is added or removed from it */
//    virtual void setRoutingTable(IRoutingTable *rt) {this->rt = rt;}
//    IRoutingTable *getRoutingTable() const {return rt;}
//
//    /** test validity of route entry, e.g. check expiry */
//    virtual bool isValid() const { return true; }

//    virtual bool matches(const Address &origin, const Address &group) const
//    {
//        return (this->group.isUnspecified() || this->group == group) &&
//                IPv4Address::maskedAddrAreEqual(origin, this->origin, this->originNetmask);
//    }

    virtual void setOrigin(IPv4Address _origin)  { if (origin != _origin) {origin = _origin; changed(F_ORIGIN);} }
    virtual void setOriginNetmask(IPv4Address _netmask) /*XXX prefixLength*/ { if (originNetmask != _netmask) {originNetmask = _netmask; changed(F_ORIGINMASK);} }
    virtual void setMulticastGroup(IPv4Address _group)  { if (group != _group) {group = _group; changed(F_MULTICASTGROUP);} }
    virtual void setParent(InterfaceEntry *_parent)  { if (parent != _parent) {parent = _parent; changed(F_PARENT);} }
    virtual bool addChild(InterfaceEntry *ie, bool isLeaf);
    virtual bool removeChild(InterfaceEntry *ie);
    virtual void setSource(RouteSource _source) /*XXX cObject*/ { if (source != _source) {source = _source; changed(F_SOURCE);} }
    virtual void setMetric(int _metric)  { if (metric != _metric) {metric = _metric; changed(F_METRIC);} }

    /** Source address prefix to match */
    Address getOrigin() const {return origin;}

    /** Represents length of prefix to match */
    Address getOriginNetmask() const {return originNetmask;}

    /** Multicast group address */
    Address getMulticastGroup() const {return group;}

    /** Parent interface */
    InterfaceEntry *getParent() const {return parent;}

    /** Child interfaces */
    const ChildInterfaceVector &getChildren() const {return children;}

    /** Source of route. MANUAL (read from file), from routing protocol, etc */
    RouteSource getSource() const {return source;}

    /** "Cost" to reach the destination */
    int getMetric() const {return metric;}
};
#endif // __INET_IPv4ROUTE_H

