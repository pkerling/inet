//
// This program is property of its copyright holder. All rights reserved.
//

#include "UDPPacket.h"
#include "Ieee80211Frame_m.h"
#include "IPv4ControlInfo.h"
#include "IPv4Datagram.h"
#include "IPv4InterfaceData.h"
#include "UDPControlInfo.h"
#include "xDYMO.h"

DYMO_NAMESPACE_BEGIN

Define_Module(DYMO::xDYMO);

#define DYMO_EV EV << "DYMO at " << getSelfAddress() << " "

//
// construction
//

xDYMO::xDYMO() {
    routingTable = NULL;
    networkProtocol = NULL;
    expungeTimer = NULL;
}

xDYMO::~xDYMO() {
    for (std::map<Address, RREQTimer *>::iterator it = targetAddressToRREQTimer.begin(); it != targetAddressToRREQTimer.end(); it++)
        cancelAndDelete(it->second);
    cancelAndDelete(expungeTimer);
}

//
// module interface
//

void xDYMO::initialize(int stage) {
    if (stage == 0) {
        // context parameters
        routingTableModuleName = par("routingTableModuleName");
        networkProtocolModuleName = par("networkProtocolModuleName");
        // DYMO parameters from RFC
        clientAddresses = par("clientAddresses");
        useMulticastRREP = par("useMulticastRREP");
        activeInterval = par("activeInterval");
        maxIdleTime = par("maxIdleTime");
        maxSequenceNumberLifetime = par("maxSequenceNumberLifetime");
        routeRREQWaitTime = par("routeRREQWaitTime");
        rreqHolddownTime = par("rreqHolddownTime");
        maxHopCount = par("maxHopCount");
        discoveryAttemptsMax = par("discoveryAttemptsMax");
        appendInformation = par("appendInformation");
        bufferSizePackets = par("bufferSizePackets");
        bufferSizeBytes = par("bufferSizeBytes");
        // DYMO extension parameters
        maxJitter = par("maxJitter");
        sendIntermediateRREP = par("sendIntermediateRREP");
        minHopLimit = par("minHopLimit");
        maxHopLimit = par("maxHopLimit");
        // context
        notificationBoard = NotificationBoardAccess().get(this);
        interfaceTable = InterfaceTableAccess().get(this);
        // KLUDGE: simplify this when RoutingTable implements IGenericRoutingTable
        routingTable = check_and_cast<RoutingTable *>(findModuleWhereverInNode(routingTableModuleName, this))->asGeneric();
        networkProtocol = check_and_cast<IGenericNetworkProtocol *>(findModuleWhereverInNode(networkProtocolModuleName, this));
        // internal
        expungeTimer = new cMessage("ExpungeTimer");
        notificationBoard->subscribe(this, NF_LINK_BREAK);
        // TODO: fill in clientNetworks based on clientAddresses
    }
    else if (stage == 4) {
        // join multicast groups
        for (int i = 0; i < interfaceTable->getNumInterfaces(); i++) {
            InterfaceEntry * interfaceEntry = interfaceTable->getInterface(i);
            if (interfaceEntry->isMulticast()) {
                IPv4InterfaceData * ipv4InterfaceData = interfaceTable->getInterface(i)->ipv4Data();
                if (ipv4InterfaceData)
                   // Most AODVv2 messages are sent with the IP destination address set to the link-local
                   // multicast address LL-MANET-Routers [RFC5498] unless otherwise specified. Therefore,
                   // all AODVv2 routers MUST subscribe to LL-MANET-Routers [RFC5498] to receiving AODVv2 messages.
                   ipv4InterfaceData->joinMulticastGroup(IPv4Address::LL_MANET_ROUTERS);
            }
        }
        // hook to network protocol
        networkProtocol->registerHook(0, this);
    }
}

void xDYMO::handleMessage(cMessage * message) {
    if (message->isSelfMessage())
        processSelfMessage(message);
    else
        processMessage(message);
}

//
// handling messages
//

void xDYMO::processSelfMessage(cMessage * message) {
    if (message == expungeTimer)
        processExpungeTimer();
    else if (dynamic_cast<RREQWaitRREPTimer *>(message))
        processRREQWaitRREPTimer((RREQWaitRREPTimer *)message);
    else if (dynamic_cast<RREQBackoffTimer *>(message))
        processRREQBackoffTimer((RREQBackoffTimer *)message);
    else if (dynamic_cast<RREQHolddownTimer *>(message))
        processRREQHolddownTimer((RREQHolddownTimer *)message);
    else
        throw cRuntimeError("Unknown self message");
}

void xDYMO::processMessage(cMessage * message) {
    if (dynamic_cast<UDPPacket *>(message))
        processUDPPacket((UDPPacket *)message);
    else
        throw cRuntimeError("Unknown message");
}

//
// route discovery
//

void xDYMO::startRouteDiscovery(const Address & target) {
    DYMO_EV << "Starting route discovery: originator = " << getSelfAddress() << ", target = " << target << endl;
    ASSERT(!hasOngoingRouteDiscovery(target));
    sendRREQ(createRREQ(target, 0));
    scheduleRREQWaitRREPTimer(createRREQWaitRREPTimer(target, 0));
}

void xDYMO::retryRouteDiscovery(const Address & target, int retryCount) {
    DYMO_EV << "Retrying route discovery: originator = " << getSelfAddress() << ", target = " << target << ", retry = " << retryCount << endl;
    ASSERT(hasOngoingRouteDiscovery(target));
    sendRREQ(createRREQ(target, retryCount));
    scheduleRREQWaitRREPTimer(createRREQWaitRREPTimer(target, retryCount));
}

void xDYMO::completeRouteDiscovery(const Address & target) {
    DYMO_EV << "Completing route discovery: originator = " << getSelfAddress() << ", target = " << target << endl;
    ASSERT(hasOngoingRouteDiscovery(target));
    std::multimap<Address, IGenericDatagram *>::iterator lt = targetAddressToDelayedPackets.lower_bound(target);
    std::multimap<Address, IGenericDatagram *>::iterator ut = targetAddressToDelayedPackets.upper_bound(target);
    for (std::multimap<Address, IGenericDatagram *>::iterator it = lt; it != ut; it++)
        reinjectDelayedDatagram(it->second);
    eraseDelayedDatagrams(target);
}

void xDYMO::cancelRouteDiscovery(const Address & target) {
    DYMO_EV << "Canceling route discovery: originator = " << getSelfAddress() << ", target = " << target << endl;
    ASSERT(hasOngoingRouteDiscovery(target));
    std::multimap<Address, IGenericDatagram *>::iterator lt = targetAddressToDelayedPackets.lower_bound(target);
    std::multimap<Address, IGenericDatagram *>::iterator ut = targetAddressToDelayedPackets.upper_bound(target);
    for (std::multimap<Address, IGenericDatagram *>::iterator it = lt; it != ut; it++)
        dropDelayedDatagram(it->second);
    eraseDelayedDatagrams(target);
}

