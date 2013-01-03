//
// This program is property of its copyright holder. All rights reserved.
//

#ifndef DYMO_H_
#define DYMO_H_

#include <vector>
#include <map>
#include <omnetpp.h>
#include "INotifiable.h"
#include "InterfaceTableAccess.h"
#include "IGenericNetworkProtocol.h"
#include "IGenericRoutingTable.h"
#include "DYMOdefs.h"
#include "DYMORouteData.h"
#include "DYMO_m.h"

// KLUDGE: kill this when RoutingTable implements IGenericRoutingTable
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
class INET_API xDYMO : public cSimpleModule, public INotifiable, public IGenericNetworkProtocol::IHook {
  private:
    // context parameters
    const char * routingTableModuleName;
    const char * networkProtocolModuleName;

    // DYMO parameters from RFC
    const char * clientAddresses;
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
    bool sendIntermediateRREP;
    int minHopLimit;
    int maxHopLimit;

    // context
    NotificationBoard * notificationBoard;
    IInterfaceTable * interfaceTable;
    IGenericRoutingTable * routingTable;
    IGenericNetworkProtocol * networkProtocol;

    // internal
    cMessage * expungeTimer;
    DYMOSequenceNumber sequenceNumber;
    std::map<Address, DYMOSequenceNumber> targetToSequenceNumber;
    std::vector<std::pair<Address, int> > clientNetworks; // 5.3.  Router Clients and Client Networks
    std::multimap<Address, IGenericDatagram *> targetAddressToDelayedPackets;
    std::map<Address, RREQTimer *> targetAddressToRREQTimer;

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
    void retryRouteDiscovery(const Address & target, int retryCount);
    void completeRouteDiscovery(const Address & target);
    void cancelRouteDiscovery(const Address & target);
    bool hasOngoingRouteDiscovery(const Address & target);

    // handling IP datagrams
    void delayDatagram(IGenericDatagram * datagram);
    void reinjectDelayedDatagram(IGenericDatagram * datagram);
    void dropDelayedDatagram(IGenericDatagram * datagram);
    void eraseDelayedDatagrams(const Address & target);
    bool hasDelayedDatagrams(const Address & target);

    // handling RREQ timers
    void cancelRREQTimer(const Address & target);
    void deleteRREQTimer(const Address & target);
    void eraseRREQTimer(const Address & target);

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
    void sendUDPPacket(UDPPacket * packet, double delay);
    void processUDPPacket(UDPPacket * packet);

    // handling DYMO packets
    void sendDYMOPacket(DYMOPacket * packet, InterfaceEntry * interfaceEntry, const IPv4Address & nextHop, double delay);
    void processDYMOPacket(DYMOPacket * packet);

    // handling RteMsg packets
    bool permissibleRteMsg(RteMsg * rteMsg);
    void processRteMsg(RteMsg * rteMsg);

    // handling RREQ packets
    RREQ * createRREQ(const Address & target, int retryCount);
    void sendRREQ(RREQ * rreq);
    void processRREQ(RREQ * rreq);

    // handling RREP packets
    RREP * createRREP(RteMsg * rteMsg);
    RREP * createRREP(RteMsg * rteMsg, IGenericRoute * route);
    void sendRREP(RREP * rrep);
    void sendRREP(RREP * rrep, IGenericRoute * route);
    void processRREP(RREP * rrep);

    // handling RERR packets
    RERR * createRERR(std::vector<Address> & addresses);
    void sendRERR(RERR * rerr);
    void sendRERRForUndeliverablePacket(const Address & destination);
    void sendRERRForBrokenLink(InterfaceEntry * interfaceEntry, const Address & nextHop);
    void processRERR(RERR * rerr);

    // handling routes
    IGenericRoute * createRoute(RteMsg * rteMsg, AddressBlock & addressBlock);
    void updateRoutes(RteMsg * rteMsg, AddressBlock & addressBlock);
    void updateRoute(RteMsg * rteMsg, AddressBlock & addressBlock, IGenericRoute * route);
    int getLinkCost(InterfaceEntry * interfaceEntry, DYMOMetricType metricType);
    bool isLoopFree(RteMsg * rteMsg, IGenericRoute * route);

    // handling expunge timer
    void processExpungeTimer();
    void scheduleExpungeTimer();
    void expungeRoutes();
    simtime_t getNextExpungeTime();
    DYMORouteState getRouteState(DYMORouteData * routeData);

    // client address
    const Address getSelfAddress();
    bool isClientAddress(const Address & address);

    // added node
    void addSelfNode(RteMsg * rteMsg);
    void addNode(RteMsg * rteMsg, AddressBlock & addressBlock);

    // sequence number
    void incrementSequenceNumber();

    // generic network protocol
    virtual Result datagramPreRoutingHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) { Enter_Method("datagramPreRoutingHook"); return ensureRouteForDatagram(datagram); }
    virtual Result datagramLocalInHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) { return ACCEPT; }
    virtual Result datagramForwardHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry * outputInterfaceEntry, const Address & nextHopAddress) { return ACCEPT; }
    virtual Result datagramPostRoutingHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry * outputInterfaceEntry, const Address & nextHopAddress) { return ACCEPT; }
    virtual Result datagramLocalOutHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) { Enter_Method("datagramLocalOutHook"); return ensureRouteForDatagram(datagram); }
    bool isDYMODatagram(IGenericDatagram * datagram);
    Result ensureRouteForDatagram(IGenericDatagram * datagram);

    // notifications
    virtual void receiveChangeNotification(int category, const cObject * details);
};

DYMO_NAMESPACE_END

#endif /* DYMO_H_ */
