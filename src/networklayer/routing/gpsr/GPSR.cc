//
// Copyright (C) 2004 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

#include "GPSR.h"
#include "NotificationBoard.h"
#include "InterfaceTableAccess.h"
#include "IPProtocolId_m.h"

Define_Module(GPSR);

#define GPSR_EV EV << "GPSR at " << getHostName() << " "

// KLUDGE:
PositionTable GPSR::globalPositionTable;

GPSR::GPSR() {
    notificationBoard = NULL;
    addressPolicy = NULL;
    beaconTimer = NULL;
}

GPSR::~GPSR() {
    cancelAndDelete(beaconTimer);
    cancelAndDelete(purgeNeighborsTimer);
}

//
// module interface
//

void GPSR::initialize(int stage) {
    if (stage == 0) {
        // context parameters
        routingTableModuleName = par("routingTableModuleName");
        networkProtocolModuleName = par("networkProtocolModuleName");
        // gpsr parameters
        interfaces = par("interfaces");
        beaconInterval = par("beaconInterval");
        maxJitter = par("maxJitter");
        neighborValidityInterval = par("neighborValidityInterval");
        // context
        notificationBoard = NotificationBoardAccess().get(this);
        interfaceTable = InterfaceTableAccess().get(this);
        mobility = check_and_cast<IMobility *>(findModuleWhereverInNode("mobility", this));
        // KLUDGE: simplify this when IPv4RoutingTable implements IRoutingTable
        routingTable = check_and_cast<IRoutingTable *>(findModuleWhereverInNode(routingTableModuleName, this));
        networkProtocol = check_and_cast<INetfilter *>(findModuleWhereverInNode(networkProtocolModuleName, this));
        // internal
        beaconTimer = new cMessage("BeaconTimer");
        purgeNeighborsTimer = new cMessage("PurgeNeighborsTimer");
        scheduleBeaconTimer();
        schedulePurgeNeighborsTimer();
    }
    else if (stage == 4) {
        notificationBoard->subscribe(this, NF_LINK_BREAK);
        addressPolicy = getSelfAddress().getAddressPolicy();
        // join multicast groups
        cPatternMatcher interfaceMatcher(interfaces, false, true, false);
        for (int i = 0; i < interfaceTable->getNumInterfaces(); i++) {
            InterfaceEntry * interfaceEntry = interfaceTable->getInterface(i);
            if (interfaceEntry->isMulticast() && interfaceMatcher.matches(interfaceEntry->getName()))
                // Most AODVv2 messages are sent with the IP destination address set to the link-local
                // multicast address LL-MANET-Routers [RFC5498] unless otherwise specified. Therefore,
                // all AODVv2 routers MUST subscribe to LL-MANET-Routers [RFC5498] to receiving AODVv2 messages.
                addressPolicy->joinMulticastGroup(interfaceEntry, addressPolicy->getLinkLocalManetRoutersMulticastAddress());
        }
        // hook to netfilter
        networkProtocol->registerHook(0, this);
    }
}

void GPSR::handleMessage(cMessage * message) {
    if (message->isSelfMessage())
        processSelfMessage(message);
    else
        processMessage(message);
}

//
// handling messages
//

void GPSR::processSelfMessage(cMessage * message) {
    if (message == beaconTimer)
        processBeaconTimer();
    else if (message == purgeNeighborsTimer)
        processPurgeNeighborsTimer();
    else
        throw cRuntimeError("Unknown self message");
}

void GPSR::processMessage(cMessage * message) {
    if (dynamic_cast<UDPPacket *>(message))
        processUDPPacket((UDPPacket *)message);
    else
        throw cRuntimeError("Unknown message");
}

//
// beacon timers
//

void GPSR::scheduleBeaconTimer() {
    GPSR_EV << "Scheduling beacon timer" << endl;
    scheduleAt(simTime() + beaconInterval, beaconTimer);
}

void GPSR::processBeaconTimer() {
    GPSR_EV << "Processing beacon timer" << endl;
    sendBeaconPacket(createBeaconPacket(), uniform(0, maxJitter).dbl());
    scheduleBeaconTimer();
    schedulePurgeNeighborsTimer();
    // KLUDGE:
    globalPositionTable.setPosition(getSelfAddress(), mobility->getCurrentPosition());
}

//
// handling purge neighbors timers
//

void GPSR::schedulePurgeNeighborsTimer() {
    GPSR_EV << "Scheduling purge neighbors timer" << endl;
    simtime_t nextExpiration = getNextNeighborExpiration();
    if (nextExpiration == SimTime::getMaxTime()) {
        if (purgeNeighborsTimer->isScheduled())
            cancelEvent(purgeNeighborsTimer);
    }
    else {
        if (!purgeNeighborsTimer->isScheduled())
            scheduleAt(nextExpiration, purgeNeighborsTimer);
        else {
            if (purgeNeighborsTimer->getArrivalTime() != nextExpiration) {
                cancelEvent(purgeNeighborsTimer);
                scheduleAt(nextExpiration, purgeNeighborsTimer);
            }
        }
    }
}

void GPSR::processPurgeNeighborsTimer() {
    GPSR_EV << "Processing purge neighbors timer" << endl;
    purgeNeighbors();
    schedulePurgeNeighborsTimer();
}

//
// handling UDP packets
//

void GPSR::sendUDPPacket(UDPPacket * packet, double delay) {
    if (delay == 0)
        send(packet, "ipOut");
    else
        sendDelayed(packet, delay, "ipOut");
}