bool xDYMO::hasOngoingRouteDiscovery(const Address & target) {
    return targetAddressToRREQTimer.find(target) != targetAddressToRREQTimer.end();
}

//
// handling IP datagrams
//

void xDYMO::delayDatagram(IGenericDatagram * datagram) {
    DYMO_EV << "Queuing datagram: source = " << datagram->getSourceAddress() << ", destination = " << datagram->getDestinationAddress() << endl;
    const Address & target = datagram->getDestinationAddress();
    targetAddressToDelayedPackets.insert(std::pair<Address, IGenericDatagram *>(target, datagram));
}

void xDYMO::reinjectDelayedDatagram(IGenericDatagram * datagram) {
    DYMO_EV << "Sending queued datagram: source = " << datagram->getSourceAddress() << ", destination = " << datagram->getDestinationAddress() << endl;
    networkProtocol->reinjectDatagram(const_cast<const IGenericDatagram *>(datagram), IHook::ACCEPT);
}

void xDYMO::dropDelayedDatagram(IGenericDatagram * datagram) {
    DYMO_EV << "Dropping queued datagram: source = " << datagram->getSourceAddress() << ", destination = " << datagram->getDestinationAddress() << endl;
    networkProtocol->reinjectDatagram(const_cast<const IGenericDatagram *>(datagram), IHook::DROP);
}

void xDYMO::eraseDelayedDatagrams(const Address & target) {
    std::multimap<Address, IGenericDatagram *>::iterator lt = targetAddressToDelayedPackets.lower_bound(target);
    std::multimap<Address, IGenericDatagram *>::iterator ut = targetAddressToDelayedPackets.upper_bound(target);
    targetAddressToDelayedPackets.erase(lt, ut);
}

bool xDYMO::hasDelayedDatagrams(const Address & target) {
    return targetAddressToDelayedPackets.find(target) != targetAddressToDelayedPackets.end();
}

//
// handling RREQ timers
//

void xDYMO::cancelRREQTimer(const Address & target) {
    std::map<Address, RREQTimer *>::iterator tt = targetAddressToRREQTimer.find(target);
    cancelEvent(tt->second);
}

void xDYMO::deleteRREQTimer(const Address & target) {
    std::map<Address, RREQTimer *>::iterator tt = targetAddressToRREQTimer.find(target);
    delete tt->second;
}

void xDYMO::eraseRREQTimer(const Address & target) {
    std::map<Address, RREQTimer *>::iterator tt = targetAddressToRREQTimer.find(target);
    targetAddressToRREQTimer.erase(tt);
}

//
// handling RREQ wait RREP timers
//

RREQWaitRREPTimer * xDYMO::createRREQWaitRREPTimer(const Address & target, int retryCount) {
    RREQWaitRREPTimer * message = new RREQWaitRREPTimer("RREQWaitRREPTimer");
    message->setRetryCount(retryCount);
    message->setTarget(target);
    return message;
}

void xDYMO::scheduleRREQWaitRREPTimer(RREQWaitRREPTimer * message) {
    DYMO_EV << "Scheduling RREQ wait RREP timer" << endl;
    targetAddressToRREQTimer[message->getTarget()] = message;
    scheduleAt(simTime() + routeRREQWaitTime, message);
}

void xDYMO::processRREQWaitRREPTimer(RREQWaitRREPTimer * message) {
    DYMO_EV << "Processing RREQ wait RREP timer" << endl;
    const Address & target = message->getTarget();
    if (message->getRetryCount() == discoveryAttemptsMax - 1) {
        cancelRouteDiscovery(target);
        cancelRREQTimer(target);
        eraseRREQTimer(target);
        scheduleRREQHolddownTimer(createRREQHolddownTimer(target));
    }
    else
        scheduleRREQBackoffTimer(createRREQBackoffTimer(target, message->getRetryCount()));
    delete message;
}

//
// handling RREQ backoff timer
//

RREQBackoffTimer * xDYMO::createRREQBackoffTimer(const Address & target, int retryCount) {
    RREQBackoffTimer * message = new RREQBackoffTimer("RREQBackoffTimer");
    message->setRetryCount(retryCount);
    message->setTarget(target);
    return message;
}

void xDYMO::scheduleRREQBackoffTimer(RREQBackoffTimer * message) {
    DYMO_EV << "Scheduling RREQ backoff timer" << endl;
    targetAddressToRREQTimer[message->getTarget()] = message;
    scheduleAt(simTime() + computeRREQBackoffTime(message->getRetryCount()), message);
}

void xDYMO::processRREQBackoffTimer(RREQBackoffTimer * message) {
    DYMO_EV << "Processing RREQ backoff timer" << endl;
    retryRouteDiscovery(message->getTarget(), message->getRetryCount() + 1);
    delete message;
}

simtime_t xDYMO::computeRREQBackoffTime(int retryCount) {
    return pow(routeRREQWaitTime, retryCount);
}

//
// handling RREQ holddown timers
//

RREQHolddownTimer * xDYMO::createRREQHolddownTimer(const Address & target) {
    RREQHolddownTimer * message = new RREQHolddownTimer("RREQHolddownTimer");
    message->setTarget(target);
    return message;
}

void xDYMO::scheduleRREQHolddownTimer(RREQHolddownTimer * message) {
    DYMO_EV << "Scheduling RREQ holddown timer" << endl;
    targetAddressToRREQTimer[message->getTarget()] = message;
    scheduleAt(simTime() + rreqHolddownTime, message);
}

void xDYMO::processRREQHolddownTimer(RREQHolddownTimer * message) {
    DYMO_EV << "Processing RREQ holddown timer" << endl;
    const Address & target = message->getTarget();
    eraseRREQTimer(target);
    if (hasDelayedDatagrams(target))
        startRouteDiscovery(target);
    delete message;
}

//
// handling UDP packets
//

void xDYMO::sendUDPPacket(UDPPacket * packet, double delay) {
    if (delay == 0)
        send(packet, "ipOut");
    else
        sendDelayed(packet, delay, "ipOut");
}

void xDYMO::processUDPPacket(UDPPacket * packet) {
    cPacket * encapsulatedPacket = packet->decapsulate();
    if (dynamic_cast<DYMOPacket *>(encapsulatedPacket)) {
        DYMOPacket * dymoPacket = (DYMOPacket *)encapsulatedPacket;
        IPv4ControlInfo * ipv4ControlInfo = check_and_cast<IPv4ControlInfo *>(packet->removeControlInfo());
        dymoPacket->setControlInfo(ipv4ControlInfo);
        processDYMOPacket(dymoPacket);
    }
    else
        throw cRuntimeError("Unknown UDP packet");
    delete packet;
}


