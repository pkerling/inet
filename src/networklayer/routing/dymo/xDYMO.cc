//
// This program is property of its copyright holder. All rights reserved.
//

#include "UDPPacket.h"
#include "UDPControlInfo.h"
#include "xDYMO.h"

DYMO_NAMESPACE_BEGIN

Define_Module(DYMO::xDYMO);

//
// construction
//

xDYMO::xDYMO() {
    routingTable = NULL;
    networkProtocol = NULL;
}

xDYMO::~xDYMO() {
    for (std::map<Address, RREQTimer *>::iterator it = targetAddressToRREQTimer.begin(); it != targetAddressToRREQTimer.end(); it++)
        cancelAndDelete(it->second);
}

//
// module interface
//

void xDYMO::initialize(int stage) {
    if (stage == 0) {
        // context parameters
        routingTableModuleName = par("routingTableModuleName");
        networkProtocolModuleName = par("networkProtocolModuleName");
        // dymo parameters from RFC
        activeInterval = par("activeInterval");
        maxIdleTime = par("maxIdleTime");
        maxSequenceNumberLifetime = par("maxSequenceNumberLifetime");
        routeRREQWaitTime = par("routeRREQWaitTime");
        rreqHolddownTime = par("rreqHolddownTime");
        maxHopCount = par("maxHopCount");
        discoveryAttemptsMax = par("discoveryAttemptsMax");
        bufferSizePackets = par("bufferSizePackets");
        bufferSizeBytes = par("bufferSizeBytes");
        // dymo extension parameters
        sendIntermediateRREP = par("sendIntermediateRREP");
        minHopLimit = par("minHopLimit");
        maxHopLimit = par("maxHopLimit");
        // context
        interfaceTable = InterfaceTableAccess().get(this);
        routingTable = check_and_cast<RoutingTable *>(findModuleWhereverInNode(routingTableModuleName, this))->asGeneric();
        networkProtocol = check_and_cast<IGenericNetworkProtocol *>(findModuleWhereverInNode(networkProtocolModuleName, this));
        // internal
        sequenceNumber = 1;
        socket.setOutputGate(gate("udpOut"));
        socket.bind(DYMO_UDP_PORT);
        socket.setMulticastLoop(false);
        socket.joinMulticastGroup(IPv4Address::LL_MANET_ROUTERS, interfaceTable->getInterfaceByName("wlan0")->getInterfaceId());
        // hook to network protocol
        networkProtocol->registerHook(0, this);
    }
}

void xDYMO::handleMessage(cMessage * message) {
    if (message->isSelfMessage()) {
        if (dynamic_cast<RREQWaitRREPTimer *>(message))
            processRREQWaitRREPTimer((RREQWaitRREPTimer *)message);
        else if (dynamic_cast<RREQBackoffTimer *>(message))
            processRREQBackoffTimer((RREQBackoffTimer *)message);
        else if (dynamic_cast<RREQHolddownTimer *>(message))
            processRREQHolddownTimer((RREQHolddownTimer *)message);
        else
            throw cRuntimeError("Unknown message");
    }
    else if (dynamic_cast<DYMOPacket *>(message))
        processDYMOPacket((DYMOPacket *)message);
    else
        throw cRuntimeError("Unknown message");
}

//
// route discovery
//

void xDYMO::startRouteDiscovery(const Address & target) {
    ASSERT(!hasOngoingRouteDiscovery(target));
    sendRREQ(createRREQ(target, 0));
    sendRREQWaitRREPTimer(createRREQWaitRREPTimer(target, 0));
}

void xDYMO::completeRouteDiscovery(const Address & target) {
    ASSERT(hasOngoingRouteDiscovery(target));
    std::multimap<Address, IGenericDatagram *>::iterator lt = targetAddressToDelayedPackets.lower_bound(target);
    std::multimap<Address, IGenericDatagram *>::iterator ut = targetAddressToDelayedPackets.upper_bound(target);
    for (std::multimap<Address, IGenericDatagram *>::iterator it = lt; it != ut; it++)
        reinjectDatagram(it->second);
    targetAddressToDelayedPackets.erase(lt, ut);
    cancelAndDelete(targetAddressToRREQTimer[target]); // TODO:
}

