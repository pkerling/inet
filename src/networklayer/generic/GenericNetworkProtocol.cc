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


#include "GenericNetworkProtocol.h"
#include "GenericDatagram.h"
#include "GenericNetworkProtocolControlInfo.h"
#include "GenericRoute.h"
#include "GenericRoutingTable.h"
#include "GenericNetworkProtocolInterfaceData.h"

Define_Module(GenericNetworkProtocol);


void GenericNetworkProtocol::initialize()
{
    QueueBase::initialize();

    ift = InterfaceTableAccess().get();
    rt = check_and_cast<GenericRoutingTable *>(findModuleWhereverInNode("routingTable", this));

    queueOutGate = gate("queueOut");

    defaultHopLimit = par("hopLimit");
    mapping.parseProtocolMapping(par("protocolMapping"));

    numLocalDeliver = numDropped = numUnroutable = numForwarded = 0;

    WATCH(numLocalDeliver);
    WATCH(numDropped);
    WATCH(numUnroutable);
    WATCH(numForwarded);
}

void GenericNetworkProtocol::updateDisplayString()
{
    char buf[80] = "";
    if (numForwarded>0) sprintf(buf+strlen(buf), "fwd:%d ", numForwarded);
    if (numLocalDeliver>0) sprintf(buf+strlen(buf), "up:%d ", numLocalDeliver);
    if (numDropped>0) sprintf(buf+strlen(buf), "DROP:%d ", numDropped);
    if (numUnroutable>0) sprintf(buf+strlen(buf), "UNROUTABLE:%d ", numUnroutable);
    getDisplayString().setTagArg("t",0,buf);
}

void GenericNetworkProtocol::endService(cPacket *pk)
{
    if (pk->getArrivalGate()->isName("transportIn"))
    {
        handleMessageFromHL(pk);
    }
    else
    {
        GenericDatagram *dgram = check_and_cast<GenericDatagram *>(pk);
        handlePacketFromNetwork(dgram);
    }

    if (ev.isGUI())
        updateDisplayString();
}

InterfaceEntry *GenericNetworkProtocol::getSourceInterfaceFrom(cPacket *msg)
{
    cGate *g = msg->getArrivalGate();
    return g ? ift->getInterfaceByNetworkLayerGateIndex(g->getIndex()) : NULL;
}

void GenericNetworkProtocol::handlePacketFromNetwork(GenericDatagram *datagram)
{
    //
    // "Prerouting"
    //

    // check for header biterror
    if (datagram->hasBitError()) {
        //TODO discard
    }

    // remove control info
    delete datagram->removeControlInfo();

    // hop counter decrement; FIXME but not if it will be locally delivered
    datagram->setHopLimit(datagram->getHopLimit()-1);

    // route packet
    if (!datagram->getDestinationAddress().isMulticast())
        routePacket(datagram, NULL, false);
    else
        routeMulticastPacket(datagram, NULL, getSourceInterfaceFrom(datagram));
}

void GenericNetworkProtocol::handleMessageFromHL(cPacket *msg)
{
    // if no interface exists, do not send datagram
    if (ift->getNumInterfaces() == 0)
    {
        EV << "No interfaces exist, dropping packet\n";
        delete msg;
        return;
    }

    // encapsulate and send
    InterfaceEntry *destIE; // will be filled in by encapsulate()
    GenericDatagram *datagram = encapsulate(msg, destIE);

    if (datagramLocalOutHook(datagram, destIE) != IHook::ACCEPT)
        return;

    datagramLocalOut(datagram, destIE);
}