//
// handling DYMO packets
//

void xDYMO::sendDYMOPacket(DYMOPacket * packet, InterfaceEntry * interfaceEntry, const IPv4Address & nextHop, double delay) {
    IPv4ControlInfo * ipv4ControlInfo = new IPv4ControlInfo();
    // 5.4. AODVv2 Packet Header Fields and Information Elements
    // In addition, IP Protocol Number 138 has been reserved for MANET protocols [RFC5498].
    ipv4ControlInfo->setProtocol(IP_PROT_MANET);
    // The IPv4 TTL (IPv6 Hop Limit) field for all packets containing AODVv2 messages is set to 255.
    ipv4ControlInfo->setTimeToLive(255);
    ipv4ControlInfo->setDestAddr(nextHop);
    ipv4ControlInfo->setSrcAddr(getSelfAddress().toIPv4());
    if (interfaceEntry)
        ipv4ControlInfo->setInterfaceId(interfaceEntry->getInterfaceId());
    UDPPacket * udpPacket = new UDPPacket(packet->getName());
    udpPacket->encapsulate(packet);
    // In its default mode of operation, AODVv2 uses the UDP port 269 [RFC5498] to carry protocol packets.
    udpPacket->setSourcePort(DYMO_UDP_PORT);
    udpPacket->setDestinationPort(DYMO_UDP_PORT);
    udpPacket->setControlInfo(ipv4ControlInfo);
    sendUDPPacket(udpPacket, delay);
}

void xDYMO::processDYMOPacket(DYMOPacket * packet) {
    if (dynamic_cast<RREQ *>(packet))
        processRREQ((RREQ *)packet);
    else if (dynamic_cast<RREP *>(packet))
        processRREP((RREP *)packet);
    else if (dynamic_cast<RERR *>(packet))
        processRERR((RERR *)packet);
    else
        throw cRuntimeError("Unknown DYMO packet");
}

//
// handling RteMsg packets
//

bool xDYMO::permissibleRteMsg(RteMsg * rteMsg) {
    // 7.5. Handling a Received RteMsg
    AddressBlock & originatorNode = rteMsg->getOriginatorNode();
    AddressBlock & targetNode = rteMsg->getTargetNode();
    IPv4ControlInfo * ipv4ControlInfo = check_and_cast<IPv4ControlInfo *>(rteMsg->getControlInfo());
    // 1. HandlingRtr MUST handle AODVv2 messages only from adjacent
    //    routers as specified in Section 5.4. AODVv2 messages from other
    //    sources MUST be disregarded.
    //    5.4. AODVv2 Packet Header Fields and Information Elements
    //    If a packet is received with a value other than 255, any AODVv2
    //    message contained in the packet MUST be disregarded by AODVv2.
    // FIXME: we should rather compare with 255 but unfortunately IPv4 decrements
    // TTL too early in the sender, see http://en.wikipedia.org/wiki/Time_to_live
    if (ipv4ControlInfo->getTimeToLive() != 254)
        return false;
    // 2. If the RteMsg.<msg-hop-limit> is equal to 0, then the message is disregarded.
    if (rteMsg->getHopLimit() == 0)
        return false;
    // 3. If the RteMsg.<msg-hop-count> is present, and RteMsg.<msg-hop-
    //    count> >= MAX_HOPCOUNT, then the message is disregarded.
    if (rteMsg->getHopCount() >= maxHopCount)
        return false;
    // 4. HandlingRtr examines the RteMsg to ascertain that it contains the
    //    required information: TargNode.Addr, OrigNode.Addr,
    //    RteMsg_Gen.Metric and RteMsg_Gen.SeqNum.  If the required
    //    information does not exist, the message is disregarded.
    // TODO: implement
    // 5. HandlingRtr checks that OrigNode.Addr and TargNode.Addr are valid
    //    routable unicast addresses.  If not, the message is disregarded.
    const Address & originatorAddress = originatorNode.getAddress();
    const Address & targetAddress = targetNode.getAddress();
    if (originatorAddress.isUnspecified() || originatorAddress.isMulticast() || originatorAddress.isBroadcast() ||
        targetAddress.isUnspecified() || targetAddress.isMulticast() || targetAddress.isBroadcast())
        return false;
    // 6. HandlingRtr checks that the Metric Type associated with
    //    OrigNode.Metric and TargNode.Metric is known, and that Cost(L)
    //    can be computed.  If not, the message is disregarded.
    //     *  DISCUSSION: alternatively, can change the AddrBlk metric to
    //        use HopCount, measured from<msg-hop-limit>.
    if (originatorNode.getMetricType() != HOP_COUNT || targetNode.getMetricType() != HOP_COUNT)
        return false;
    // 7. If MAX_METRIC[RteMsg.MetricType] <= (RteMsg_Gen.Metric +
    //    Cost(L)), where 'L' is the incoming link, the RteMsg is
    //    disregarded.
    // TODO: implement
    return true;
}

void xDYMO::processRteMsg(RteMsg * rteMsg) {
    // 7.5. Handling a Received RteMsg
    // 1. HandlingRtr MUST process the routing information contained in the
    //    RteMsg as speciied in Section 6.1.
    if (dynamic_cast<RREQ *>(rteMsg))
        updateRoutes(rteMsg, rteMsg->getOriginatorNode());
    else if (dynamic_cast<RREP *>(rteMsg))
        updateRoutes(rteMsg, rteMsg->getTargetNode());
    // 2. HandlingRtr MAY process AddedNode routing information (if
    //    present) as specified in Section 13.7.1 Otherwise, if AddedNode
    //    information is not processed, it MUST be deleted.
    int count = rteMsg->getAddedNodeArraySize();
    for (int i = 0; i < count; i++)
        updateRoutes(rteMsg, rteMsg->getAddedNode(i));
    // 3. By sending the updated RteMsg, HandlingRtr advertises that it
    //    will route for addresses contained in the outgoing RteMsg based
    //    on the information enclosed.  HandlingRtr MAY choose not to send
    //    the RteMsg, though not resending this RteMsg could decrease
    //    connectivity in the network or result in a nonoptimal path.  The
    //    circumstances under which HandlingRtr might choose to not re-
    //    transmit a RteMsg are not specified in this document.  Some
    //    examples might include the following:
    //    * HandlingRtr is already heavily loaded and does not want to
    //      advertise routing for the contained addresses
    //    * HandlingRtr recently transmitted identical routing information
    //      (e.g. in a RteMsg advertising the same metric)
    //    * HandlingRtr is low on energy and has to reduce energy expended
    //      for sending protocol messages or packet forwarding
    //    Unless HandlingRtr is prepared to send an updated RteMsg, it
    //    halts processing.  Otherwise, processing continues as follows.
    // TODO: why is this here and how could we halt here?
    // 4. HandlingRtr MUST decrement RteMsg.<msg-hop-limit>.  If
    //    RteMsg.<msg-hop-limit> is then zero (0), no further action is taken.
    rteMsg->setHopLimit(rteMsg->getHopLimit() - 1);
    // 5. HandlingRtr MUST increment RteMsg.<msg-hop-count>.
    rteMsg->setHopCount(rteMsg->getHopCount() + 1);
}

