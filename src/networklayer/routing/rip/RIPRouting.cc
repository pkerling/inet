//
// Copyright (C) 2013 Opensim Ltd.
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

#include <algorithm>

#include "InterfaceTableAccess.h"
#include "IPvXAddress.h" // XXX temporarily
#include "UDP.h"

#include "RIPPacket_m.h"
#include "RIPRouting.h"

Define_Module(RIPRouting);

// XXX temporarily
inline Address address(const IPvXAddress &addr)
{
    return addr.isIPv6() ? Address(addr.get6()) : Address(addr.get4());
}

// XXX temporarily
inline IPvXAddress ipvxAddress(const Address &addr)
{
    return !addr.toIPv6().isUnspecified() ? IPvXAddress(addr.toIPv6()) : IPvXAddress(addr.toIPv4());
}

// XXX temporarily
inline Address netmask(const Address &addrType, int prefixLength)
{
    return IPv4Address::makeNetmask(prefixLength); // XXX IPv4 only
}

bool RIPRouting::isNeighbour(const Address &address)
{
    return true; // TODO
}

bool RIPRouting::isOwnAddress(const Address &address)
{
    return false; // TODO
}

/**
 * Protocol specific data of RIP routes.
 */
struct RIPRouteData : public cObject
{
    bool routeChanged;
    simtime_t expiryTime;
    simtime_t purgeTime;
};

RIPRouting::RIPRouting()
    : ift(NULL), rt(NULL), updateTimer(NULL), triggeredUpdateTimer(NULL)
{
}

RIPRouting::~RIPRouting()
{
    delete updateTimer;
    delete triggeredUpdateTimer;
}

void RIPRouting::initialize(int stage)
{
    if (stage == 0) {
        usePoisonedSplitHorizon = par("usePoisonedSplitHorizon");
        const char *routingTableName = par("routingTableName");
        ift = InterfaceTableAccess().get();
        rt = ModuleAccess<IGenericRoutingTable>(routingTableName).get();
        updateTimer = new cMessage("RIP-timer");
        triggeredUpdateTimer = new cMessage("RIP-trigger");
        socket.setOutputGate(gate("udpOut"));
    }
    else if (stage == 3) {
        // initialize RIP interfaces
        // TODO now each multicast interface is added, with metric 1; use configuration instead
        // TODO update the list when the interface table changed
        for (int i = 0; i < ift->getNumInterfaces(); ++i)
        {
            InterfaceEntry *ie = ift->getInterface(i);
            if (ie->isMulticast())
                ripInterfaces.push_back(RIPInterfaceEntry(ie, 1 /*ie->ipv4Data()->getMetric()???*/));
        }
    }
    else if (stage == 4) { // interfaces and static routes are already initialized

        allRipRoutersGroup = Address(IPv4Address(RIP_IPV4_MULTICAST_ADDRESS)); // XXX set according to the type of the routing table

        socket.bind(RIP_UDP_PORT);
        //socket.joinMulticastGroup(allRipRoutersGroup); // XXX

        sendInitialRequests();

        // set update timer
        scheduleAt(RIP_UPDATE_INTERVAL, updateTimer);
    }
}

RIPRouting::RIPInterfaceEntry *RIPRouting::findInterfaceEntryById(int interfaceId)
{
    for (InterfaceVector::iterator it = ripInterfaces.begin(); it != ripInterfaces.end(); ++it)
        if (it->ie->getInterfaceId() == interfaceId)
            return &(*it);
    return NULL;
}

void RIPRouting::sendInitialRequests()
{
    for (InterfaceVector::iterator it = ripInterfaces.begin(); it != ripInterfaces.end(); ++it)
    {
        RIPPacket *packet = new RIPPacket();
        packet->setCommand(RIP_REQUEST);
        packet->setEntryArraySize(1);
        RIPEntry &entry = packet->getEntry(0);
        entry.addressFamilyId = RIP_AF_NONE;
        entry.metric = RIP_INFINITE_METRIC;
        sendPacket(packet, allRipRoutersGroup, RIP_UDP_PORT, it->ie);
    }
}

void RIPRouting::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage())
    {
        if (msg == updateTimer)
        {
            processRegularUpdate();
            scheduleAt(simTime() + RIP_UPDATE_INTERVAL, msg);
        }
        else if (msg == triggeredUpdateTimer)
        {
            processTriggeredUpdate();
        }
    }
    else if (msg->getKind() == UDP_I_DATA)
    {
        RIPPacket *packet = check_and_cast<RIPPacket*>(msg);
        unsigned char command = packet->getCommand();
        if (command == RIP_REQUEST)
            processRequest(packet);
        else if (command == RIP_RESPONSE)
            processResponse(packet);
        else
            delete packet;
    }
    else if (msg->getKind() == UDP_I_ERROR)
    {
        EV << "Ignoring UDP error report\n";
        delete msg;
    }
}