void GenericNetworkProtocol::routePacket(GenericDatagram *datagram, InterfaceEntry *destIE, bool fromHL)
{
    // TBD add option handling code here

    Address destAddr = datagram->getDestinationAddress();

    EV << "Routing datagram `" << datagram->getName() << "' with dest=" << destAddr << ": ";

    // check for local delivery
    if (rt->isLocalAddress(destAddr))
    {
        EV << "local delivery\n";
        if (datagram->getSourceAddress().isUnspecified())
            datagram->setSourceAddress(destAddr); // allows two apps on the same host to communicate
        numLocalDeliver++;
        reassembleAndDeliver(datagram);
        return;
    }

    // if datagram arrived from input gate and Generic_FORWARD is off, delete datagram
    if (!fromHL && !rt->isForwardingEnabled())
    {
        EV << "forwarding off, dropping packet\n";
        numDropped++;
        delete datagram;
        return;
    }

    Address nextHopAddr;

    // if output port was explicitly requested, use that, otherwise use GenericNetworkProtocol routing
    // TODO: see IPv4, using destIE here leaves nextHopAddre unspecified
//    if (destIE)
//    {
//        EV << "using manually specified output interface " << destIE->getName() << "\n";
//        // and nextHopAddr remains unspecified
//    }
//    else
//    {
        // use GenericNetworkProtocol routing (lookup in routing table)
        const GenericRoute *re = rt->findBestMatchingRoute(destAddr);

        // error handling: destination address does not exist in routing table:
        // notify ICMP, throw packet away and continue
        if (re==NULL)
        {
            EV << "unroutable, sending ICMP_DESTINATION_UNREACHABLE\n";
            numUnroutable++;
            delete datagram;
            return;
        }

        // extract interface and next-hop address from routing table entry
        destIE = re->getInterface();
        nextHopAddr = re->getNextHop();
//    }

    // set datagram source address if not yet set
    if (datagram->getSourceAddress().isUnspecified())
        datagram->setSourceAddress(destIE->getGenericNetworkProtocolData()->getAddress());

    // default: send datagram to fragmentation
    EV << "output interface is " << destIE->getName() << ", next-hop address: " << nextHopAddr << "\n";
    numForwarded++;

    //
    // fragment and send the packet
    //
    fragmentAndSend(datagram, destIE, nextHopAddr);
}

void GenericNetworkProtocol::routeMulticastPacket(GenericDatagram *datagram, InterfaceEntry *destIE, InterfaceEntry *fromIE)
{
    Address destAddr = datagram->getDestinationAddress();
    // if received from the network...
    if (fromIE!=NULL)
    {
        // check for local delivery
        if (rt->isLocalMulticastAddress(destAddr))
        {
            GenericDatagram *datagramCopy = (GenericDatagram *) datagram->dup();

            // FIXME code from the MPLS model: set packet dest address to routerId (???)
            datagramCopy->setDestinationAddress(rt->getRouterId());

            reassembleAndDeliver(datagramCopy);
        }
//
//        // don't forward if GenericNetworkProtocol forwarding is off
//        if (!rt->isGenericForwardingEnabled())
//        {
//            delete datagram;
//            return;
//        }
//
//        // don't forward if dest address is link-scope
//        if (destAddr.isLinkLocalMulticast())
//        {
//            delete datagram;
//            return;
//        }
    }
    else {
        //TODO
        for (int i=0; i<ift->getNumInterfaces(); ++i) {
            InterfaceEntry * destIE = ift->getInterface(i);
            if (!destIE->isLoopback())
                fragmentAndSend(datagram, destIE, datagram->getDestinationAddress());
        }
    }

//    Address destAddr = datagram->getDestinationAddress();
//    EV << "Routing multicast datagram `" << datagram->getName() << "' with dest=" << destAddr << "\n";
//
//    numMulticast++;
//
//    // DVMRP: process datagram only if sent locally or arrived on the shortest
//    // route (provided routing table already contains srcAddr); otherwise
//    // discard and continue.
//    InterfaceEntry *shortestPathIE = rt->getInterfaceForDestinationAddr(datagram->getSourceAddress());
//    if (fromIE!=NULL && shortestPathIE!=NULL && fromIE!=shortestPathIE)
//    {
//        // FIXME count dropped
//        EV << "Packet dropped.\n";
//        delete datagram;
//        return;
//    }
//
//    // if received from the network...
//    if (fromIE!=NULL)
//    {
//        // check for local delivery
//        if (rt->isLocalMulticastAddress(destAddr))
//        {
//            GenericDatagram *datagramCopy = (GenericDatagram *) datagram->dup();
//
//            // FIXME code from the MPLS model: set packet dest address to routerId (???)
//            datagramCopy->setDestinationAddress(rt->getRouterId());
//
//            reassembleAndDeliver(datagramCopy);
//        }
//
//        // don't forward if GenericNetworkProtocol forwarding is off
//        if (!rt->isGenericForwardingEnabled())
//        {
//            delete datagram;
//            return;
//        }
//
//        // don't forward if dest address is link-scope
//        if (destAddr.isLinkLocalMulticast())
//        {
//            delete datagram;
//            return;
//        }
//
//    }
//
//    // routed explicitly via Generic_MULTICAST_IF
//    if (destIE!=NULL)
//    {
//        ASSERT(datagram->getDestinationAddress().isMulticast());
//
//        EV << "multicast packet explicitly routed via output interface " << destIE->getName() << endl;
//
//        // set datagram source address if not yet set
//        if (datagram->getSourceAddress().isUnspecified())
//            datagram->setSourceAddress(destIE->ipv4Data()->getGenericAddress());
//
//        // send
//        fragmentAndSend(datagram, destIE, datagram->getDestinationAddress());
//
//        return;
//    }
//
//    // now: routing
//    MulticastRoutes routes = rt->getMulticastRoutesFor(destAddr);
//    if (routes.size()==0)
//    {
//        // no destination: delete datagram
//        delete datagram;
//    }
//    else
//    {
//        // copy original datagram for multiple destinations
//        for (unsigned int i=0; i<routes.size(); i++)
//        {
//            InterfaceEntry *destIE = routes[i].interf;
//
//            // don't forward to input port
//            if (destIE && destIE!=fromIE)
//            {
//                GenericDatagram *datagramCopy = (GenericDatagram *) datagram->dup();
//
//                // set datagram source address if not yet set
//                if (datagramCopy->getSourceAddress().isUnspecified())
//                    datagramCopy->setSourceAddress(destIE->ipv4Data()->getGenericAddress());
//
//                // send
//                Address nextHopAddr = routes[i].gateway;
//                fragmentAndSend(datagramCopy, destIE, nextHopAddr);
//            }
//        }
//
//        // only copies sent, delete original datagram
//        delete datagram;
//    }
}

