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
#include "IPv4RoutingTable.h" // XXX temporarily
#include "NotificationBoard.h"
#include "NotifierConsts.h"
#include "UDP.h"

#include "RIPPacket_m.h"
#include "RIPRouting.h"

Define_Module(RIPRouting);

// XXX temporarily
inline Address netmask(const Address &addrType, int prefixLength)
{
    return IPv4Address::makeNetmask(prefixLength); // XXX IPv4 only
}

inline int prefixLength(const Address &netmask)
{
    return netmask.getType() == Address::IPv4 ? netmask.toIPv4().getNetmaskLength() : 32; // XXX IPv4 only
}

bool RIPRouting::isNeighbour(const Address &address)
{
    return true; // TODO
}

bool RIPRouting::isOwnAddress(const Address &address)
{
    return false; // TODO
}

std::ostream& operator<<(std::ostream& os, const RIPRoute& e)
{
    os << e.info();
    return os;
}

std::string RIPRoute::info() const
{
    std::stringstream out;

    if (route)
    {
        const Address &dest = route->getDestination();
        int prefixLength = route->getPrefixLength();
        const Address &gateway = route->getNextHop();
        InterfaceEntry *interfacePtr = route->getInterface();
        out << "dest:"; if (dest.isUnspecified()) out << "*  "; else out << dest << "  ";
        out << "prefix:" << prefixLength << "  ";
        out << "gw:"; if (gateway.isUnspecified()) out << "*  "; else out << gateway << "  ";
        out << "metric:" << metric << " ";
//        out << "if:"; if (!interfacePtr) out << "*  "; else out << interfacePtr->getName() << "(" << interfacePtr->ipv4Data()->getIPAddress() << ")  ";
        switch (type)
        {
            case RIP_ROUTE_INTERFACE: out << "INTERFACE"; break;
            case RIP_ROUTE_STATIC: out << "STATIC"; break;
            case RIP_ROUTE_DEFAULT: out << "DEFAULT"; break;
            case RIP_ROUTE_RTE: out << "RTE"; break;
            case RIP_ROUTE_REDISTRIBUTE: out << "REDISTRIBUTE"; break;
        }
    }

    return out.str();
}

RIPRouting::RIPRouting()
    : ift(NULL), rt(NULL), updateTimer(NULL), triggeredUpdateTimer(NULL)
{
}

RIPRouting::~RIPRouting()
{
    for (RouteVector::iterator it = ripRoutes.begin(); it != ripRoutes.end(); ++it)
        delete *it;
    delete updateTimer;
    delete triggeredUpdateTimer;
}

void RIPRouting::initialize(int stage)
{
    if (stage == 0) {
        usePoisonedSplitHorizon = par("usePoisonedSplitHorizon");
        const char *routingTableName = par("routingTableName");
        ift = InterfaceTableAccess().get();
        //rt = ModuleAccess<IRoutingTable>(routingTableName).get();
        rt = ModuleAccess<IPv4RoutingTable>(routingTableName).get()->asGeneric(); // XXX
        allRipRoutersGroup = Address(IPv4Address(RIP_IPV4_MULTICAST_ADDRESS)); // XXX set according to the type of the routing table
        updateTimer = new cMessage("RIP-timer");
        triggeredUpdateTimer = new cMessage("RIP-trigger");
        socket.setOutputGate(gate("udpOut"));

        //WATCH(ripInterfaces);
        WATCH_PTRVECTOR(ripRoutes);
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

        // add rip routes from configured interface routes (TODO add static routes too)
        for (int i = 0; i < rt->getNumRoutes(); ++i)
        {
            IRoute *route = rt->getRoute(i);
            InterfaceEntry *ie = dynamic_cast<InterfaceEntry*>(route->getSource());
            if (ie && !ie->isLoopback())
            {
                RIPInterfaceEntry *ripIe = findInterfaceEntryById(ie->getInterfaceId());
                RIPRoute *ripRoute = new RIPRoute(route, RIPRoute::RIP_ROUTE_INTERFACE, ripIe ? ripIe->metric : 1);
                ripRoute->ie = ie;
                ripRoutes.push_back(ripRoute);
            }
        }

        socket.setMulticastLoop(false);
        socket.bind(RIP_UDP_PORT);
        for (InterfaceVector::iterator it = ripInterfaces.begin(); it != ripInterfaces.end(); ++it)
            socket.joinMulticastGroup(allRipRoutersGroup, it->ie->getInterfaceId());

        // XXX subscribe according to the address type
        NotificationBoard *nb = NotificationBoardAccess().get();
        nb->subscribe(this, NF_IPv4_ROUTE_DELETED);
        nb->subscribe(this, NF_IPv4_ROUTE_ADDED);

        sendInitialRequests();

        // set update timer
        scheduleAt(RIP_UPDATE_INTERVAL, updateTimer);
    }
}

