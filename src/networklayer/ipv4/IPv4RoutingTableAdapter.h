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

#ifndef __INET_IPv4ROUTINGTABLEADAPTER_H
#define __INET_IPv4ROUTINGTABLEADAPTER_H

#include "INETDefs.h"
#include "IRoutingTable.h"
#include "IIPv4RoutingTable.h"
#include "IPv4RouteAdapter.h"

/**
 * TODO
 */
class INET_API IPv4RoutingTableAdapter : public IRoutingTable
{
    private:
        IIPv4RoutingTable *rt;
        static IRoute *toGeneric(IPv4Route *e) {return e ? e->asGeneric() : NULL;}
        static IMulticastRoute *toGeneric(IPv4MulticastRoute *e) {return e ? e->asGeneric() : NULL;}
        static IPv4Route *fromGeneric(IRoute *e) {return e ? dynamic_cast<IPv4RouteAdapter*>(e)->getIPv4Route() : NULL;} //FIXME will crash if cast is unsuccessful!!!
        static IPv4MulticastRoute *fromGeneric(IMulticastRoute *e) {return e ? dynamic_cast<IPv4MulticastRouteAdapter*>(e)->getIPv4Route() : NULL;} //FIXME will crash if cast is unsuccessful!!!
    public:
        IPv4RoutingTableAdapter(IIPv4RoutingTable *routingTable) {rt = routingTable;}
        virtual bool isForwardingEnabled() const {return rt->isIPForwardingEnabled();} //XXX inconsistent names
        virtual bool isMulticastForwardingEnabled() const {return rt->isMulticastForwardingEnabled();}
        virtual Address getRouterId() const {return rt->getRouterId();}
        virtual bool isLocalAddress(const Address& dest) const {return rt->isLocalAddress(dest.toIPv4());}
        virtual InterfaceEntry *getInterfaceByAddress(const Address& address) const {return rt->getInterfaceByAddress(address.toIPv4());}
        virtual IRoute *findBestMatchingRoute(const Address& dest) const {return toGeneric(rt->findBestMatchingRoute(dest.toIPv4()));}
        virtual InterfaceEntry *getOutputInterfaceForDestination(const Address& dest) const {return rt->getInterfaceForDestAddr(dest.toIPv4());} //XXX inconsistent names
        virtual Address getNextHopForDestination(const Address& dest) const {return rt->getGatewayForDestAddr(dest.toIPv4());}  //XXX inconsistent names
        virtual bool isLocalMulticastAddress(const Address& dest) const {return rt->isLocalMulticastAddress(dest.toIPv4());}
        virtual IMulticastRoute *findBestMatchingMulticastRoute(const Address &origin, const Address& group) const {return toGeneric(const_cast<IPv4MulticastRoute*>(rt->findBestMatchingMulticastRoute(origin.toIPv4(), group.toIPv4())));} //XXX remove 'const' from IPv4 method?
        virtual int getNumRoutes() const {return rt->getNumRoutes();}
        virtual IRoute *getRoute(int k) const {return toGeneric(rt->getRoute(k));}
        virtual IRoute *getDefaultRoute() const {return toGeneric(rt->getDefaultRoute());}
        virtual void addRoute(IRoute *entry) {rt->addRoute(fromGeneric(entry));}
        virtual IRoute *removeRoute(IRoute *entry) {rt->removeRoute(fromGeneric(entry)); return entry;}
        virtual bool deleteRoute(IRoute *entry) {return rt->deleteRoute(fromGeneric(entry));}
        virtual int getNumMulticastRoutes() const {return rt->getNumMulticastRoutes();}
        virtual IMulticastRoute *getMulticastRoute(int k) const {return toGeneric(rt->getMulticastRoute(k));}
        virtual void addMulticastRoute(IMulticastRoute *entry) {rt->addMulticastRoute(fromGeneric(entry));}
        virtual IMulticastRoute *removeMulticastRoute(IMulticastRoute *entry) {rt->removeMulticastRoute(fromGeneric(entry)); return entry;}
        virtual bool deleteMulticastRoute(IMulticastRoute *entry) {return rt->deleteMulticastRoute(fromGeneric(entry));}
        virtual void purgeExpiredRoutes() {rt->purge();}  //XXX inconsistent names
        virtual IRoute *createRoute() { return (new IPv4Route())->asGeneric(); }
};

#endif