void RIPRouting::processRegularUpdate()
{
    for (InterfaceVector::iterator it = ripInterfaces.begin(); it != ripInterfaces.end(); ++it)
    {
        sendRoutes(allRipRoutersGroup, RIP_UDP_PORT, it->ie, false);
    }
}

void RIPRouting::processTriggeredUpdate()
{
    for (InterfaceVector::iterator it = ripInterfaces.begin(); it != ripInterfaces.end(); ++it)
    {
        sendRoutes(allRipRoutersGroup, RIP_UDP_PORT, it->ie, true);
    }

    // clear routeChanged flags
    int numRoutes = rt->getNumRoutes();
    for (int i = 0; i < numRoutes; ++i)
    {
        RIPRouteData *ripData = dynamic_cast<RIPRouteData*>(rt->getRoute(i)->getProtocolData());
        if (ripData)
            ripData->routeChanged = false;
    }
}

// RFC 2453 3.9.1
void RIPRouting::processRequest(RIPPacket *packet)
{
    int numEntries = packet->getEntryArraySize();
    if (numEntries == 0)
    {
        delete packet;
        return;
    }

    UDPDataIndication *ctrlInfo = check_and_cast<UDPDataIndication*>(packet->removeControlInfo());
    for (int i = 0; i < numEntries; ++i)
    {
        RIPEntry &entry = packet->getEntry(i);
        switch (entry.addressFamilyId)
        {
            case RIP_AF_NONE:
                if (numEntries == 0 && entry.metric == RIP_INFINITE_METRIC)
                {
                    InterfaceEntry *ie = ift->getInterfaceById(ctrlInfo->getInterfaceId());
                    sendRoutes(address(ctrlInfo->getSrcAddr()), ctrlInfo->getSrcPort(), ie, false);
                    delete ctrlInfo;
                    delete packet;
                    return;
                }
                break;
            case RIP_AF_AUTH:
                // TODO ?
                break;
            case RIP_AF_INET:
                IGenericRoute *route = rt->findBestMatchingRoute(entry.address);
                // TODO should we check route source here? if it is not a RIP route, what ensures that metric < 16?
                if (route)
                {
                    entry.nextHop = route->getNextHop();
                    entry.subnetMask = netmask(entry.address, route->getPrefixLength());
                    entry.metric = route->getMetric();
                    entry.routeTag = 0; // TODO
                }
                else
                {
                    entry.metric = RIP_INFINITE_METRIC;
                }
                break;
        }
    }

    packet->setCommand(RIP_RESPONSE);
    socket.sendTo(packet, ctrlInfo->getSrcAddr(), ctrlInfo->getSrcPort());

    delete ctrlInfo;
}

/**
 * Called by regular routing updates (every 30 sec) with changedOnly=false,
 * and triggered updates by changedOnly=true.
 */
// XXX set packetLength
void RIPRouting::sendRoutes(const Address &address, int port, InterfaceEntry *ie, bool changedOnly)
{
    int numRoutes = rt->getNumRoutes();
    RIPPacket *packet = new RIPPacket();
    packet->setCommand(RIP_RESPONSE);
    packet->setEntryArraySize(MAX_RIP_ENTRIES);
    int k = 0; // index into RIP entries

    for (int i = 0; i < numRoutes; ++i)
    {
        IGenericRoute *route = rt->getRoute(i);
        RIPRouteData *ripData = dynamic_cast<RIPRouteData *>(route->getProtocolData());

        if (changedOnly && (ripData == NULL || !ripData->routeChanged))
            continue;

        // Split Horizon check
        int metric = route->getMetric();
        if (route->getInterface() == ie)
        {
            if (!usePoisonedSplitHorizon)
                continue;
            else
                metric = RIP_INFINITE_METRIC;
        }

        // fill next entry
        RIPEntry &entry = packet->getEntry(k++);
        entry.addressFamilyId = RIP_AF_INET;
        entry.address = route->getDestination();
        entry.subnetMask = netmask(entry.address, route->getPrefixLength());
        entry.nextHop = route->getNextHop();
        entry.routeTag = 0; // TODO ?
        entry.metric = metric;

        // if packet is full, then send it and allocate a new one
        if (k >= MAX_RIP_ENTRIES)
        {
            sendPacket(packet, address, port, ie);
            packet = new RIPPacket();
            packet->setCommand(RIP_RESPONSE);
            packet->setEntryArraySize(MAX_RIP_ENTRIES);
            k = 0;
        }
    }

    // send last packet if it has entries
    if (k > 0)
    {
        packet->setEntryArraySize(k);
        sendPacket(packet, address, port, ie);
    }
    else
        delete packet;
}

