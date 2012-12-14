//
// This program is property of its copyright holder. All rights reserved.
//

#ifndef DYMO_H_
#define DYMO_H_

#include <vector>
#include <map>
#include <omnetpp.h>
#include "InterfaceTableAccess.h"
#include "UDPSocket.h"
#include "IGenericNetworkProtocol.h"
#include "IGenericRoutingTable.h"
#include "DYMOdefs.h"
#include "DYMORouteData.h"
#include "DYMO_m.h"

// TODO: KLUDGE: kill these
#include "RoutingTable.h"

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
class INET_API xDYMO : public cSimpleModule, public IGenericNetworkProtocol::IHook {
private:
    // context parameters
    const char *routingTableModuleName;
    const char *networkProtocolModuleName;

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
    IInterfaceTable *interfaceTable;
    IGenericRoutingTable * routingTable;
    IGenericNetworkProtocol * networkProtocol;

    // internal
    DYMOSequenceNumber sequenceNumber;
    std::multimap<Address, IGenericDatagram *> targetAddressToDelayedPackets;
    std::map<Address, RREQTimer *> targetAddressToRREQTimer;
    UDPSocket socket;

public:
    xDYMO();
    virtual ~xDYMO();

    // module interface
    int numInitStages() const  { return 1; }
    void initialize(int stage);
    void handleMessage(cMessage * message);

private:
    // route discovery
    void startRouteDiscovery(const Address & target);
    void completeRouteDiscovery(const Address & target);
    void cancelRouteDiscovery(const Address & target);
    bool hasOngoingRouteDiscovery(const Address & target);

    // handling IP datagrams
    void delayDatagram(IGenericDatagram * datagram);
    void reinjectDatagram(IGenericDatagram * datagram);
    void dropDatagram(IGenericDatagram * datagram);

    // handling RREQ wait RREP timer
    RREQWaitRREPTimer * createRREQWaitRREPTimer(const Address & target, int retryCount);
    void sendRREQWaitRREPTimer(RREQWaitRREPTimer * message);
    void processRREQWaitRREPTimer(RREQWaitRREPTimer * message);

    // handling RREQ backoff timer
    RREQBackoffTimer * createRREQBackoffTimer(const Address & target, int retryCount);
    void sendRREQBackoffTimer(RREQBackoffTimer * message);
    void processRREQBackoffTimer(RREQBackoffTimer * message);
    simtime_t computeRREQBackoffTime(int retryCount);

    // handling RREQ holddown timer
    RREQHolddownTimer * createRREQHolddownTimer(const Address & target);
    void sendRREQHolddownTimer(RREQHolddownTimer * message);
    void processRREQHolddownTimer(RREQHolddownTimer * message);

    // handling DYMO packets
    void sendDYMOPacket(DYMOPacket * packet);
    void processDYMOPacket(DYMOPacket * packet);

    // handling RteMsg packets
    bool permissibleRteMsg(RteMsg * packet);
    void processRteMsg(RteMsg * packet);

    // handling RREQ packets
    RREQ * createRREQ(const Address & target, int retryCount);
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
    IGenericRoute * createRoute(RteMsg * packet);
    void processRoutes(RteMsg * packet);
    void updateRoute(RteMsg * packet, IGenericRoute * route);
    bool isLoopFree(RteMsg * packet, IGenericRoute * route);
    void expungeRoutes();

    // hook into network protocol
    virtual Result datagramPreRoutingHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) { Enter_Method("datagramPreRoutingHook"); return ensureRouteForDatagram(datagram); }
    virtual Result datagramLocalInHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) { return ACCEPT; }
    virtual Result datagramForwardHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry * outputInterfaceEntry, const Address & nextHopAddress) { return ACCEPT; }
    virtual Result datagramPostRoutingHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry * outputInterfaceEntry, const Address & nextHopAddress) { return ACCEPT; }
    virtual Result datagramLocalOutHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) { Enter_Method("datagramLocalOutHook"); return ensureRouteForDatagram(datagram); }
    Result ensureRouteForDatagram(IGenericDatagram * datagram);

    // utilities
    void incrementSequenceNumber();
    DYMORouteState getRouteState(DYMORouteData * routeData);
};

DYMO_NAMESPACE_END

#endif /* DYMO_H_ */
