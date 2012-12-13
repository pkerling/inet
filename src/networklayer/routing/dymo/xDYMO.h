//
// This program is property of its copyright holder. All rights reserved.
//

#ifndef DYMO_H_
#define DYMO_H_

#include <vector>
#include <map>
#include <omnetpp.h>
#include "UDPSocket.h"
#include "IGenericRoutingTable.h"
#include "DYMOdefs.h"
#include "DYMORouteData.h"
#include "DYMO_m.h"

DYMO_NAMESPACE_BEGIN

/**
 * Dynamic MANET On-demand (DYMO also known as AODVv2) Routing
 * based on draft-ietf-manet-dymo-24.
 *
 * Optional features implemented:
 *  - To reduce congestion in a network, repeated attempts at route
      discovery for a particular Target Node SHOULD utilize a binary
      exponential backoff.
 *  - 13.1. Expanding Rings Multicast
 *  - 13.2. Intermediate RREP
 */
class INET_API xDYMO : public cSimpleModule {
private:
    // context parameters
    const char *routingTableModuleName;

    // dymo parameters from RFC
    double activeInterval;
    double maxIdleTime;
    double maxSequenceNumberLifetime;
    double routeRREQWaitTime;
    double rreqHolddownTime;
    int maxHopCount;
    int discoveryAttemptsMax;
    int bufferSizePackets;
    int bufferSizeBytes;

    // dymo extension parameters
    bool sendIntermediateRREP;
    int minHopLimit;
    int maxHopLimit;

    // context
    IGenericRoutingTable * routingTable;

    // internal
    DYMOSequenceNumber sequenceNumber;
    std::multimap<Uint128, cPacket *> addressToDelayedPackets;
    std::map<Uint128, RREQTimer *> addressToRREQTimer;
    UDPSocket socket;

public:
    xDYMO();
    virtual ~xDYMO() { }

    // module interface
    int numInitStages() const  { return 1; }
    void initialize(int stage);
    void handleMessage(cMessage * message);

private:
    // route discovery
    void startRouteDiscovery(Address & target);
    void completeRouteDiscovery(Address & target);
    void cancelRouteDiscovery(Address & target);
    bool hasOngoingRouteDiscovery(Address & target);

    // handling IP datagrams
    void delayDatagram(cPacket * packet);
    void reinjectDatagram(cPacket * packet);
    void dropDatagram(cPacket * packet);

    // handling UDP packets
    void sendUDPPacket(UDPPacket * packet);
    void processUDPPacket(UDPPacket * packet);

    // handling RREQ wait RREP timer
    RREQWaitRREPTimer * createRREQWaitRREPTimer(Address & target, int retryCount);
    void sendRREQWaitRREPTimer(RREQWaitRREPTimer * message);
    void processRREQWaitRREPTimer(RREQWaitRREPTimer * message);

    // handling RREQ backoff timer
    RREQBackoffTimer * createRREQBackoffTimer(Address & target, int retryCount);
    void sendRREQBackoffTimer(RREQBackoffTimer * message);
    void processRREQBackoffTimer(RREQBackoffTimer * message);
    simtime_t computeRREQBackoffTime(int retryCount);

    // handling RREQ holddown timer
    RREQHolddownTimer * createRREQHolddownTimer(Address & target);
    void sendRREQHolddownTimer(RREQHolddownTimer * message);
    void processRREQHolddownTimer(RREQHolddownTimer * message);

    // handling DYMO packets
    void sendDYMOPacket(DYMOPacket * packet);
    void processDYMOPacket(DYMOPacket * packet);

    // handling RteMsg packets
    bool permissibleRteMsg(RteMsg * packet);
    void processRteMsg(RteMsg * packet);

    // handling RREQ packets
    RREQ * createRREQ(Address & target, int retryCount);
    void sendRREQ(RREQ * packet);
    void processRREQ(RREQ * packet);

    // handling RREP packets
    RREP * createRREP(RREQ * rreq);
    void sendRREP(RREP * packet);
    void processRREP(RREP * packet);

    // handling RERR packets
    RERR * createRERRForUndeliverablePacket();
    RERR * createRERRForBrokenLink();
    void sendRERR(RERR * packet);
    void processRERR(RERR * packet);

    // handling routes
    IGenericRoute * createRoute();
    void processRoutes(RteMsg * packet);
    void updateRoute(RteMsg * packet, IGenericRoute * route);
    bool isLoopFree(RteMsg * packet, IGenericRoute * route);
    void expungeRoutes();

    // netfilter interface
    void netfilterCallback(cPacket * packet, InterfaceEntry * fromInterface, InterfaceEntry * toInterface, Address & nextHop);

    // utilities
    void incrementSequenceNumber();
    DYMORouteState getRouteState(DYMORouteData * routeData);
};

DYMO_NAMESPACE_END

#endif /* DYMO_H_ */
