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

#ifndef __INET_IPv6ROUTEADAPTER_H
#define __INET_IPv6ROUTEADAPTER_H

#include "IRoute.h"

/**
 * TODO
 */
class INET_API IPv6RouteAdapter : public IRoute
{
    private:
        IPv6Route *e;
    public:
        IPv6RouteAdapter(IPv6Route *route) {e = route;}
        IPv6Route *getIPv6Route() {return e;}
        virtual IRoutingTable *getRoutingTable() const {return NULL; /*TODO: e->getRoutingTable()->asGeneric();*/}

        virtual void setEnabled(bool enabled) {/*TODO: e->setEnabled(enabled);*/}
        virtual void setDestination(const Address& dest) {/*TODO: e->setDestination(dest.toIPv6());*/}
        virtual void setPrefixLength(int len) {/*TODO: e->setPrefixLength(len));*/}
        virtual void setNextHop(const Address& nextHop) {e->setNextHop(nextHop.toIPv6());}
        virtual void setInterface(InterfaceEntry *ie) {/*TODO: e->setInterface(ie);*/}  //TODO change IPv6Route API from interfaceID to interface ptr
        virtual void setSource(cObject *source) {/*TODO: e->setSource(source);*/}
        virtual void setMetric(int metric) {e->setMetric(metric);}

        virtual bool isEnabled() const {return true; /*TODO: e->isEnabled();*/}
        virtual bool isExpired() const {return false; /*e->isExpired();*/}
        virtual Address getDestination() const {return e->getDestPrefix();} //TODO rename IPv6 method
        virtual int getPrefixLength() const {return e->getPrefixLength();}
        virtual Address getNextHop() const {return e->getNextHop();}
        virtual InterfaceEntry *getInterface() const {return NULL; /*TODO e->getInterface();*/} //TODO change IPv6Route API from interfaceID to interface ptr
        virtual cObject *getSource() const {return NULL; /*TODO: e->getSource();*/}
        virtual int getMetric() const {return e->getMetric();}
        virtual cObject *getProtocolData() const { return NULL; } // TODO:
        virtual void setProtocolData(cObject *protocolData) { } // TODO:
};

//TODO: currently there is no IPv6MulticastRoute class
///**
// *
// */
//class INET_API IPv6MulticastRouteAdapter : public IGenericMulticastRoute
//{
//    private:
//        IPv6MulticastRoute *e;
//    public:
//        IPv6MulticastRouteAdapter(IPv6MulticastRoute *route) {e = route;}
//        IPv6MulticastRoute *getIPv6Route() {return e;}
//        virtual IRoutingTable *getRoutingTable() const {return e->getRoutingTable()->asGeneric();}
//
//        virtual void setEnabled(bool enabled) {/*TODO: e->setEnabled(enabled);*/}
//        virtual void setOrigin(const Address& origin) {e->setOrigin(origin.toIPv6());}
//        virtual void setPrefixLength(int len) {e->setOriginNetmask(IPv6Address::makeNetmask(len));} //TODO inconsistent naming
//        virtual void setMulticastGroup(const Address& group) {e->setMulticastGroup(group.toIPv6());}
//        virtual void setParent(InterfaceEntry *ie) {e->setParent(ie);}
//        virtual bool addChild(InterfaceEntry *ie, bool isLeaf) {return e->addChild(ie, isLeaf);}
//        virtual bool removeChild(InterfaceEntry *ie) {return e->removeChild(ie);}
//        virtual void setSource(cObject *source) {/*TODO: e->setSource(source);*/}
//        virtual void setMetric(int metric) {e->setMetric(metric);}
//
//        virtual bool isEnabled() const {return true; /*TODO: e->isEnabled();*/}
//        virtual bool isExpired() const {return !e->isValid();}  //TODO rename IPv6 method
//        virtual Address getOrigin() const {return e->getOrigin();}
//        virtual int getPrefixLength() const {return e->getOriginNetmask().getNetmaskLength();} //TODO inconsistent naming
//        virtual Address getMulticastGroup() const {return e->getMulticastGroup();}
//        virtual InterfaceEntry *getParent() const {return e->getParent();}
//        virtual int getNumChildren() const {return e->getChildren().size();}
//        virtual InterfaceEntry *getChild(int i) const {return e->getChildren()[i]->getInterface();}  //XXX impedance mismatch
//        virtual bool getChildIsLeaf(int i) const {return e->getChildren()[i]->isLeaf();}
//        virtual cObject *getSource() const {return NULL; /*TODO: e->getSource();*/}
//        virtual int getMetric() const {return e->getMetric();}
//};

#endif