//
// handling RREQ packets
//

RREQ * xDYMO::createRREQ(const Address & target, int retryCount) {
    RREQ * rreq = new RREQ("RREQ");
    AddressBlock & originatorNode = rreq->getOriginatorNode();
    AddressBlock & targetNode = rreq->getTargetNode();
    // 7.3. RREQ Generation
    // 1. RREQ_Gen MUST increment its OwnSeqNum by one (1) according to the
    //    rules specified in Section 5.5.
    incrementSequenceNumber();
    // 2. OrigNode MUST be a unicast address.  If RREQ_Gen is not OrigNode,
    //    then OwnSeqNum will be used as the value of OrigNode.SeqNum. will
    //    be used by AODVv2 routers to create a route toward the OrigNode,
    //    enabling a RREP from TargRtr, and eventually used for proper
    //    forwarding of data packets.
    // 3. If RREQ_Gen requires that only TargRtr is allowed to generate a
    //    RREP, then RREQ_Gen includes the "Destination RREP Only" TLV as
    //    part of the RFC 5444 message header.  This also assures that
    //    TargRtr increments its sequence number.  Otherwise, intermediate
    //    AODVv2 routers MAY respond to the RREQ_Gen's RREQ if they have an
    //    valid route to TargNode (see Section 13.2).
    // 4. msg-hopcount MUST be set to 0.
    rreq->setHopCount(0);
    //    *  This RFC 5444 constraint causes the typical RteMsg payload
    //       incur additional enlargement.
    // 5. RREQ_Gen adds the TargNode.Addr to the RREQ.
    targetNode.setAddress(target);
    targetNode.setPrefixLength(target.getPrefixLength());
    // 6. If a previous value of the TargNode's SeqNum is known RREQ_Gen SHOULD
    //    include TargNode.SeqNum in all but the last RREQ attempt.
    if (retryCount < discoveryAttemptsMax - 1)
        // TODO: implement
        targetNode.setSequenceNumber(-1);
    // 7. RREQ_Gen adds OrigNode.Addr, its prefix, and the RREQ_Gen.SeqNum (OwnSeqNum) to the RREQ.
    const Address & originator = getSelfAddress();
    originatorNode.setAddress(originator);
    originatorNode.setPrefixLength(originator.getPrefixLength());
    originatorNode.setSequenceNumber(sequenceNumber);
    // 8. If OrigNode.Metric is included it is set to the cost of the route
    //    between OrigNode and RREQ_Gen.
    originatorNode.setMetric(0);
    originatorNode.setMetricType(HOP_COUNT);
    targetNode.setMetricType(HOP_COUNT);
    // expanding ring search
    int hopLimit = minHopLimit + (maxHopLimit - minHopLimit) * retryCount / discoveryAttemptsMax;
    rreq->setHopLimit(hopLimit);
    return rreq;
}

void xDYMO::sendRREQ(RREQ * rreq) {
    const Address & target = rreq->getTargetNode().getAddress();
    const Address & originator = rreq->getOriginatorNode().getAddress();
    // TODO: compute proper length
    rreq->setBitLength(0);
    DYMO_EV << "Sending RREQ: originator = " << originator << ", target = " << target << endl;
    sendDYMOPacket(rreq, NULL, IPv4Address::LL_MANET_ROUTERS, uniform(0, maxJitter).dbl());
}

void xDYMO::processRREQ(RREQ * rreqIncoming) {
    const Address & target = rreqIncoming->getTargetNode().getAddress();
    const Address & originator = rreqIncoming->getOriginatorNode().getAddress();
    DYMO_EV << "Processing RREQ: originator = " << originator << ", target = " << target << endl;
    if (permissibleRteMsg(rreqIncoming)) {
        processRteMsg(rreqIncoming);
        // 7.5.1. Additional Handling for Outgoing RREQ
        // o If the upstream router is in the Blacklist, and Current_Time <
        //   BlacklistRmTime, then HandlingRtr MUST NOT transmit any outgoing
        //   RREQ, and processing is complete.
        // TODO: implement
        // o Otherwise, if the upstream router is in the Blacklist, and
        //   Current_Time >= BlacklistRmTime, then the upstream router SHOULD
        //   be removed from the Blacklist, and message processing continued.
        // TODO: implement
        if (isClientAddress(target)) {
            // o If TargNode is a client of HandlingRtr, then a RREP is generated
            //   by the HandlingRtr (i.e., TargRtr) and unicast to the upstream
            //   router towards the RREQ OrigNode, as specified in Section 7.4.
            //   Afterwards, TargRtr processing for the RREQ is complete.
            DYMO_EV << "Received RREQ for client: originator = " << originator << ", target = " << target << endl;
            if (useMulticastRREP)
                sendRREP(createRREP(rreqIncoming));
            else {
                IGenericRoute * route = routingTable->findBestMatchingRoute(originator);
                RREP * rrep = createRREP(rreqIncoming, route);
                sendRREP(rrep, route);
            }
        }
        else {
            // o If HandlingRtr is not the TargetNode, then the outgoing RREQ (as
            //   altered by the procedure defined above) SHOULD be sent to the IP
            //   multicast address LL-MANET-Routers [RFC5498].  If the RREQ is
            //   unicast, the IP.DestinationAddress is set to the NextHopAddress.
            DYMO_EV << "Forwarding RREQ: originator = " << originator << ", target = " << target << endl;
            RREQ * rreqOutgoing = rreqIncoming->dup();
            if (appendInformation)
                addSelfNode(rreqOutgoing);
            sendRREQ(rreqOutgoing);
        }
    }
    else
        DYMO_EV << "Dropping non-permissible RREQ" << endl;
    delete rreqIncoming;
}

//
// handling RREP packets
//

RREP * xDYMO::createRREP(RteMsg * rteMsg) {
    return createRREP(rteMsg, NULL);
}