void GenericNetworkProtocol::reassembleAndDeliver(GenericDatagram *datagram)
{
    // decapsulate and send on appropriate output gate
    int protocol = datagram->getTransportProtocol();
    cPacket *packet = decapsulateGeneric(datagram);

    int gateindex = mapping.getOutputGateForProtocol(protocol);
    send(packet, "transportOut", gateindex);
}

cPacket *GenericNetworkProtocol::decapsulateGeneric(GenericDatagram *datagram)
{
    // decapsulate transport packet
    InterfaceEntry *fromIE = getSourceInterfaceFrom(datagram);
    cPacket *packet = datagram->decapsulate();

    // create and fill in control info
    GenericNetworkProtocolControlInfo *controlInfo = new GenericNetworkProtocolControlInfo();
    controlInfo->setProtocol(datagram->getTransportProtocol());
    controlInfo->setSourceAddress(datagram->getSourceAddress());
    controlInfo->setDestinationAddress(datagram->getDestinationAddress());
    controlInfo->setInterfaceId(fromIE ? fromIE->getInterfaceId() : -1);
    controlInfo->setHopLimit(datagram->getHopLimit());

    // attach control info
    packet->setControlInfo(controlInfo);

    return packet;
}


void GenericNetworkProtocol::fragmentAndSend(GenericDatagram *datagram, InterfaceEntry *ie, Address nextHopAddr)
{
    if (datagram->getByteLength() > ie->getMTU())
        error("datagram too large"); //TODO refine

    sendDatagramToOutput(datagram, ie, nextHopAddr);
}


GenericDatagram *GenericNetworkProtocol::encapsulate(cPacket *transportPacket, InterfaceEntry *&destIE)
{
    GenericNetworkProtocolControlInfo *controlInfo = check_and_cast<GenericNetworkProtocolControlInfo*>(transportPacket->removeControlInfo());
    GenericDatagram *datagram = encapsulate(transportPacket, destIE, controlInfo);
    delete controlInfo;
    return datagram;
}

