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

#ifndef __INET_IPv6ROUTINGTABLEADAPTER_H
#define __INET_IPv6ROUTINGTABLEADAPTER_H

#include "INETDefs.h"
#include "IRoutingTable.h"
#include "IPv6RoutingTable.h"
#include "IPv6RouteAdapter.h"

/**
 * TODO
 */
//TODO nothing works, really...
class INET_API IPv6RoutingTableAdapter : public IRoutingTable
{
    private:
        IPv6RoutingTable *rt; //TODO IIPv6RoutingTable is missing!
        static IRoute *toGeneric(IPv6Route *e) {return e ? e->asGeneric() : NULL;}
        //TODO static IGenericMulticastRoute *toGeneric(IPv6MulticastRoute *e) {return e ? e->asGeneric() : NULL;}
        static IPv6Route *fromGeneric(IRoute *e) {return e ? dynamic_cast<IPv6RouteAdapter*>(e)->getIPv6Route() : NULL;} //FIXME will crash if cast is unsuccessful!!!
        //TODO static IPv6MulticastRoute *fromGeneric(IGenericMulticastRoute *e) {return e ? dynamic_cast<IPv6MulticastRouteAdapter*>(e)->getIPv6Route() : NULL;} //FIXME will crash if cast is unsuccessful!!!
    public:
        IPv6RoutingTableAdapter(IPv6RoutingTable *routingTable) {rt = routingTable;}
        virtual bool isForwardingEnabled() const {return rt->isRouter();}  //XXX inconsistent names
        virtual bool isMulticastForwardingEnabled() const {return true; /*TODO rt->isMulticastForwardingEnabled();*/}
        virtual Address getRouterId() const {return Address(); /*TODO rt->getRouterId();*/}
        virtual bool isLocalAddress(const Address& dest) const {return rt->isLocalAddress(dest.toIPv6());}
        virtual bool isLocalBroadcastAddress(const Address& dest) const {return false; /*TODO rt->isLocalBroadcastAddress(dest.toIPv6());*/}
        virtual InterfaceEntry *getInterfaceByAddress(const Address& address) const {return rt->getInterfaceByAddress(address.toIPv6());}
        virtual InterfaceEntry *findInterfaceByLocalBroadcastAddress(const Address& dest) const {return NULL; /*TODO rt->findInterfaceByLocalBroadcastAddress(dest.toIPv6());*/}
        virtual IRoute *findBestMatchingRoute(const Address& dest) const {return toGeneric(const_cast<IPv6Route*>(rt->doLongestPrefixMatch(dest.toIPv6())));}  //FIXME what a name??!! also: remove const; ALSO: THIS DOES NOT UPDATE DESTCACHE LIKE METHODS BUILT ON IT!
        virtual InterfaceEntry *getOutputInterfaceForDestination(const Address& dest) const {return NULL; /*TODO: rt->getInterfaceForDestAddr(dest.toIPv6()); */} //XXX exists but different API
        virtual Address getNextHopForDestination(const Address& dest) const {return Address(); /*TODO: rt->getGatewayForDestAddr(dest.toIPv6());*/}  //XXX exists but different API
        virtual bool isLocalMulticastAddress(const Address& dest) const {return false; /*TODO rt->isLocalMulticastAddress(dest.toIPv6());*/}
        virtual IMulticastRoute *findBestMatchingMulticastRoute(const Address &origin, const Address& group) const {return NULL; /*TODO toGeneric(rt->findBestMatchingMulticastRoute(origin.toIPv6(), group.toIPv6()));*/}
        virtual int getNumRoutes() const {return rt->getNumRoutes();}
        virtual IRoute *getRoute(int k) const {return toGeneric(rt->getRoute(k));}
        virtual IRoute *getDefaultRoute() const {return NULL; /*TODO toGeneric(rt->getDefaultRoute());*/}
        virtual void addRoute(IRoute *entry) {rt->addRoutingProtocolRoute(fromGeneric(entry));} //XXX contrast that with addStaticRoute()!
        virtual IRoute *removeRoute(IRoute *entry) {rt->removeRoute(fromGeneric(entry)); return entry;}
        virtual bool deleteRoute(IRoute *entry) {rt->removeRoute(fromGeneric(entry)); return true; /*TODO retval!*/}
        virtual int getNumMulticastRoutes() const {return 0; /*TODO rt->getNumMulticastRoutes();*/}
        virtual IMulticastRoute *getMulticastRoute(int k) const {return NULL; /*TODO toGeneric(rt->getMulticastRoute(k));*/}
        virtual void addMulticastRoute(IMulticastRoute *entry) {/*TODO rt->addMulticastRoute(fromGeneric(entry));*/}
        virtual IMulticastRoute *removeMulticastRoute(IMulticastRoute *entry) {/*TODO rt->removeMulticastRoute(fromGeneric(entry));*/ return entry;}
        virtual bool deleteMulticastRoute(IMulticastRoute *entry) {return false; /*TODO: rt->deleteMulticastRoute(fromGeneric(entry));*/}
        virtual void purgeExpiredRoutes() {/*TODO rt->purge();*/}  //XXX inconsistent names
        virtual IRoute *createRoute() { return (new IPv6Route(IPv6Address(), 0, IPv6Route::STATIC))->asGeneric(); }
};

#endif