void GPSR::processUDPPacket(UDPPacket * packet) {
    cPacket * encapsulatedPacket = packet->decapsulate();
    if (dynamic_cast<GPSRBeacon *>(encapsulatedPacket)) {
        GPSRBeacon * beacon = (GPSRBeacon *)encapsulatedPacket;
        beacon->setControlInfo(packet->removeControlInfo());
        processBeaconPacket(beacon);
    }
    else
        throw cRuntimeError("Unknown UDP packet");
    delete packet;
}

//
// handling beacons
//

GPSRBeacon * GPSR::createBeaconPacket() {
    GPSRBeacon * beacon = new GPSRBeacon();
    beacon->setAddress(getSelfAddress());
    beacon->setPosition(mobility->getCurrentPosition());
    return beacon;
}

void GPSR::sendBeaconPacket(GPSRBeacon * beacon, double delay) {
    GPSR_EV << "Sending beacon packet: address = " << beacon->getAddress() << " position = " << beacon->getPosition() << endl;
    INetworkProtocolControlInfo * networkProtocolControlInfo = addressPolicy->createNetworkProtocolControlInfo();
    networkProtocolControlInfo->setProtocol(IP_PROT_MANET);
    networkProtocolControlInfo->setHopLimit(255);
    networkProtocolControlInfo->setDestinationAddress(addressPolicy->getLinkLocalManetRoutersMulticastAddress());
    networkProtocolControlInfo->setSourceAddress(getSelfAddress());
    UDPPacket * udpPacket = new UDPPacket(beacon->getName());
    udpPacket->encapsulate(beacon);
    // In its default mode of operation, AODVv2 uses the UDP port 269 [RFC5498] to carry protocol packets.
    udpPacket->setSourcePort(GPSR_UDP_PORT);
    udpPacket->setDestinationPort(GPSR_UDP_PORT);
    udpPacket->setControlInfo(dynamic_cast<cObject *>(networkProtocolControlInfo));
    sendUDPPacket(udpPacket, delay);
}

void GPSR::processBeaconPacket(GPSRBeacon * beacon) {
    GPSR_EV << "Processing beacon packet: address = " << beacon->getAddress() << " position = " << beacon->getPosition() << endl;
    neighborPositionTable.setPosition(beacon->getAddress(), beacon->getPosition());
    delete beacon;
}

//
// position
//

Coord GPSR::getDestinationPosition(const Address & address) {
    // KLUDGE: implement registry protocol
    return globalPositionTable.getPosition(address);
}

Coord GPSR::getNeighborPosition(const Address & address) {
    return neighborPositionTable.getPosition(address);
}

//
// address
//

std::string GPSR::getHostName() {
    return getParentModule()->getFullName();
}

Address GPSR::getSelfAddress() {
    return routingTable->getRouterId();
}

//
// neighbor
//

simtime_t GPSR::getNextNeighborExpiration() {
    simtime_t oldestPosition = neighborPositionTable.getOldestPosition();
    if (oldestPosition == SimTime::getMaxTime())
        return oldestPosition;
    else
        return oldestPosition + neighborValidityInterval;
}

void GPSR::purgeNeighbors() {
    neighborPositionTable.removeOldPositions(simTime() - neighborValidityInterval);
}

//
// next hop
//

Address GPSR::findNextHop(const Address & destination) {
    Address bestNeighbor;
    Coord selfPosition = mobility->getCurrentPosition();
    Coord destinationPosition = getDestinationPosition(destination);
    double bestDistance = (destinationPosition - selfPosition).length();
    std::vector<Address> addresses = neighborPositionTable.getAddresses();
    for (std::vector<Address>::iterator it = addresses.begin(); it != addresses.end(); it++) {
        const Address & neighborAddress = *it;
        Coord neighborPosition = neighborPositionTable.getPosition(neighborAddress);
        double neighborDistance = (destinationPosition - neighborPosition).length();
        if (neighborDistance < bestDistance) {
            bestDistance = neighborDistance;
            bestNeighbor = neighborAddress;
        }
    }
    return bestNeighbor;
}

//
// netfilter
//

bool GPSR::isGPSRDatagram(INetworkDatagram * datagram) {
    cPacket * packet = dynamic_cast<cPacket *>(datagram);
    INetworkProtocolControlInfo * networkProtocolControlInfo = dynamic_cast<INetworkProtocolControlInfo *>(packet->getControlInfo());
    return networkProtocolControlInfo && networkProtocolControlInfo->getProtocol() == IP_PROT_MANET;
}

INetfilter::IHook::Result GPSR::routeDatagram(INetworkDatagram * datagram, const InterfaceEntry *& outputInterfaceEntry, Address & nextHop) {
    const Address & source = datagram->getSourceAddress();
    const Address & destination = datagram->getDestinationAddress();
    if (destination.isMulticast() || destination.isBroadcast() || routingTable->isLocalAddress(destination) || isGPSRDatagram(datagram))
        return ACCEPT;
    else {
        GPSR_EV << "Finding next hop: source = " << source << ", destination = " << destination << endl;
        nextHop = findNextHop(destination);
        if (nextHop.isUnspecified())
            GPSR_EV << "No  next hop found: source = " << source << ", destination = " << destination << endl;
        else {
            GPSR_EV << "Next hop found: source = " << source << ", destination = " << destination << ", nextHop: " << nextHop << endl;
            // KLUDGE:
            outputInterfaceEntry = interfaceTable->getInterface(1);
        }
        return ACCEPT;
    }
}

//
// notification
//

void GPSR::receiveChangeNotification(int category, const cObject *details) {
    Enter_Method("receiveChangeNotification");
    if (category == NF_LINK_BREAK) {
        GPSR_EV << "Received link break" << endl;
    }
}