RREP * xDYMO::createRREP(RteMsg * rteMsg, IGenericRoute * route) {
    DYMORouteData * routeData = (DYMORouteData *)route->getProtocolData();
    RREP * rrep = new RREP("RREP");
    AddressBlock & originatorNode = rrep->getOriginatorNode();
    AddressBlock & targetNode = rrep->getTargetNode();
    // 1. RREP_Gen first uses the routing information to update its route
    //    table entry for OrigNode if necessary as specified in Section 6.2.
    // TODO: why is this here? this should have already happened.
    // 2. RREP_Gen MUST increment its OwnSeqNum by one (1) according to
    //    the rules specified in Section 5.5.
    incrementSequenceNumber();
    // 3. RREP.AddrBlk[OrigNode] := RREQ.AddrBlk[OrigNode]
    originatorNode = AddressBlock(rteMsg->getOriginatorNode());
    // 4. RREP.AddrBlk[TargNode] := RREQ.AddrBlk[TargNode]
    targetNode = AddressBlock(rteMsg->getTargetNode());
    // 5. RREP.SeqNumTLV[OrigNode] := RREQ.SeqNumTLV[OrigNode]
    originatorNode.setSequenceNumber(rteMsg->getOriginatorNode().getSequenceNumber());
    // 6. RREP.SeqNumTLV[TargNode] := OwnSeqNum
    targetNode.setSequenceNumber(sequenceNumber);
    // 7. If Route[TargNode].PfxLen/8 is equal to the number of bytes in
    //    the addresses of the RREQ (4 for IPv4, 16 for IPv6), then no
    //    <prefix-length> is included with the iRREP.  Otherwise,
    //    RREP.PfxLen[TargNode] := RREQ.PfxLen[TargNode] according to the
    //    rules of RFC 5444 AddrBlk encoding.
    // TODO: implement
    // 8. RREP.MetricType[TargNode] := Route[TargNode].MetricType
    targetNode.setMetricType(routeData->getMetricType());
    // 9. RREP.Metric[TargNode] := Route[TargNode].Metric
    targetNode.setMetric(route->getMetric());
    // 10. <msg-hop-limit> SHOULD be set to RteMsg.<msg-hop-count>.
    rrep->setHopLimit(rteMsg->getHopCount());
    // 11. IP.DestinationAddr := Route[OrigNode].NextHop
    // TODO: can't be done here
    return rrep;
}

void xDYMO::sendRREP(RREP * rrep) {
    const Address & target = rrep->getTargetNode().getAddress();
    const Address & originator = rrep->getOriginatorNode().getAddress();
    // TODO: compute proper length
    rrep->setBitLength(0);
    DYMO_EV << "Sending RREP: originator = " << originator << ", target = " << target << endl;
    sendDYMOPacket(rrep, NULL, IPv4Address::LL_MANET_ROUTERS, 0);
}

void xDYMO::sendRREP(RREP * rrep, IGenericRoute * route) {
    const Address & target = rrep->getTargetNode().getAddress();
    const Address & originator = rrep->getOriginatorNode().getAddress();
    // TODO: compute proper length
    rrep->setBitLength(0);
    DYMO_EV << "Sending RREP: originator = " << originator << ", target = " << target << endl;
    sendDYMOPacket(rrep, route->getInterface(), route->getNextHop().toIPv4(), 0);
}

void xDYMO::processRREP(RREP * rrepIncoming) {
    const Address & target = rrepIncoming->getTargetNode().getAddress();
    const Address & originator = rrepIncoming->getOriginatorNode().getAddress();
    DYMO_EV << "Processing RREP: originator = " << originator << ", target = " << target << endl;
    if (permissibleRteMsg(rrepIncoming)) {
        processRteMsg(rrepIncoming);
        // 7.5.2. Additional Handling for Outgoing RREP
        if (isClientAddress(originator)) {
            DYMO_EV << "Received RREP for client: originator = " << originator << ", target = " << target << endl;
            if (hasOngoingRouteDiscovery(target)) {
                completeRouteDiscovery(target);
                cancelRREQTimer(target);
                deleteRREQTimer(target);
                eraseRREQTimer(target);
            }
        }
        else {
            // o If HandlingRtr is not OrigRtr then the outgoing RREP is sent to
            //   the Route.NextHopAddress for the RREP.AddrBlk[OrigNode].  If no
            //   forwarding route exists to OrigNode, then a RERR SHOULD be
            //   transmitted to RREP.AddrBlk[TargNode].  See Table 1 for notational
            //   conventions; OrigRtr, OrigNode, and TargNode are routers named in
            //   the context of OrigRtr, that is, the router originating the RREQ
            //   to which the RREP is responding.
            DYMO_EV << "Forwarding RREP: originator = " << originator << ", target = " << target << endl;
            RREP * rrepOutgoing = rrepIncoming->dup();
            if (appendInformation)
                addSelfNode(rrepOutgoing);
            if (useMulticastRREP)
                sendRREP(rrepOutgoing);
            else {
                IGenericRoute * route = routingTable->findBestMatchingRoute(originator);
                sendRREP(rrepOutgoing, route);
            }
        }
    }
    else
        DYMO_EV << "Dropping non-permissible RREQ" << endl;
    delete rrepIncoming;
}

//
// handling RERR packets
//

RERR * xDYMO::createRERR(std::vector<Address> & unreachableAddresses) {
    RERR * rerr = new RERR();
    for (int i = 0; i < (int)unreachableAddresses.size(); i++) {
        const Address & unreachableAddress = unreachableAddresses[i];
        AddressBlock * addressBlock = new AddressBlock();
        addressBlock->setAddress(unreachableAddress);
        addressBlock->setPrefixLength(unreachableAddress.getPrefixLength());
        int size = rerr->getUnreachableNodeArraySize();
        rerr->setUnreachableNodeArraySize(size + 1);
        rerr->setUnreachableNode(size, *addressBlock);
    }
    rerr->setHopLimit(maxHopLimit);
    return rerr;
}

void xDYMO::sendRERR(RERR * rerr) {
    // TODO: compute proper length
    rerr->setBitLength(0);
    DYMO_EV << "Sending RERR: unreachableNodeCount = " << rerr->getUnreachableNodeArraySize() << endl;
    sendDYMOPacket(rerr, NULL, IPv4Address::LL_MANET_ROUTERS, 0);
}

void xDYMO::sendRERRForUndeliverablePacket(const Address & destination) {
    DYMO_EV << "Sending RERR for undeliverable packet: destination = " << destination << endl;
    // 8.3.1. Case 1: Undeliverable Packet
    // The first case happens when the router receives a packet but does not
    // have a valid route for the destination of the packet.  In this case,
    // there is exactly one UnreachableNode to be included in the RERR's
    // AddrBlk.  RERR_dest SHOULD be the multicast address LL-MANET-Routers,
    // but RERR_Gen MAY instead set RERR_dest to be the next hop towards the
    // source IP address of the packet which was undeliverable.  In the
    // latter case, the PktSource MsgTLV MUST be included, containing the
    // the source IP address of the undeliverable packet.  If a value for
    // the UnreachableNode's SeqNum (UnreachableNode.SeqNum) is known, it
    // MUST be placed in the RERR.  Otherwise, if no Seqnum AddrTLV is
    // included, all nodes handling the RERR will assume their route through
    // RERR_Gen towards the UnreachableNode is no longer valid and flag
    // those routes as broken.  RERR_Gen MUST discard the packet or message
    // that triggered generation of the RERR.
    std::vector<Address> unreachableAddresses;
    unreachableAddresses.push_back(destination);
    sendRERR(createRERR(unreachableAddresses));
}