/**
 * 1. validate packet
 * 2. for each entry:
 *      metric = MIN(p.metric + cost of if it arrived at, infinity)
 *      if there is no route for the dest address:
 *        add new route to the routing table unless the metric is infinity
 *      else:
 *        if received from the route.gateway
 *          reinitialize timeout
 *        if (received from route.gateway AND route.metric != metric) OR metric < route.metric
 *          updateRoute(route)
 */
void RIPRouting::processResponse(RIPPacket *packet)
{
    bool isValid = isValidResponse(packet);
    if (!isValid)
    {
        EV << "RIP: dropping invalid response\n";
        delete packet;
        return;
    }

    UDPDataIndication *ctrlInfo = check_and_cast<UDPDataIndication*>(packet->removeControlInfo());
    RIPInterfaceEntry *incomingIe = findInterfaceEntryById(ctrlInfo->getInterfaceId());
    if (!incomingIe)
    {
        delete packet;
        return;
    }

    int numEntries = packet->getEntryArraySize();
    for (int i = 0; i < numEntries; ++i) {
        RIPEntry &entry = packet->getEntry(i);
        int metric = std::min((int)entry.metric + incomingIe->metric, RIP_INFINITE_METRIC);
        Address nextHop = entry.nextHop.isUnspecified() ? address(ctrlInfo->getSrcAddr()) : entry.nextHop;

        IGenericRoute *route = findRoute(entry.address, entry.subnetMask);
        if (route)
        {
            if (route->getNextHop() == nextHop)
            {
                RIPRouteData *ripData = dynamic_cast<RIPRouteData*>(route->getProtocolData());
                if (ripData)
                    ripData->expiryTime = simTime() + RIP_ROUTE_EXPIRY_TIME;
            }
            if ((route->getNextHop() == nextHop && route->getMetric() != metric) || metric < route->getMetric())
                updateRoute(route, nextHop, metric);
        }
        else
        {
            if (metric != RIP_INFINITE_METRIC)
                addRoute(entry.address, entry.subnetMask, nextHop, metric);
        }
    }

    delete packet;
}

bool RIPRouting::isValidResponse(RIPPacket *packet)
{
    UDPDataIndication *ctrlInfo = check_and_cast<UDPDataIndication*>(packet->getControlInfo());

    // check that received from RIP_UDP_PORT
    if (ctrlInfo->getSrcPort() != RIP_UDP_PORT)
    {
        EV << "RIP: source port is not " << RIP_UDP_PORT << "\n";
        return false;
    }

    // check that source is on a directly connected network
    if (!isNeighbour(address(ctrlInfo->getSrcAddr())))
    {
        EV << "RIP: source is not directly connected " << ctrlInfo->getSrcAddr() << "\n";
        return false;
    }

    // check that it is not our response (received own multicast message)
    if (isOwnAddress(address(ctrlInfo->getSrcAddr())))
    {
        EV << "RIP: received own response\n";
        return false;
    }

    // validate entries
    int numEntries = packet->getEntryArraySize();
    for (int i = 0; i < numEntries; ++i)
    {
        RIPEntry &entry = packet->getEntry(i);

        // check that metric is in range [1,16]
        if (entry.metric < 1 || entry.metric > RIP_INFINITE_METRIC)
        {
            EV << "RIP: received metric is not in the [1," << RIP_INFINITE_METRIC << "] range.\n";
            return false;
        }

        // check that destination address is a unicast address
        // TODO exclude 0.x.x.x, 127.x.x.x too
        if (/*!entry.address.isUnicast()*/ entry.address.isBroadcast() || entry.address.isMulticast())
        {
            EV << "RIP: destination address of an entry is not unicast: " << entry.address << "\n";
            return false;
        }
    }

    return true;
}

// XXX should we return only RIP routes here? the result will be updated ...
IGenericRoute *RIPRouting::findRoute(const Address &destination, const Address &subnetMask)
{
    int numRoutes = rt->getNumRoutes();
    int prefixLength = subnetMask.getPrefixLength();
    for (int i = 0; i < numRoutes; ++i)
    {
        IGenericRoute *route = rt->getRoute(i);
        if (route->getDestination() == destination && route->getPrefixLength() == prefixLength)
            return route;
    }
    return NULL;
}