GenericDatagram *GenericNetworkProtocol::encapsulate(cPacket *transportPacket, InterfaceEntry *&destIE, GenericNetworkProtocolControlInfo *controlInfo)
{
    GenericDatagram *datagram = createGenericDatagram(transportPacket->getName());
//    datagram->setByteLength(HEADER_BYTES); //TODO parameter
    datagram->encapsulate(transportPacket);

    // set source and destination address
    Address dest = controlInfo->getDestinationAddress();
    datagram->setDestinationAddress(dest);

    // Generic_MULTICAST_IF option, but allow interface selection for unicast packets as well
    destIE = ift->getInterfaceById(controlInfo->getInterfaceId());

    Address src = controlInfo->getSourceAddress();

    // when source address was given, use it; otherwise it'll get the address
    // of the outgoing interface after routing
    if (!src.isUnspecified())
    {
        // if interface parameter does not match existing interface, do not send datagram
        if (rt->getInterfaceByAddress(src)==NULL)
            opp_error("Wrong source address %s in (%s)%s: no interface with such address",
                      src.str().c_str(), transportPacket->getClassName(), transportPacket->getFullName());
        datagram->setSourceAddress(src);
    }

    // set other fields
    short ttl;
    if (controlInfo->getHopLimit() > 0)
        ttl = controlInfo->getHopLimit();
    else if (false) //TODO: datagram->getDestinationAddress().isLinkLocalMulticast())
        ttl = 1;
    else
        ttl = defaultHopLimit;

    datagram->setHopLimit(ttl);
    datagram->setTransportProtocol(controlInfo->getProtocol());

    // setting GenericNetworkProtocol options is currently not supported

    return datagram;
}

GenericDatagram *GenericNetworkProtocol::createGenericDatagram(const char *name)
{
    return new GenericDatagram(name);
}

void GenericNetworkProtocol::sendDatagramToOutput(GenericDatagram *datagram, InterfaceEntry *ie, Address nextHopAddr)
{
    // hop counter check
    if (datagram->getHopLimit() <= 0)
    {
        // drop datagram, destruction responsibility in ICMP
        EV << "datagram hopLimit reached zero, discarding\n";
        delete datagram;  //TODO stats counter???
        return;
    }

    // send out datagram to ARP, with control info attached
    GenericRoutingDecision *routingDecision = new GenericRoutingDecision();
    routingDecision->setInterfaceId(ie->getInterfaceId());
    routingDecision->setNextHop(nextHopAddr);
    datagram->setControlInfo(routingDecision);

    send(datagram, queueOutGate);
}

void GenericNetworkProtocol::datagramLocalOut(GenericDatagram* datagram, InterfaceEntry* destIE)
{
    // route packet
    if (!datagram->getDestinationAddress().isMulticast())
        routePacket(datagram, destIE, true);
    else
        routeMulticastPacket(datagram, destIE, NULL);
}


void GenericNetworkProtocol::registerHook(int priority, IHook* hook) {
    Enter_Method("registerHook()");
    hooks.insert(std::pair<int, IHook*>(priority, hook));
}

void GenericNetworkProtocol::unregisterHook(int priority, IHook* hook) {
    Enter_Method("unregisterHook()");
    for (std::multimap<int, IHook*>::iterator iter = hooks.begin(); iter != hooks.end(); iter++) {
        if ((iter->first == priority) && (iter->second == hook)) {
            hooks.erase(iter);
            return;
        }
    }
}

void GenericNetworkProtocol::reinjectDatagram(const INetworkDatagram* datagram, IHook::Result verdict) {

    Enter_Method("reinjectDatagram()");
    for (std::list<QueuedDatagramForHook>::iterator iter = queuedDatagramsForHooks.begin(); iter != queuedDatagramsForHooks.end(); iter++) {
        if (iter->datagram == datagram) {
            GenericDatagram* datagram = iter->datagram;
            InterfaceEntry* outIE = iter->outIE;
            QueuedDatagramForHook::HookType hookType = iter->hookType;
            queuedDatagramsForHooks.erase(iter);
            switch (hookType) {
                case QueuedDatagramForHook::LOCALOUT:
                    if (verdict == IHook::DROP) {
                        delete datagram;
                        return;
                    }
                    else
                        datagramLocalOut(datagram, outIE);
                    break;
                default:
                    error("Re-injection of datagram queued for this hook not implemented");
                    break;
            }
            return;
        }
    }
}