void xDYMO::sendRERRForBrokenLink(InterfaceEntry * interfaceEntry, const Address & nextHop) {
    DYMO_EV << "Sending RERR for broken link: nextHop = " << nextHop << endl;
    // 8.3.2. Case 2: Broken Link
    // The second case happens when the link breaks to an active downstream
    // neighbor (i.e., the next hop of an active route).  In this case,
    // RERR_dest MUST be the multicast address LL-MANET-Routers, except when
    // the optional feature of maintaining precursor lists is used as
    // specified in Section 13.3.  All Active, Idle and Expired routes that
    // use the broken link MUST be marked as Broken.  The set of
    // UnreachableNodes is initialized by identifying those Active routes
    // which use the broken link.  For each such Active Route, Route.Dest is
    // added to the set of Unreachable Nodes.  After the Active Routes using
    // the broken link have all been included as UnreachableNodes, idle
    // routes MAY also be included, as long as the packet size of the RERR
    // does not exceed the MTU of the physical medium.
    // If the set of UnreachableNodes is empty, no RERR is generated.
    // Otherwise, RERR_Gen generates a new RERR, and the address of each
    // UnreachableNode (IP.DestinationAddress from a data packet or
    // RREP.TargNode.Address) is inserted into an AddrBlock.  If a prefix is
    // known for the UnreachableNode.Address, it SHOULD be included.
    // Otherwise, the UnreachableNode.Address is assumed to be a host
    // address with a full length prefix.  The value for each
    // UnreachableNode's SeqNum (UnreachableNode.SeqNum) MUST be placed in a
    // SeqNum AddrTLV.  If none of UnreachableNode.Addr entries are
    // associated with known prefix lengths, then the AddrBLK SHOULD NOT
    // include any prefix-length information.  Otherwise, for each
    // UnreachableNode.Addr that does not have any associated prefix-length
    // information, the prefix-length for that address MUST be assigned to
    // zero.
    std::vector<Address> unreachableAddresses;
    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IGenericRoute * route = routingTable->getRoute(i);
        DYMORouteData * routeData = dynamic_cast<DYMORouteData *>(route->getProtocolData());
        if (routeData) {
            DYMORouteState routeState = getRouteState(routeData);
            // TODO: check output interface against route interface
            if (routeState != BROKEN && route->getNextHop() == nextHop)
            {
                DYMO_EV << "Marking route as broken " << route << endl;
                route->setEnabled(false);
                routeData->setBroken(true);
                unreachableAddresses.push_back(route->getDestination());
            }
        }
    }
    if (unreachableAddresses.size() == 0)
        DYMO_EV << "No unreachable address found" << endl;
    else
        sendRERR(createRERR(unreachableAddresses));
}

void xDYMO::processRERR(RERR * rerrIncoming) {
    DYMO_EV << "Processing RERR" << endl;
    // 8.4. Receiving and Handling RERR Messages
    // HandlingRtr examines the incoming RERR to assure that it contains
    // Msg.<msg-hop-limit> and at least one UnreachableNode.Address.  If the
    // required information does not exist, the incoming RERR message is
    // disregarded and further processing stopped.
    if (rerrIncoming->getHopLimit() == 0 || rerrIncoming->getUnreachableNodeArraySize() == 0)
        return;
    else {
        IPv4ControlInfo *ipv4ControlInfo = check_and_cast<IPv4ControlInfo *>(rerrIncoming->getControlInfo());
        // Otherwise, for each UnreachableNode.Address, HandlingRtr searches its
        // route table for a route using longest prefix matching.  If no such
        // Route is found, processing is complete for that UnreachableNode.Address.
        std::vector<Address> unreachableAddresses;
        for (int i = 0; i < (int)rerrIncoming->getUnreachableNodeArraySize(); i++) {
            AddressBlock & addressBlock = rerrIncoming->getUnreachableNode(i);
            for (int j = 0; j < routingTable->getNumRoutes(); j++) {
                IGenericRoute * route = routingTable->getRoute(j);
                DYMORouteData *routeData = dynamic_cast<DYMORouteData *>(route->getProtocolData());
                if (routeData) {
                    const Address & unreachableAddress = addressBlock.getAddress();
                    // HandlingRtr verifies the following:
                    // 1. The UnreachableNode.Address is a routable unicast address.
                    // 2. Route.NextHopAddress is the same as RERR IP.SourceAddress.
                    // 3. Route.NextHopInterface is the same as the interface on which the
                    //    RERR was received.
                    // 4. The UnreachableNode.SeqNum is unknown, OR Route.SeqNum <=
                    //    UnreachableNode.SeqNum (using signed 16-bit arithmetic).
                    if (unreachableAddress.isUnicast() &&
                        unreachableAddress == route->getDestination() &&
                        route->getNextHop() == Address(ipv4ControlInfo->getSrcAddr()) &&
                        route->getInterface()->getInterfaceId() == ipv4ControlInfo->getInterfaceId() &&
                        routeData->getSequenceNumber() <= addressBlock.getSequenceNumber())
                    {
                        // If the route satisfies all of the above conditions, HandlingRtr sets
                        // the Route.Broken flag for that route.
                        DYMO_EV << "Marking route as broken " << route << endl;
                        route->setEnabled(false);
                        routeData->setBroken(true);
                        unreachableAddresses.push_back(unreachableAddress);
                    }
                }
            }
        }
        if (unreachableAddresses.size() == 0)
            DYMO_EV << "No unreachable address found" << endl;
        else {
            // Furthermore, if Msg.<msg-hop-limit> is greater than 0, then HandlingRtr
            // adds the UnreachableNode address and TLV information to an AddrBlk for
            // delivery in the outgoing RERR message to one or more of HandlingRtr's
            // upstream neighbors.
            // If there are no UnreachableNode addresses to be transmitted in an
            // RERR to upstream routers, HandlingRtr MUST discard the RERR, and no
            // further action is taken.
            // Otherwise, Msg.<msg-hop-limit> is decremented by one (1) and
            RERR * rerrOutgoing = createRERR(unreachableAddresses);
            rerrOutgoing->setHopLimit(rerrIncoming->getHopLimit() - 1);
            // processing continues as follows:
            // o If precursor lists are (optionally) maintained, the outgoing RERR
            //   SHOULD be sent to the active precursors of the broken route as
            //   specified in Section 13.3.
            // o Otherwise, if the incoming RERR message was received at the LL-
            //   MANET-Routers [RFC5498] multicast address, the outgoing RERR
            //   SHOULD also be sent to LL-MANET-Routers.
            // o Otherwise, if the PktSource MsgTLV is present, and HandlingRtr has
            //   a Route to PktSource.Addr, then HandlingRtr MUST send the outgoing
            //   RERR to Route[PktSource.Addr].NextHop.
            // o Otherwise, the outgoing RERR MUST be sent to LL-MANET-Routers.
            sendRERR(rerrOutgoing);
        }
    }
    delete rerrIncoming;
}