void xDYMO::cancelRouteDiscovery(const Address & target) {
    ASSERT(hasOngoingRouteDiscovery(target));
    std::multimap<Address, IGenericDatagram *>::iterator lt = targetAddressToDelayedPackets.lower_bound(target);
    std::multimap<Address, IGenericDatagram *>::iterator ut = targetAddressToDelayedPackets.upper_bound(target);
    for (std::multimap<Address, IGenericDatagram *>::iterator it = lt; it != ut; it++)
        dropDatagram(it->second);
    targetAddressToDelayedPackets.erase(lt, ut);
    cancelAndDelete(targetAddressToRREQTimer[target]); // TODO:
}

bool xDYMO::hasOngoingRouteDiscovery(const Address & target) {
    return targetAddressToRREQTimer.find(target) != targetAddressToRREQTimer.end();
}

//
// handling IP datagrams
//

void xDYMO::delayDatagram(IGenericDatagram * datagram) {
    const Address & target = datagram->getDestinationAddress();
    targetAddressToDelayedPackets.insert(std::pair<Address, IGenericDatagram *>(target, datagram));
}

void xDYMO::reinjectDatagram(IGenericDatagram * datagram) {
    // TODO:
}

void xDYMO::dropDatagram(IGenericDatagram * datagram) {
    // TODO: send ICMP destination unreachable error
    delete datagram;
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

void xDYMO::sendRREQWaitRREPTimer(RREQWaitRREPTimer * message) {
    targetAddressToRREQTimer[message->getTarget()] = message;
    scheduleAt(simTime() + routeRREQWaitTime, message);
}

void xDYMO::processRREQWaitRREPTimer(RREQWaitRREPTimer * message) {
    const Address & target = message->getTarget();
    if (message->getRetryCount() == discoveryAttemptsMax - 1) {
        cancelRouteDiscovery(target);
        sendRREQHolddownTimer(createRREQHolddownTimer(target));
    }
    else
        sendRREQBackoffTimer(createRREQBackoffTimer(target, message->getRetryCount()));
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

void xDYMO::sendRREQBackoffTimer(RREQBackoffTimer * message) {
    targetAddressToRREQTimer[message->getTarget()] = message;
    scheduleAt(simTime() + computeRREQBackoffTime(message->getRetryCount()), message);
}

void xDYMO::processRREQBackoffTimer(RREQBackoffTimer * message) {
    int retryCount = message->getRetryCount() + 1;
    const Address & target = message->getTarget();
    sendRREQ(createRREQ(target, retryCount));
    sendRREQWaitRREPTimer(createRREQWaitRREPTimer(target, retryCount));
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

void xDYMO::sendRREQHolddownTimer(RREQHolddownTimer * message) {
    targetAddressToRREQTimer[message->getTarget()] = message;
    scheduleAt(simTime() + rreqHolddownTime, message);
}

void xDYMO::processRREQHolddownTimer(RREQHolddownTimer * message) {
    std::map<Address, RREQTimer *>::iterator it = targetAddressToRREQTimer.find(message->getTarget());
    targetAddressToRREQTimer.erase(it);
    delete message;
}

//
// handling DYMO packets
//

void xDYMO::sendDYMOPacket(DYMOPacket * packet) {
    socket.sendTo(packet, IPv4Address::LL_MANET_ROUTERS, DYMO_UDP_PORT);
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

bool xDYMO::permissibleRteMsg(RteMsg * packet) {
    // 7.5. Handling a Received RteMsg
    // 1. HandlingRtr MUST handle AODVv2 messages only from adjacent
    //    routers as specified in Section 5.4. AODVv2 messages from other
    //    sources MUST be disregarded.
    // 2. If the RteMsg.<msg-hop-limit> is equal to 0, then the message is disregarded.
    if (packet->getHopLimit() == 0)
        return false;
    // 3. If the RteMsg.<msg-hop-count> is present, and RteMsg.<msg-hop-
    //    count> >= MAX_HOPCOUNT, then the message is disregarded.
    if (packet->getHopCount() >= maxHopCount)
        return false;
    // 4. HandlingRtr examines the RteMsg to ascertain that it contains the
    //    required information: TargNode.Addr, OrigNode.Addr,
    //    RteMsg_Gen.Metric and RteMsg_Gen.SeqNum.  If the required
    //    information does not exist, the message is disregarded.
    // 5. HandlingRtr checks that OrigNode.Addr and TargNode.Addr are valid
    //    routable unicast addresses.  If not, the message is disregarded.
    // 6. HandlingRtr checks that the Metric Type associated with
    //    OrigNode.Metric and TargNode.Metric is known, and that Cost(L)
    //    can be computed.  If not, the message is disregarded.
    //     *  DISCUSSION: alternatively, can change the AddrBlk metric to
    //        use HopCount, measured from<msg-hop-limit>.
    // 7. If MAX_METRIC[RteMsg.MetricType] <= (RteMsg_Gen.Metric +
    //    Cost(L)), where 'L' is the incoming link, the RteMsg is
    //    disregarded.
    return true;
}

void xDYMO::processRteMsg(RteMsg * packet) {
    if (permissibleRteMsg(packet)) {
        // 7.5. Handling a Received RteMsg
        // 1. HandlingRtr MUST process the routing information contained in the
        //    RteMsg as speciied in Section 6.1.
        processRoutes(packet);
        // 2. HandlingRtr MAY process AddedNode routing information (if
        //    present) as specified in Section 13.7.1 Otherwise, if AddedNode
        //    information is not processed, it MUST be deleted.
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
        // 4. HandlingRtr MUST decrement RteMsg.<msg-hop-limit>.  If
        //    RteMsg.<msg-hop-limit> is then zero (0), no further action is taken.
        packet->setHopLimit(packet->getHopLimit() - 1);
        // 5. HandlingRtr MUST increment RteMsg.<msg-hop-count>.
        packet->setHopCount(packet->getHopCount() + 1);
    }
    else
        delete packet;
}

//
// handling RREQ packets
//

RREQ * xDYMO::createRREQ(const Address & target, int retryCount) {
    RREQ * packet = new RREQ("RREQ");
    AddressBlock & originatorNode = packet->getOriginatorNode();
    AddressBlock & targetNode = packet->getTargetNode();
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
    packet->setHopCount(0);
    //    *  This RFC 5444 constraint causes the typical RteMsg payload
    //       incur additional enlargement.
    // 5. RREQ_Gen adds the TargNode.Addr to the RREQ.
    targetNode.setAddress(target);
    // 6. If a previous value of the TargNode's SeqNum is known RREQ_Gen SHOULD
    //    include TargNode.SeqNum in all but the last RREQ attempt.
    if (retryCount < discoveryAttemptsMax - 1)
        targetNode.setSequenceNumber(-1); // TODO:
    // 7. RREQ_Gen adds OrigNode.Addr, its prefix, and the RREQ_Gen.SeqNum
    //    (OwnSeqNum) to the RREQ.
    Address address; // TODO:
    int prefixLength = 0; // TODO:
    originatorNode.setAddress(address);
    originatorNode.setPrefixLength(prefixLength);
    originatorNode.setSequenceNumber(sequenceNumber);
    // 8. If OrigNode.Metric is included it is set to the cost of the route
    //    between OrigNode and RREQ_Gen.

    // expanding ring search
    int hopLimit = minHopLimit + (maxHopLimit - minHopLimit) * retryCount / discoveryAttemptsMax;
    packet->setHopLimit(hopLimit);
    return packet;
}

void xDYMO::sendRREQ(RREQ * packet) {
    // 7.5.1. Additional Handling for Outgoing RREQ
    // o If the upstream router is in the Blacklist, and Current_Time <
    //   BlacklistRmTime, then HandlingRtr MUST NOT transmit any outgoing
    //   RREQ, and processing is complete.
    // o Otherwise, if the upstream router is in the Blacklist, and
    //   Current_Time >= BlacklistRmTime, then the upstream router SHOULD
    //   be removed from the Blacklist, and message processing continued.
    // o If TargNode is a client of HandlingRtr, then a RREP is generated
    //   by the HandlingRtr (i.e., TargRtr) and unicast to the upstream
    //   router towards the RREQ OrigNode, as specified in Section 7.4.
    //   Afterwards, TargRtr processing for the RREQ is complete.
    // o If HandlingRtr is not the TargetNode, then the outgoing RREQ (as
    //   altered by the procedure defined above) SHOULD be sent to the IP
    //   multicast address LL-MANET-Routers [RFC5498].  If the RREQ is
    //   unicast, the IP.DestinationAddress is set to the NextHopAddress.
    sendDYMOPacket(packet);
}

void xDYMO::processRREQ(RREQ * packet) {
    processRteMsg(packet);
}

//
// handling RREP packets
//

RREP * xDYMO::createRREP(RREQ * rreq) {
    RREP * rrep = new RREP("RREP");
    AddressBlock & originatorNode = rrep->getOriginatorNode();
    AddressBlock & targetNode = rrep->getTargetNode();
    // 1. RREP_Gen first uses the routing information to update its route
    //    table entry for OrigNode if necessary as specified in
    //    Section 6.2.
    // 2. RREP_Gen MUST increment its OwnSeqNum by one (1) according to
    //    the rules specified in Section 5.5.
    incrementSequenceNumber();
    // 3. RREP.AddrBlk[OrigNode] := RREQ.AddrBlk[OrigNode]
    originatorNode = AddressBlock(rreq->getOriginatorNode());
    // 4. RREP.AddrBlk[TargNode] := RREQ.AddrBlk[TargNode]
    targetNode = AddressBlock(rreq->getTargetNode());
    // 5. RREP.SeqNumTLV[OrigNode] := RREQ.SeqNumTLV[OrigNode]
    originatorNode.setSequenceNumber(rreq->getOriginatorNode().getSequenceNumber());
    // 6. RREP.SeqNumTLV[TargNode] := OwnSeqNum
    targetNode.setSequenceNumber(sequenceNumber);
    // 7. If Route[TargNode].PfxLen/8 is equal to the number of bytes in
    //    the addresses of the RREQ (4 for IPv4, 16 for IPv6), then no
    //    <prefix-length> is included with the iRREP.  Otherwise,
    //    RREP.PfxLen[TargNode] := RREQ.PfxLen[TargNode] according to the
    //    rules of RFC 5444 AddrBlk encoding.
    // 8. RREP.MetricType[TargNode] := Route[TargNode].MetricType
    // 9. RREP.Metric[TargNode] := Route[TargNode].Metric
    rrep->setMetric(-1); // TODO
    // 10. <msg-hop-limit> SHOULD be set to RteMsg.<msg-hop-count>.
    rrep->setHopLimit(rreq->getHopCount());
    // 11. IP.DestinationAddr := Route[OrigNode].NextHop
    return rrep;
}

void xDYMO::sendRREP(RREP * packet) {
    // 7.5.2. Additional Handling for Outgoing RREP
    // o If HandlingRtr is not OrigRtr then the outgoing RREP is sent to
    //   the Route.NextHopAddress for the RREP.AddrBlk[OrigNode].  If no
    //   forwarding route exists to OrigNode, then a RERR SHOULD be
    //   transmitted to RREP.AddrBlk[TargNode].  See Table 1 for notational
    //   conventions; OrigRtr, OrigNode, and TargNode are routers named in
    //   the context of OrigRtr, that is, the router originating the RREQ
    //   to which the RREP is responding.
    sendDYMOPacket(packet);
}

void xDYMO::processRREP(RREP * packet) {
    processRteMsg(packet);
}

//
// handling RERR packets
//

RERR * xDYMO::createRERRForUndeliverablePacket() {
    RERR * packet = new RERR();
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
    return packet;
}

RERR * xDYMO::createRERRForBrokenLink() {
    RERR * packet = new RERR();
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
   return packet;
}

void xDYMO::sendRERR(RERR * packet) {
    sendDYMOPacket(packet);
}

void xDYMO::processRERR(RERR * packet) {
    // 8.4. Receiving and Handling RERR Messages
    // HandlingRtr examines the incoming RERR to assure that it contains
    // Msg.<msg-hop-limit> and at least one UnreachableNode.Address.  If the
    // required information does not exist, the incoming RERR message is
    // disregarded and further processing stopped.  Otherwise, for each
    // UnreachableNode.Address, HandlingRtr searches its route table for a
    // route using longest prefix matching.  If no such Route is found,
    // processing is complete for that UnreachableNode.Address.  Otherwise,
    // HandlingRtr verifies the following:
    // 1. The UnreachableNode.Address is a routable unicast address.
    // 2. Route.NextHopAddress is the same as RERR IP.SourceAddress.
    // 3. Route.NextHopInterface is the same as the interface on which the
    //    RERR was received.
    // 4. The UnreachableNode.SeqNum is unknown, OR Route.SeqNum <=
    //    UnreachableNode.SeqNum (using signed 16-bit arithmetic).
    // If the route satisfies all of the above conditions, HandlingRtr sets
    // the Route.Broken flag for that route.  Furthermore, if Msg.<msg-hop-
    // limit> is greater than 0, then HandlingRtr adds the UnreachableNode
    // address and TLV information to an AddrBlk for for delivery in the
    // outgoing RERR message to one or more of HandlingRtr's upstream
    // neighbors.
    // If there are no UnreachableNode addresses to be transmitted in an
    // RERR to upstream routers, HandlingRtr MUST discard the RERR, and no
    // further action is taken.
    // Otherwise, Msg.<msg-hop-limit> is decremented by one (1) and
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
}

//
// handling routes
//

void xDYMO::processRoutes(RteMsg * packet) {
    // 6.1. Evaluating Incoming Routing Information
    Address & target = packet->getTargetNode().getAddress();
    // find out the matching route by destination address and metric type and source (this pointer, not address)
    IGenericRoute * route = NULL;
    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IGenericRoute * routeCandidate = routingTable->getRoute(i);
        DYMORouteData *routeDataCandidate = dynamic_cast<DYMORouteData *>(routeCandidate->getProtocolData());
        if (routeDataCandidate && routeCandidate->getDestination() == target) { // TODO: && route->getSource() == this
            route = routeCandidate;
            break;
        }
    }
    if (!route)
        routingTable->addRoute(createRoute(packet));
    else {
        DYMORouteData * routeData = check_and_cast<DYMORouteData *>(route->getProtocolData());
        // Offers improvement if
        // (RteMsg.SeqNum > Route.SeqNum) OR
        // {(RteMsg.SeqNum == Route.SeqNum) AND
        // [(RteMsg.Metric < Route.Metric) OR
        // ((Route.Broken == TRUE) && LoopFree (RteMsg, Route))]}    if
        if ((packet->getSequenceNumber() > routeData->getSequenceNumber()) ||
            (packet->getSequenceNumber() == routeData->getSequenceNumber() && packet->getMetric() < route->getMetric()) ||
            (routeData->getBroken() && isLoopFree(packet, route)))
        {
            // it's more recent, or it's not stale and is shorter, or it can safely repair a broken route
            updateRoute(packet, route);
        }
    }
}

IGenericRoute * xDYMO::createRoute(RteMsg * packet) {
    Address & target = packet->getTargetNode().getAddress();
    UDPDataIndication *udpData = check_and_cast<UDPDataIndication *>(packet->getControlInfo());
    IGenericRoute *route = routingTable->createRoute();
    route->setDestination(target);
    route->setPrefixLength(32); // TODO:
    route->setNextHop(target); // TODO:
    InterfaceEntry *interfaceEntry = interfaceTable->getInterfaceById(udpData->getInterfaceId());
    if (interfaceEntry)
        route->setInterface(interfaceEntry);
    return route;
}

void xDYMO::updateRoute(RteMsg * packet, IGenericRoute * route) {
    // 6.2. Applying Route Updates To Route Table Entries
    UDPDataIndication *udpData = check_and_cast<UDPDataIndication *>(packet->getControlInfo());
    DYMORouteData * routeData = check_and_cast<DYMORouteData *>(route->getProtocolData());
    // Route.Address := RteMsg.Addr
    // If (RteMsg.PfxLen != 0), then Route.PfxLen := RteMsg.PfxLen
    // Route.SeqNum := RteMsg.SeqNum
    routeData->setSequenceNumber(packet->getSequenceNumber());
    // Route.NextHopAddress := IP.SourceAddress (i.e., an address of the node from which the RteMsg was received)
    // TODO: route->setNextHop(udpData->getSrcAddr());
    // Route.NextHopInterface is set to the interface on which RteMsg was received
    InterfaceEntry *interfaceEntry = interfaceTable->getInterfaceById(udpData->getInterfaceId());
    if (interfaceEntry)
        route->setInterface(interfaceEntry);
    // Route.Broken flag := FALSE
    routeData->setBroken(false);
    // If RteMsg.MetricType is included, then Route.MetricType := RteMsg.MetricType.  Otherwise, Route.MetricType := DEFAULT_METRIC_TYPE.
    // Route.MetricType := RteMsg.MetricType
    routeData->setMetricType(packet->getMetricType()); // TODO:
    // Route.Metric := RteMsg.Metric
    route->setMetric(packet->getMetric());
    // Route.LastUsed := Current_Time
    routeData->setLastUsed(simTime());
    // If RteMsg.VALIDITY_TIME is not included, then Route.ExpirationTime := MAXTIME, otherwise Route.ExpirationTime := Current_Time + RteMsg.VALIDITY_TIME
    routeData->setExpirationTime(0); // TODO:
}

bool xDYMO::isLoopFree(RteMsg * packet, IGenericRoute * route) {
    // TODO:
    return true;
}

void xDYMO::expungeRoutes() {
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
                (routeData->getExpirationTime() > 0 && simTime() >= routeData->getExpirationTime()))
            {
                routingTable->removeRoute(route);
                i--;
            }
        }
    }
}

//
// netfilter interface
//

IGenericNetworkProtocol::IHook::Result xDYMO::ensureRouteForDatagram(IGenericDatagram * datagram) {
    expungeRoutes();
    const Address & destination = datagram->getDestinationAddress();
    if (destination.isMulticast() || destination.isBroadcast())
        return ACCEPT;
    else {
        const Address & nextHopAddress = routingTable->getNextHopForDestination(destination);
        if (!nextHopAddress.isUnspecified())
            return ACCEPT;
        else {
            delayDatagram(datagram);
            if (!hasOngoingRouteDiscovery(destination))
                startRouteDiscovery(destination);
            return QUEUE;
        }
    }
}

//
// utilities
//

void xDYMO::incrementSequenceNumber() {
    sequenceNumber++;
    if (sequenceNumber == 0)
        sequenceNumber = 1;
}

DYMORouteState xDYMO::getRouteState(DYMORouteData * routeData) {
    simtime_t lastUsed = routeData->getLastUsed();
    if (routeData->getBroken())
        return BROKEN;
    else if (lastUsed - simTime() <= activeInterval)
        return ACTIVE;
    else if (routeData->getExpirationTime() != 0) {
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

DYMO_NAMESPACE_END
