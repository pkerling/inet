//
// This program is property of its copyright holder. All rights reserved.
//

#ifndef DYMO_H_
#define DYMO_H_

#include <vector>
#include <map>
#include <omnetpp.h>
#include "INotifiable.h"
#include "IAddressPolicy.h"
#include "INetfilter.h"
#include "IRoutingTable.h"
#include "DYMOdefs.h"
#include "DYMORouteData.h"
#include "DYMO_m.h"

// KLUDGE: kill this when IPv4RoutingTable implements IRoutingTable
#include "IPv4RoutingTable.h"

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
class INET_API xDYMO : public cSimpleModule, public INotifiable, public INetfilter::IHook {
  private:
    // context parameters
    const char * routingTableModuleName;
    const char * networkProtocolModuleName;

    // DYMO parameters from RFC
    const char * clientAddresses;
    bool useMulticastRREP;
    const char * interfaces;
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
    IAddressPolicy * addressPolicy;
    IInterfaceTable * interfaceTable;
    IRoutingTable * routingTable;
    INetfilter * networkProtocol;

    // internal
    cMessage * expungeTimer;
    DYMOSequenceNumber sequenceNumber;
    std::map<Address, DYMOSequenceNumber> targetAddressToSequenceNumber;
    std::map<Address, RREQTimer *> targetAddressToRREQTimer;
    std::multimap<Address, INetworkDatagram *> targetAddressToDelayedPackets;
    std::vector<std::pair<Address, int> > clientAddressAndPrefixLengthPairs; // 5.3.  Router Clients and Client Networks

  public:
    xDYMO();
    virtual ~xDYMO();

  protected:
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
    void delayDatagram(INetworkDatagram * datagram);
    void reinjectDelayedDatagram(INetworkDatagram * datagram);
    void dropDelayedDatagram(INetworkDatagram * datagram);
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
    void sendDYMOPacket(DYMOPacket * packet, const InterfaceEntry * interfaceEntry, const Address & nextHop, double delay);
    void processDYMOPacket(DYMOPacket * packet);

    // handling RteMsg packets
    bool permissibleRteMsg(RteMsg * rteMsg);
    void processRteMsg(RteMsg * rteMsg);
    int computeRteMsgBitLength(RteMsg * rteMsg);

    // handling RREQ packets
    RREQ * createRREQ(const Address & target, int retryCount);
    void sendRREQ(RREQ * rreq);
    void processRREQ(RREQ * rreq);
    int computeRREQBitLength(RREQ * rreq);

    // handling RREP packets
    RREP * createRREP(RteMsg * rteMsg);
    RREP * createRREP(RteMsg * rteMsg, IRoute * route);
    void sendRREP(RREP * rrep);
    void sendRREP(RREP * rrep, IRoute * route);
    void processRREP(RREP * rrep);
    int computeRREPBitLength(RREP * rrep);

    // handling RERR packets
    RERR * createRERR(std::vector<Address> & addresses);
    void sendRERR(RERR * rerr);
    void sendRERRForUndeliverablePacket(const Address & destination);
    void sendRERRForBrokenLink(const InterfaceEntry * interfaceEntry, const Address & nextHop);
    void processRERR(RERR * rerr);
    int computeRERRBitLength(RERR * rerr);

    // handling routes
    IRoute * createRoute(RteMsg * rteMsg, AddressBlock & addressBlock);
    void updateRoutes(RteMsg * rteMsg, AddressBlock & addressBlock);
    void updateRoute(RteMsg * rteMsg, AddressBlock & addressBlock, IRoute * route);
    int getLinkCost(const InterfaceEntry * interfaceEntry, DYMOMetricType metricType);
    bool isLoopFree(RteMsg * rteMsg, IRoute * route);

    // handling expunge timer
    void processExpungeTimer();
    void scheduleExpungeTimer();
    void expungeRoutes();
    simtime_t getNextExpungeTime();
    DYMORouteState getRouteState(DYMORouteData * routeData);

    // handling addresses
    std::string getHostName();
    Address getSelfAddress();
    bool isClientAddress(const Address & address);

    // added node
    void addSelfNode(RteMsg * rteMsg);
    void addNode(RteMsg * rteMsg, AddressBlock & addressBlock);

    // sequence number
    void incrementSequenceNumber();

    // generic network protocol
    virtual Result datagramPreRoutingHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry *& outputInterfaceEntry, Address & nextHopAddress) { Enter_Method("datagramPreRoutingHook"); return ensureRouteForDatagram(datagram); }
    virtual Result datagramForwardHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry *& outputInterfaceEntry, Address & nextHopAddress) { return ACCEPT; }
    virtual Result datagramPostRoutingHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry *& outputInterfaceEntry, Address & nextHopAddress) { return ACCEPT; }
    virtual Result datagramLocalInHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) { return ACCEPT; }
    virtual Result datagramLocalOutHook(INetworkDatagram * datagram, const InterfaceEntry *& outputInterfaceEntry, Address & nextHopAddress) { Enter_Method("datagramLocalOutHook"); return ensureRouteForDatagram(datagram); }
    bool isDYMODatagram(INetworkDatagram * datagram);
    Result ensureRouteForDatagram(INetworkDatagram * datagram);

    // notifications
    virtual void receiveChangeNotification(int category, const cObject * details);
};

DYMO_NAMESPACE_END

#endif /* DYMO_H_ */