//
// handling routes
//

void xDYMO::updateRoutes(RteMsg * rteMsg, AddressBlock & addressBlock) {
    // 6.1. Evaluating Incoming Routing Information
    // HandRtr searches its route table to see if there is a route table
    // entry with the same MetricType of the RteMsg, matching RteMsg.Addr.
    IGenericRoute * route = NULL;
    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IGenericRoute * routeCandidate = routingTable->getRoute(i);
        DYMORouteData *routeDataCandidate = dynamic_cast<DYMORouteData *>(routeCandidate->getProtocolData());
        // TODO: add check for route->getSource() == this
        if (routeDataCandidate && routeCandidate->getDestination() == addressBlock.getAddress() && routeDataCandidate->getMetricType() == addressBlock.getMetricType()) {
            route = routeCandidate;
            break;
        }
    }
    // If not, HandRtr creates a route table entry for RteMsg.Addr as described
    // in Section 6.2. Otherwise, HandRtr compares the incoming routing information
    /// in RteMsg against the already stored routing information in the route table
    // entry (Route) for RteMsg.Addr, as described below.
    if (!route) {
        IGenericRoute * route = createRoute(rteMsg, addressBlock);
        DYMO_EV << "Adding new route " << route << endl;
        routingTable->addRoute(route);
    }
    else {
        DYMORouteData * routeData = check_and_cast<DYMORouteData *>(route->getProtocolData());
        // Offers improvement if
        // (RteMsg.SeqNum > Route.SeqNum) OR
        // {(RteMsg.SeqNum == Route.SeqNum) AND
        // [(RteMsg.Metric < Route.Metric) OR
        // ((Route.Broken == TRUE) && LoopFree (RteMsg, Route))]}    if
        if ((addressBlock.getSequenceNumber() > routeData->getSequenceNumber()) ||
            (addressBlock.getSequenceNumber() == routeData->getSequenceNumber() && addressBlock.getMetric() < route->getMetric()) ||
            (routeData->getBroken() && isLoopFree(rteMsg, route)))
        {
            // it's more recent, or it's not stale and is shorter, or it can safely repair a broken route
            routingTable->removeRoute(route);
            DYMO_EV << "Updating existing route from " << route;
            updateRoute(rteMsg, addressBlock, route);
            EV << " to " << route << endl;
            routingTable->addRoute(route);
        }
    }
}

IGenericRoute * xDYMO::createRoute(RteMsg * rteMsg, AddressBlock & addressBlock) {
    IGenericRoute * route = routingTable->createRoute();
    route->setProtocolData(new DYMORouteData());
    updateRoute(rteMsg, addressBlock, route);
    return route;
}

void xDYMO::updateRoute(RteMsg * rteMsg, AddressBlock & addressBlock, IGenericRoute * route) {
    // 6.2. Applying Route Updates To Route Table Entries
    IPv4ControlInfo *ipv4ControlInfo = check_and_cast<IPv4ControlInfo *>(rteMsg->getControlInfo());
    DYMORouteData * routeData = check_and_cast<DYMORouteData *>(route->getProtocolData());
    // Route.Address := RteMsg.Addr
    route->setDestination(addressBlock.getAddress());
    // If (RteMsg.PfxLen != 0), then Route.PfxLen := RteMsg.PfxLen
    route->setPrefixLength(addressBlock.getPrefixLength());
    // Route.SeqNum := RteMsg.SeqNum
    routeData->setSequenceNumber(addressBlock.getSequenceNumber());
    // Route.NextHopAddress := IP.SourceAddress (i.e., an address of the node from which the RteMsg was received)
    route->setNextHop(Address(ipv4ControlInfo->getSrcAddr()));
    // Route.NextHopInterface is set to the interface on which RteMsg was received
    InterfaceEntry *interfaceEntry = interfaceTable->getInterfaceById(ipv4ControlInfo->getInterfaceId());
    if (interfaceEntry)
        route->setInterface(interfaceEntry);
    // Route.Broken flag := FALSE
    route->setEnabled(true);
    routeData->setBroken(false);
    // If RteMsg.MetricType is included, then Route.MetricType := RteMsg.MetricType.  Otherwise, Route.MetricType := DEFAULT_METRIC_TYPE.
    // Route.MetricType := RteMsg.MetricType
    // TODO: implement
    routeData->setMetricType(addressBlock.getMetricType());
    // Route.Metric := RteMsg.Metric
    route->setMetric(addressBlock.getMetric());
    // Route.LastUsed := Current_Time
    routeData->setLastUsed(simTime());
    // If RteMsg.VALIDITY_TIME is not included, then Route.ExpirationTime := MAXTIME, otherwise Route.ExpirationTime := Current_Time + RteMsg.VALIDITY_TIME
    simtime_t validityTime = addressBlock.getValidityTime();
    // TODO: implement
    if (validityTime == 0)
        routeData->setExpirationTime(SimTime::getMaxTime());
    else {
        routeData->setExpirationTime(simTime() + validityTime);
        scheduleExpungeTimer();
    }
}

// TODO: use
int xDYMO::getLinkCost(InterfaceEntry * interfaceEntry, DYMOMetricType metricType) {
    switch (metricType) {
        case HOP_COUNT:
            return 1;
        default:
            throw cRuntimeError("Unknown metric type");
    }
}

bool xDYMO::isLoopFree(RteMsg * rteMsg, IGenericRoute * route) {
    // TODO: implement
    return true;
}

//
// handling expunge timer
//

void xDYMO::processExpungeTimer() {
    expungeRoutes();
    scheduleExpungeTimer();
}

void xDYMO::scheduleExpungeTimer() {
    simtime_t expungeTime = getNextExpungeTime();
    if (expungeTime >= simTime()) {
        if (expungeTimer->isScheduled())
            cancelEvent(expungeTimer);
        scheduleAt(getNextExpungeTime(), expungeTimer);
    }
}

void xDYMO::expungeRoutes() {
    DYMO_EV << "Expunging routes from routing table: router = " << getSelfAddress() << endl;
    // 6.3. Route Table Entry Timeouts
    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IGenericRoute * route = routingTable->getRoute(i);
        if (route->getSource() == this) {
            DYMORouteData * routeData = check_and_cast<DYMORouteData *>(route->getProtocolData());
            // An Active route MUST NOT be expunged
            // An Idle route SHOULD NOT be expunged
            // An Expired route MAY be expunged (least recently used first)
            // A route MUST be expunged if (Current_Time - Route.LastUsed) >= MAX_SEQNUM_LIFETIME.
            // A route MUST be expunged if Current_Time >= Route.ExpirationTime
            if ((getRouteState(routeData) == EXPIRED) ||
                (simTime() - routeData->getLastUsed() >= maxSequenceNumberLifetime) ||
                (simTime() >= routeData->getExpirationTime()))
            {
                DYMO_EV << "Expunging route " << route << endl;
                routingTable->removeRoute(route);
                i--;
            }
        }
    }
}