// keep our data structures consistent with other modules
void RIPRouting::receiveChangeNotification(int category, const cObject *details)
{
//    if (simulation.getContextType()==CTX_INITIALIZE)
//        return;  // ignore notifications during initialize

    if (category == NF_IPv4_ROUTE_DELETED)
    {
        IRoute *route = check_and_cast<IPv4Route*>(details)->asGeneric();
        for (RouteVector::iterator it = ripRoutes.begin(); it != ripRoutes.end(); ++it)
            if ((*it)->route == route)
                (*it)->route = NULL;
    }
    else if (category == NF_IPv4_ROUTE_ADDED)
    {
        IRoute *route = check_and_cast<IPv4Route*>(details)->asGeneric();
        InterfaceEntry *ie = dynamic_cast<InterfaceEntry*>(route->getSource());
        if (ie)
        {
            RIPRoute *ripRoute = findInterfaceRoute(ie);
            if (ripRoute)
                ripRoute->route = route;
            else
                /*???*/;
        }
    }
}

RIPRoute *RIPRouting::findInterfaceRoute(InterfaceEntry *ie)
{
    for (RouteVector::iterator it = ripRoutes.begin(); it != ripRoutes.end(); ++it)
        if ((*it)->type == RIPRoute::RIP_ROUTE_INTERFACE && (*it)->ie == ie)
            return *it;
    return NULL;
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
        RIPPacket *packet = new RIPPacket("RIP request");
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

    // clear changed flags
    for (RouteVector::iterator it = ripRoutes.begin(); it != ripRoutes.end(); ++it)
    {
        (*it)->changed = false;
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
                if (numEntries == 1 && entry.metric == RIP_INFINITE_METRIC)
                {
                    InterfaceEntry *ie = ift->getInterfaceById(ctrlInfo->getInterfaceId());
                    sendRoutes(ctrlInfo->getSrcAddr(), ctrlInfo->getSrcPort(), ie, false);
                    delete ctrlInfo;
                    delete packet;
                    return;
                }
                break;
            case RIP_AF_AUTH:
                // TODO ?
                break;
            case RIP_AF_INET:
                IRoute *route = rt->findBestMatchingRoute(entry.address);
                // XXX should we check route source here? if it is not a RIP route, what ensures that metric < 16?
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
    packet->setName("RIP response");
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
    RIPPacket *packet = new RIPPacket("RIP response");
    packet->setCommand(RIP_RESPONSE);
    packet->setEntryArraySize(MAX_RIP_ENTRIES);
    int k = 0; // index into RIP entries

    for (RouteVector::iterator it = ripRoutes.begin(); it != ripRoutes.end(); ++it)
    {
        RIPRoute *ripRoute = *it;
        IRoute *route = ripRoute->route;
        ASSERT(route != NULL);

        if (changedOnly && !ripRoute->changed)
            continue;

        // Split Horizon check
        int metric = ripRoute->metric;
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
        entry.routeTag = ripRoute->tag;
        entry.metric = metric;

        // if packet is full, then send it and allocate a new one
        if (k >= MAX_RIP_ENTRIES)
        {
            sendPacket(packet, address, port, ie);
            packet = new RIPPacket("RIP response");
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
        Address from = ctrlInfo->getSrcAddr();
        Address nextHop = entry.nextHop.isUnspecified() ? from : entry.nextHop;

        RIPRoute *ripRoute = findRoute(entry.address, entry.subnetMask);
        if (ripRoute)
        {
            if (ripRoute->from == from)
                ripRoute->expiryTime = simTime() + RIP_ROUTE_EXPIRY_TIME;
            if ((ripRoute->from == from && ripRoute->metric != metric) || metric < ripRoute->metric)
                updateRoute(ripRoute, nextHop, metric, from);
        }
        else
        {
            if (metric != RIP_INFINITE_METRIC)
                addRoute(entry.address, entry.subnetMask, incomingIe->ie, nextHop, metric, from);
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
    if (!isNeighbour(ctrlInfo->getSrcAddr()))
    {
        EV << "RIP: source is not directly connected " << ctrlInfo->getSrcAddr() << "\n";
        return false;
    }

    // check that it is not our response (received own multicast message)
    if (isOwnAddress(ctrlInfo->getSrcAddr()))
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

RIPRoute *RIPRouting::findRoute(const Address &destination, const Address &subnetMask)
{
    int prefixLen = prefixLength(subnetMask);
    for (RouteVector::iterator it = ripRoutes.begin(); it != ripRoutes.end(); ++it)
    {
        IRoute *route = (*it)->route;
        if (route && route->getDestination() == destination && route->getPrefixLength() == prefixLen)
            return *it;
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
void RIPRouting::addRoute(const Address &dest, const Address &subnetMask, InterfaceEntry *ie, const Address &nextHop, int metric, const Address &from)
{
    IRoute *route = rt->createRoute();
    route->setSource(this);
    route->setDestination(dest);
    route->setPrefixLength(prefixLength(subnetMask));
    route->setInterface(ie);
    route->setNextHop(nextHop);
    route->setMetric(metric);
    RIPRoute *ripRoute = new RIPRoute(route, RIPRoute::RIP_ROUTE_RTE/*XXX*/, metric);
    ripRoute->from = from;
    ripRoute->expiryTime = simTime() + RIP_ROUTE_EXPIRY_TIME;
    ripRoute->purgeTime = 0;
    ripRoute->changed = true;
    route->setProtocolData(ripRoute);
    rt->addRoute(route);
    ripRoutes.push_back(ripRoute);
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
void RIPRouting::updateRoute(RIPRoute *ripRoute, const Address &nextHop, int metric, const Address &from)
{
    if (ripRoute->route)
    {
        ripRoute->route->setNextHop(nextHop);
        ripRoute->route->setMetric(metric);
    }
    else
    {
        // TODO add new route
    }

    ripRoute->type = RIPRoute::RIP_ROUTE_RTE; // XXX?
    ripRoute->from = from;
    ripRoute->metric = metric;
    ripRoute->changed = true;

    triggerUpdate();

    if (metric == RIP_INFINITE_METRIC)
        invalidateRoute(ripRoute);
    else {
        ripRoute->expiryTime = simTime() + RIP_ROUTE_EXPIRY_TIME;
        ripRoute->purgeTime = 0;
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

RIPRoute *RIPRouting::checkRoute(RIPRoute *route)
{
    simtime_t now = simTime();
    if (route->purgeTime > 0 && now > route->purgeTime)
    {
        purgeRoute(route);
        return NULL;
    }
    if (now > route->expiryTime)
    {
        invalidateRoute(route);
        return NULL;
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
void RIPRouting::invalidateRoute(RIPRoute *ripRoute)
{
    ripRoute->route->setMetric(RIP_INFINITE_METRIC);
    ripRoute->route->setEnabled(false);
    ripRoute->metric = RIP_INFINITE_METRIC;
    ripRoute->purgeTime = ripRoute->expiryTime + RIP_ROUTE_PURGE_TIME;
    ripRoute->changed = true;
    triggerUpdate();
}

void RIPRouting::purgeRoute(RIPRoute *ripRoute)
{
    // XXX should set isExpired() to true, and let rt->purge() to do the work
    rt->deleteRoute(ripRoute->route);
}

void RIPRouting::sendPacket(RIPPacket *packet, const Address &address, int port, InterfaceEntry *ie)
{
    if (address.isMulticast())
        socket.setMulticastOutputInterface(ie->getInterfaceId());
    socket.sendTo(packet, address, port);
}
