//
// This program is property of its copyright holder. All rights reserved.
//

#ifndef DYMO_H_
#define DYMO_H_

#include <vector>
#include <map>
#include <omnetpp.h>
#include "InterfaceTableAccess.h"
#include "IGenericNetworkProtocol.h"
#include "IGenericRoutingTable.h"
#include "DYMOdefs.h"
#include "DYMORouteData.h"
#include "DYMO_m.h"

// TODO: KLUDGE: kill this when RoutingTable implements IGenericRoutingTable
#include "RoutingTable.h"

DYMO_NAMESPACE_BEGIN

/**
 * Dynamic MANET On-demand (DYMO also known as AODVv2) Routing
 * based on draft-ietf-manet-dymo-24.
 *
 * Optional features implemented:
 *  - 7.1. Route Discovery Retries and Buffering
 *    To reduce congestion in a network, repeated attempts at route
      discovery for a particular Target Node SHOULD utilize a binary
      exponential backoff.
 *  - 13.1. Expanding Rings Multicast
 *    Increase hop limit from min to max with each retry.
 *  - 13.2. Intermediate RREP
 *    Allow intermediate DYMO routers to reply with RREP.
 *  - 13.6. Message Aggregation
 *    RFC5148 add jitter to broadcasts
 */
class INET_API xDYMO : public cSimpleModule, public IGenericNetworkProtocol::IHook {
  private:
    // context parameters
    const char *routingTableModuleName;
    const char *networkProtocolModuleName;

    // DYMO parameters from RFC
    bool useMulticastRREP;
    double activeInterval;
    double maxIdleTime;
    double maxSequenceNumberLifetime;
    double routeRREQWaitTime;
    double rreqHolddownTime;
    int maxHopCount;
    int discoveryAttemptsMax;
    bool appendInformation;
    int bufferSizePackets;
    int bufferSizeBytes;

    // DYMO extension parameters
    simtime_t maxJitter;
    // TODO: implement
    bool sendIntermediateRREP;
    int minHopLimit;
    int maxHopLimit;

    // context
    IInterfaceTable *interfaceTable;
    IGenericRoutingTable * routingTable;
    IGenericNetworkProtocol * networkProtocol;

    // internal
    DYMOSequenceNumber sequenceNumber;
    cMessage *expungeTimer;
    std::vector<std::pair<Address, int> > clientNetworks; // 5.3.  Router Clients and Client Networks
    std::multimap<Address, IGenericDatagram *> targetAddressToDelayedPackets;
    std::map<Address, RREQTimer *> targetAddressToRREQTimer;
//    UDPSocket socket; // TODO: use raw socket? how on earth could we use a UDP port and a non-UDP IP protocol number?

  public:
    xDYMO();
    virtual ~xDYMO();

    // module interface
    int numInitStages() const { return 5; }
    void initialize(int stage);
    void handleMessage(cMessage * message);

  private:
    // handling messages
    void processSelfMessage(cMessage * message);
    void processMessage(cMessage * message);

    // route discovery
    void startRouteDiscovery(const Address & target);
    void completeRouteDiscovery(const Address & target);
    void cancelRouteDiscovery(const Address & target);
    void eraseRouteDiscovery(const Address & target);
    bool hasOngoingRouteDiscovery(const Address & target);

    // handling IP datagrams
    void delayDatagram(IGenericDatagram * datagram);
    void reinjectDatagram(IGenericDatagram * datagram);
    void dropDatagram(IGenericDatagram * datagram);

    // handling RREQ wait RREP timer
    RREQWaitRREPTimer * createRREQWaitRREPTimer(const Address & target, int retryCount);
    void scheduleRREQWaitRREPTimer(RREQWaitRREPTimer * message);
    void processRREQWaitRREPTimer(RREQWaitRREPTimer * message);

    // handling RREQ backoff timer
    RREQBackoffTimer * createRREQBackoffTimer(const Address & target, int retryCount);
    void scheduleRREQBackoffTimer(RREQBackoffTimer * message);
    void processRREQBackoffTimer(RREQBackoffTimer * message);
    simtime_t computeRREQBackoffTime(int retryCount);

    // handling RREQ holddown timer
    RREQHolddownTimer * createRREQHolddownTimer(const Address & target);
    void scheduleRREQHolddownTimer(RREQHolddownTimer * message);
    void processRREQHolddownTimer(RREQHolddownTimer * message);

    // handling UDP packets
    void sendUDPPacket(UDPPacket * packet);
    void processUDPPacket(UDPPacket * packet);

    // handling DYMO packets
    void sendDYMOPacket(DYMOPacket * packet, InterfaceEntry * interfaceEntry, const IPv4Address & destination);
    void processDYMOPacket(DYMOPacket * packet);

    // handling RteMsg packets
    bool permissibleRteMsg(RteMsg * packet);
    void processRteMsg(RteMsg * packet);

    // handling RREQ packets
    RREQ * createRREQ(const Address & target, int retryCount);
    void sendRREQ(RREQ * packet);
    void processRREQ(RREQ * packet);

    // handling RREP packets
    RREP * createRREP(RteMsg * packet, IGenericRoute * route);
    void sendRREP(RREP * packet);
    void sendRREP(RREP * packet, IGenericRoute * route);
    void processRREP(RREP * packet);

    // handling RERR packets
    RERR * createRERRForUndeliverablePacket();
    RERR * createRERRForBrokenLink();
    void sendRERR(RERR * packet);
    void processRERR(RERR * packet);

    // handling routes
    IGenericRoute * createRoute(RteMsg * packet, AddressBlock & addressBlock);
    void updateRoutes(RteMsg * packet, AddressBlock & addressBlock);
    void updateRoute(RteMsg * packet, AddressBlock & addressBlock, IGenericRoute * route);
    bool isLoopFree(RteMsg * packet, IGenericRoute * route);

    // handling expunge timer
    void processExpungeTimer();
    void scheduleExpungeTimer();
    void expungeRoutes();
    simtime_t getNextExpungeTime();
    DYMORouteState getRouteState(DYMORouteData * routeData);

    // client address
    bool isClientAddress(const Address & address);

    // sequence number
    void incrementSequenceNumber();

    // hook into generic network protocol
    virtual Result datagramPreRoutingHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) { Enter_Method("datagramPreRoutingHook"); return ensureRouteForDatagram(datagram); }
    virtual Result datagramLocalInHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) { return ACCEPT; }
    virtual Result datagramForwardHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry * outputInterfaceEntry, const Address & nextHopAddress) { return ACCEPT; }
    virtual Result datagramPostRoutingHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry * outputInterfaceEntry, const Address & nextHopAddress) { return ACCEPT; }
    virtual Result datagramLocalOutHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) { Enter_Method("datagramLocalOutHook"); return ensureRouteForDatagram(datagram); }
    bool isDYMODatagram(IGenericDatagram * datagram);
    Result ensureRouteForDatagram(IGenericDatagram * datagram);

    // TODO: add link failure listener
};

DYMO_NAMESPACE_END

#endif /* DYMO_H_ */
