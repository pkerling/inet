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

#ifndef __INET_IPv4ROUTEADAPTER_H
#define __INET_IPv4ROUTEADAPTER_H

#include "IRoute.h"

/**
 * TODO
 */
class INET_API IPv4RouteAdapter : public IRoute
{
    private:
        IPv4Route *e;
    public:
        IPv4RouteAdapter(IPv4Route *route) {e = route;}
        IPv4Route *getIPv4Route() {return e;}
        virtual IRoutingTable *getRoutingTable() const {return e->getRoutingTable()->asGeneric();}

        virtual void setEnabled(bool enabled) {/*TODO: e->setEnabled(enabled);*/}
        virtual void setDestination(const Address& dest) {e->setDestination(dest.toIPv4());}
        virtual void setPrefixLength(int len) {e->setNetmask(IPv4Address::makeNetmask(len));}
        virtual void setNextHop(const Address& nextHop) {e->setGateway(nextHop.toIPv4());}  //TODO rename IPv4 method
        virtual void setInterface(InterfaceEntry *ie) {e->setInterface(ie);}
        virtual void setSource(cObject *source) { e->setSourceXXX(source); }
        virtual void setMetric(int metric) {e->setMetric(metric);}

        virtual bool isEnabled() const {return true; /*TODO: e->isEnabled();*/}
        virtual bool isExpired() const {return !e->isValid();}  //TODO rename IPv4 method
        virtual Address getDestination() const {return e->getDestination();}
        virtual int getPrefixLength() const {return e->getNetmask().getNetmaskLength();}
        virtual Address getNextHop() const {return e->getGateway();} //TODO rename IPv4 method
        virtual InterfaceEntry *getInterface() const {return e->getInterface();}
        virtual cObject *getSource() const {return e->getSourceXXX(); }
        virtual int getMetric() const {return e->getMetric();}
        virtual cObject *getProtocolData() const { return e->getProtocolData(); }
        virtual void setProtocolData(cObject *protocolData) { e->setProtocolData(protocolData); }
};

/**
 * TODO
 */
class INET_API IPv4MulticastRouteAdapter : public IMulticastRoute
{
    private:
        IPv4MulticastRoute *e;
    public:
        IPv4MulticastRouteAdapter(IPv4MulticastRoute *route) {e = route;}
        IPv4MulticastRoute *getIPv4Route() {return e;}
        virtual IRoutingTable *getRoutingTable() const {return e->getRoutingTable()->asGeneric();}

        virtual void setEnabled(bool enabled) {/*TODO: e->setEnabled(enabled);*/}
        virtual void setOrigin(const Address& origin) {e->setOrigin(origin.toIPv4());}
        virtual void setPrefixLength(int len) {e->setOriginNetmask(IPv4Address::makeNetmask(len));} //TODO inconsistent naming
        virtual void setMulticastGroup(const Address& group) {e->setMulticastGroup(group.toIPv4());}
        virtual void setParent(InterfaceEntry *ie) {e->setParent(ie);}
        virtual bool addChild(InterfaceEntry *ie, bool isLeaf) {return e->addChild(ie, isLeaf);}
        virtual bool removeChild(InterfaceEntry *ie) {return e->removeChild(ie);}
        virtual void setSource(cObject *source) {/*TODO: e->setSource(source);*/}
        virtual void setMetric(int metric) {e->setMetric(metric);}

        virtual bool isEnabled() const {return true; /*TODO: e->isEnabled();*/}
        virtual bool isExpired() const {return !e->isValid();}  //TODO rename IPv4 method
        virtual Address getOrigin() const {return e->getOrigin();}
        virtual int getPrefixLength() const {return e->getOriginNetmask().getNetmaskLength();} //TODO inconsistent naming
        virtual Address getMulticastGroup() const {return e->getMulticastGroup();}
        virtual InterfaceEntry *getParent() const {return e->getParent();}
        virtual int getNumChildren() const {return e->getChildren().size();}
        virtual InterfaceEntry *getChild(int i) const {return e->getChildren()[i]->getInterface();}  //XXX impedance mismatch
        virtual bool getChildIsLeaf(int i) const {return e->getChildren()[i]->isLeaf();}
        virtual cObject *getSource() const {return NULL; /*TODO: e->getSource();*/}
        virtual int getMetric() const {return e->getMetric();}
};

#endif