INetworkProtocol::IHook::Result GenericNetworkProtocol::datagramPreRoutingHook(GenericDatagram* datagram, InterfaceEntry* inIE) {
    for (std::multimap<int, IHook*>::iterator iter = hooks.begin(); iter != hooks.end(); iter++) {
        IHook::Result r = iter->second->datagramPreRoutingHook(datagram, inIE);
        switch(r)
        {
            case IHook::ACCEPT: break;   // continue iteration
            case IHook::DROP:   delete datagram; return r;
            case IHook::QUEUE:  queuedDatagramsForHooks.push_back(QueuedDatagramForHook(datagram, inIE, NULL, QueuedDatagramForHook::PREROUTING)); return r;
            case IHook::STOLEN: return r;
            default: throw cRuntimeError("Unknown Hook::Result value: %d", (int)r);
        }
    }
    return IHook::ACCEPT;
}

INetworkProtocol::IHook::Result GenericNetworkProtocol::datagramLocalInHook(GenericDatagram* datagram, InterfaceEntry* inIE) {
    for (std::multimap<int, IHook*>::iterator iter = hooks.begin(); iter != hooks.end(); iter++) {
        IHook::Result r = iter->second->datagramLocalInHook(datagram, inIE);
        switch(r)
        {
            case IHook::ACCEPT: break;   // continue iteration
            case IHook::DROP:   delete datagram; return r;
            case IHook::QUEUE:  queuedDatagramsForHooks.push_back(QueuedDatagramForHook(datagram, inIE, NULL, QueuedDatagramForHook::LOCALIN)); return r;
            case IHook::STOLEN: return r;
            default: throw cRuntimeError("Unknown Hook::Result value: %d", (int)r);
        }
    }
    return IHook::ACCEPT;
}

INetworkProtocol::IHook::Result GenericNetworkProtocol::datagramForwardHook(GenericDatagram* datagram, InterfaceEntry* inIE, InterfaceEntry* outIE, Address& nextHopAddr) {
    for (std::multimap<int, IHook*>::iterator iter = hooks.begin(); iter != hooks.end(); iter++) {
        IHook::Result r = iter->second->datagramForwardHook(datagram, inIE, outIE, nextHopAddr);
        switch(r)
        {
            case IHook::ACCEPT: break;   // continue iteration
            case IHook::DROP:   delete datagram; return r;
            case IHook::QUEUE:  queuedDatagramsForHooks.push_back(QueuedDatagramForHook(datagram, inIE, outIE, QueuedDatagramForHook::FORWARD)); return r;
            case IHook::STOLEN: return r;
            default: throw cRuntimeError("Unknown Hook::Result value: %d", (int)r);
        }
    }
    return IHook::ACCEPT;
}

INetworkProtocol::IHook::Result GenericNetworkProtocol::datagramPostRoutingHook(GenericDatagram* datagram, InterfaceEntry* inIE, InterfaceEntry* outIE, Address& nextHopAddr) {
    for (std::multimap<int, IHook*>::iterator iter = hooks.begin(); iter != hooks.end(); iter++) {
        IHook::Result r = iter->second->datagramPostRoutingHook(datagram, inIE, outIE, nextHopAddr);
        switch(r)
        {
            case IHook::ACCEPT: break;   // continue iteration
            case IHook::DROP:   delete datagram; return r;
            case IHook::QUEUE:  queuedDatagramsForHooks.push_back(QueuedDatagramForHook(datagram, inIE, outIE, QueuedDatagramForHook::POSTROUTING)); return r;
            case IHook::STOLEN: return r;
            default: throw cRuntimeError("Unknown Hook::Result value: %d", (int)r);
        }
    }
    return IHook::ACCEPT;
}

INetworkProtocol::IHook::Result GenericNetworkProtocol::datagramLocalOutHook(GenericDatagram* datagram, InterfaceEntry* outIE) {
    for (std::multimap<int, IHook*>::iterator iter = hooks.begin(); iter != hooks.end(); iter++) {
        IHook::Result r = iter->second->datagramLocalOutHook(datagram, outIE);
        switch(r)
        {
            case IHook::ACCEPT: break;   // continue iteration
            case IHook::DROP:   delete datagram; return r;
            case IHook::QUEUE:  queuedDatagramsForHooks.push_back(QueuedDatagramForHook(datagram, NULL, outIE, QueuedDatagramForHook::LOCALOUT)); return r;
            case IHook::STOLEN: return r;
            default: throw cRuntimeError("Unknown Hook::Result value: %d", (int)r);
        }
    }
    return IHook::ACCEPT;
}