/**
 * RFC 2453 3.9.2:
 *
 * Adding a route to the routing table consists of:
 *
 * - Setting the destination address to the destination address in the RTE
 * - Setting the metric to the newly calculated metric
 * - Set the next hop address to be the address of the router from which
 *   the datagram came
 * - Initialize the timeout for the route.  If the garbage-collection
 *   timer is running for this route, stop it
 * - Set the route change flag
 * - Signal the output process to trigger an update
 */
void RIPRouting::addRoute(const Address &dest, const Address &subnetMask, const Address &nextHop, int metric)
{
    IGenericRoute *route = rt->createRoute();
    route->setSource(this);
    route->setDestination(dest);
    route->setPrefixLength(subnetMask.getPrefixLength());
    route->setNextHop(nextHop);
    route->setMetric(metric);
    RIPRouteData *ripData = new RIPRouteData();
    ripData->expiryTime = simTime() + RIP_ROUTE_EXPIRY_TIME;
    ripData->purgeTime = 0;
    ripData->routeChanged = true;
    route->setProtocolData(ripData);
    rt->addRoute(route);
    triggerUpdate();
}

/**
 * RFC 2453 3.9.2:
 *
 * Do the following actions:
 *
 *  - Adopt the route from the datagram (i.e., put the new metric in and
 *    adjust the next hop address, if necessary).
 *  - Set the route change flag and signal the output process to trigger
 *    an update
 *  - If the new metric is infinity, start the deletion process
 *    (described above); otherwise, re-initialize the timeout
 */
void RIPRouting::updateRoute(IGenericRoute *route, const Address &nextHop, int metric)
{
    route->setNextHop(nextHop);
    route->setMetric(metric);
    RIPRouteData *ripData = dynamic_cast<RIPRouteData*>(route->getProtocolData());
    if (!ripData)
        route->setProtocolData(ripData = new RIPRouteData());
    ripData->routeChanged = true;

    triggerUpdate();

    if (metric == RIP_INFINITE_METRIC)
        invalidateRoute(route);
    else {
        ripData->expiryTime = simTime() + RIP_ROUTE_EXPIRY_TIME;
        ripData->purgeTime = 0;
    }
}

void RIPRouting::triggerUpdate()
{
    if (!triggeredUpdateTimer->isScheduled())
    {
        double delay = uniform(RIP_TRIGGERED_UPDATE_DELAY_MIN, RIP_TRIGGERED_UPDATE_DELAY_MAX);
        scheduleAt(simTime() + delay, triggeredUpdateTimer);
    }
}

IGenericRoute *RIPRouting::checkRoute(IGenericRoute *route)
{
    simtime_t now = simTime();
    RIPRouteData *ripData = dynamic_cast<RIPRouteData*>(route->getProtocolData());
    if (ripData)
    {
        if (ripData->purgeTime > 0 && now > ripData->purgeTime)
        {
            purgeRoute(route);
            return NULL;
        }
        if (now > ripData->expiryTime)
        {
            invalidateRoute(route);
            return NULL;
        }
    }
    return route;
}

/*
 * Called when the timeout expires, or a metric is set to 16 because an update received from the current router.
 * It will
 * - set purgeTime to expiryTime + 120s
 * - set metric of the route to 16 (infinity)
 * - set routeChangeFlag
 * - signal the output process to trigger a response
 */
void RIPRouting::invalidateRoute(IGenericRoute *route)
{
    RIPRouteData *ripData = dynamic_cast<RIPRouteData*>(route->getProtocolData());
    if (ripData)
    {
        route->setMetric(RIP_INFINITE_METRIC);
        route->setEnabled(false);
        ripData->purgeTime = ripData->expiryTime + RIP_ROUTE_PURGE_TIME;
        ripData->routeChanged = true;
        triggerUpdate();
    }
}

void RIPRouting::purgeRoute(IGenericRoute *route)
{
    // XXX should set isExpired() to true, and let rt->purge() to do the work
    rt->deleteRoute(route);
}

void RIPRouting::sendPacket(RIPPacket *packet, const Address &address, int port, InterfaceEntry *ie)
{
    if (address.isMulticast())
        socket.setMulticastOutputInterface(ie->getInterfaceId());
    socket.sendTo(packet, ipvxAddress(address), port);
}