simtime_t xDYMO::getNextExpungeTime() {
    simtime_t nextExpirationTime = 0;
    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IGenericRoute * route = routingTable->getRoute(i);
        if (route->getSource() == this) {
            DYMORouteData * routeData = check_and_cast<DYMORouteData *>(route->getProtocolData());
            const simtime_t & expirationTime = routeData->getExpirationTime();
            if (expirationTime < nextExpirationTime)
                nextExpirationTime = expirationTime;
        }
    }
    return nextExpirationTime;
}

DYMORouteState xDYMO::getRouteState(DYMORouteData * routeData) {
    simtime_t lastUsed = routeData->getLastUsed();
    if (routeData->getBroken())
        return BROKEN;
    else if (lastUsed - simTime() <= activeInterval)
        return ACTIVE;
    else if (routeData->getExpirationTime() != SimTime::getMaxTime()) {
        if (simTime() >= routeData->getExpirationTime())
            return EXPIRED;
        else
            return TIMED;
    }
    else if (lastUsed - simTime() <= maxIdleTime)
        return IDLE;
    else
        return EXPIRED;
}

//
// client address
//

const Address xDYMO::getSelfAddress() {
    return routingTable->getRouterId();
}

bool xDYMO::isClientAddress(const Address & address) {
    if (routingTable->isLocalAddress(address))
        return true;
    else {
        for (std::vector<std::pair<Address, int> >::iterator it = clientNetworks.begin(); it != clientNetworks.end(); it++)
            // TODO: check for prefix length too
            if (it->first == address)
                return true;
        return false;
    }
}

//
// added node
//

void xDYMO::addSelfNode(RteMsg * rteMsg) {
    const Address & address = getSelfAddress();
    AddressBlock * addressBlock = new AddressBlock();
    addressBlock->setSequenceNumber(sequenceNumber);
    addressBlock->setMetric(0);
    addressBlock->setMetricType(HOP_COUNT);
    addressBlock->setAddress(address);
    addressBlock->setPrefixLength(address.getPrefixLength());
    addNode(rteMsg, *addressBlock);
}

void xDYMO::addNode(RteMsg * rteMsg, AddressBlock & addressBlock) {
    int size = rteMsg->getAddedNodeArraySize();
    rteMsg->setAddedNodeArraySize(size + 1);
    rteMsg->setAddedNode(size, addressBlock);
}

//
// sequence number
//

void xDYMO::incrementSequenceNumber() {
    // 5.5.  AODVv2 Sequence Numbers
    // Most of the time, OwnSeqNum is incremented by simply adding one (1).
    // But to increment OwnSeqNum when it has the value of the largest possible
    // number representable as a 16-bit unsigned integer (i.e., 65,535), it MUST
    // be set to one (1). In other words, the sequence number after 65,535 is 1.
    sequenceNumber++;
    if (sequenceNumber == 0)
        sequenceNumber = 1;
}

//
// hook into generic network protocol
//

bool xDYMO::isDYMODatagram(IGenericDatagram * datagram) {
    IPv4Datagram * ipv4Datagram = dynamic_cast<IPv4Datagram *>(datagram);
    IPv4ControlInfo * ipv4ControlInfo = ipv4Datagram ? dynamic_cast<IPv4ControlInfo *>(ipv4Datagram->getControlInfo()) : NULL;
    return ipv4ControlInfo && ipv4ControlInfo->getProtocol() == IP_PROT_MANET;
}

IGenericNetworkProtocol::IHook::Result xDYMO::ensureRouteForDatagram(IGenericDatagram * datagram) {
    const Address & source = datagram->getSourceAddress();
    const Address & destination = datagram->getDestinationAddress();
    if (destination.isMulticast() || destination.isBroadcast() || routingTable->isLocalAddress(destination) || isDYMODatagram(datagram))
        return ACCEPT;
    else {
        DYMO_EV << "Finding route: source = " << getSelfAddress() << ", destination = " << destination << endl;
        IGenericRoute * route = routingTable->findBestMatchingRoute(destination);
        DYMORouteData * routeData = route ? dynamic_cast<DYMORouteData *>(route->getProtocolData()) : NULL;
        bool broken = routeData && routeData->getBroken();
        if (route && !route->getNextHop().isUnspecified() && !broken) {
            DYMO_EV << "Route found: source = " << getSelfAddress() << ", destination = " << destination << ", route = " << route << endl;
            if (routeData)
                // 8.1. Handling Route Lifetimes During Packet Forwarding
                // Route.LastUsed := Current_Time, and the packet is forwarded to the route's next hop.
                routeData->setLastUsed(simTime());
            return ACCEPT;
        }
        else if (source.isUnspecified() || isClientAddress(source)) {
            DYMO_EV << (broken ? "Broken" : "Missing") << " route: source = " << getSelfAddress() << ", destination = " << destination << endl;
            delayDatagram(datagram);
            if (!hasOngoingRouteDiscovery(destination))
                startRouteDiscovery(destination);
            else
                DYMO_EV << "Ongoing route discovery: originator = " << getSelfAddress() << ", target = " << destination << endl;
            return QUEUE;
        }
        else
            // the actual routing decision will be repeated in the network protocol
            return ACCEPT;
    }
}

//
// notifications
//

void xDYMO::receiveChangeNotification(int category, const cObject *details) {
    Enter_Method("receiveChangeNotification");
    if (category == NF_LINK_BREAK) {
        DYMO_EV << "Received link break" << endl;
        Ieee80211Frame *ieee80211Frame = dynamic_cast<Ieee80211Frame *>(const_cast<cObject*>(details));
        if (ieee80211Frame) {
            IPv4Datagram * ipv4Datagram = dynamic_cast<IPv4Datagram *>(ieee80211Frame->getEncapsulatedPacket());
            if (ipv4Datagram && !isDYMODatagram(ipv4Datagram)) {
                // TODO: get nexthop and interface from the packet
                // IPv4ControlInfo * ipv4ControlInfo = dynamic_cast<IPv4ControlInfo *>(ipv4Datagram->getControlInfo());
                const Address & destination = ipv4Datagram->getDestinationAddress();
                IGenericRoute * route = routingTable->findBestMatchingRoute(destination);
                if (route) {
                    const Address & nextHop = route->getNextHop();
                    sendRERRForBrokenLink(route->getInterface(), nextHop);
                }
            }
        }
    }
}

DYMO_NAMESPACE_END
