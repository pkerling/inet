//
// This program is property of its copyright holder. All rights reserved.
// 

#include "GPSR.h"

Define_Module(GPSR);

#define GPSR_EV EV << "GPSR at " << getHostName() << " "

GPSR::GPSR() {
    notificationBoard = NULL;
    addressPolicy = NULL;
    globalLocationTable = NULL;
}

GPSR::~GPSR() {
}

//
// module interface
//

void GPSR::initialize(int stage) {
    if (stage == 0) {
        // context parameters
        networkProtocolModuleName = par("networkProtocolModuleName");
        // context
        networkProtocol = check_and_cast<INetfilter *>(findModuleWhereverInNode(networkProtocolModuleName, this));
    }
    else if (stage == 4) {
        addressPolicy = getSelfAddress().getAddressPolicy();
        // hook to network protocol
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
    throw cRuntimeError("Unknown self message");
}

void GPSR::processMessage(cMessage * message) {
    throw cRuntimeError("Unknown message");
}

//
// handling positions
//

Coord GPSR::getLocation(const Address & address) {
    return globalLocationTable->getLocation(address);
}

Coord GPSR::findNextHop(const Address & destination) {
    return Coord();
}

//
// handling addresses
//

std::string GPSR::getHostName() {
    return getParentModule()->getFullName();
}

Address GPSR::getSelfAddress() {
    return Address(); // TODO: routingTable->getRouterId();
}


//
// generic network protocol
//

bool GPSR::isGPSRDatagram(INetworkDatagram * datagram) {
    cPacket * packet = dynamic_cast<cPacket *>(datagram);
    INetworkProtocolControlInfo * networkProtocolControlInfo = dynamic_cast<INetworkProtocolControlInfo *>(packet->getControlInfo());
    return false; //return networkProtocolControlInfo && networkProtocolControlInfo->getProtocol() == IP_PROT_MANET;
}

INetfilter::IHook::Result GPSR::ensureRouteForDatagram(INetworkDatagram * datagram) {
    const Address & source = datagram->getSourceAddress();
    const Address & destination = datagram->getDestinationAddress();
//    if (destination.isMulticast() || destination.isBroadcast() || routingTable->isLocalAddress(destination) || isGPSRDatagram(datagram))
        return ACCEPT;
//    else {
//        GPSR_EV << "Finding route: source = " << source << ", destination = " << destination << endl;
//        return ACCEPT;
//    }
}

//
// notifications
//

void GPSR::receiveChangeNotification(int category, const cObject *details) {
    Enter_Method("receiveChangeNotification");
    if (category == NF_LINK_BREAK) {
        GPSR_EV << "Received link break" << endl;
    }
}
