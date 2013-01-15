//
// This program is property of its copyright holder. All rights reserved.
// 

#ifndef __INET_GPSR_H_
#define __INET_GPSR_H_

#include "INETDefs.h"
#include "Coord.h"
#include "INotifiable.h"
#include "IAddressPolicy.h"
#include "INetfilter.h"
#include "NotificationBoard.h"
#include "LocationTable.h"

class INET_API GPSR : public cSimpleModule, public INotifiable, public INetfilter::IHook {
    private:
        // context parameters
        const char * networkProtocolModuleName;

        // context
        NotificationBoard * notificationBoard;
        IAddressPolicy * addressPolicy;
        INetfilter * networkProtocol;
        LocationTable * globalLocationTable; // TODO: use a protocol message for lookup?

        // internal
        LocationTable neighborLocationTable;

    public:
        GPSR();
        virtual ~GPSR();

    protected:
        // module interface
        int numInitStages() const { return 5; }
        void initialize(int stage);
        void handleMessage(cMessage * message);

    private:
        // handling messages
        void processSelfMessage(cMessage * message);
        void processMessage(cMessage * message);

        // handling positions
        Coord getLocation(const Address & address);
        Coord findNextHop(const Address & destination);

        // handling addresses
        std::string getHostName();
        Address getSelfAddress();

        // generic network protocol
        virtual Result datagramPreRoutingHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) { Enter_Method("datagramPreRoutingHook"); return ensureRouteForDatagram(datagram); }
        virtual Result datagramLocalInHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) { return ACCEPT; }
        virtual Result datagramForwardHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry *& outputInterfaceEntry, Address & nextHopAddress) { return ACCEPT; }
        virtual Result datagramPostRoutingHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry *& outputInterfaceEntry, Address & nextHopAddress) { return ACCEPT; }
        virtual Result datagramLocalOutHook(INetworkDatagram * datagram, const InterfaceEntry *& outputInterfaceEntry) { Enter_Method("datagramLocalOutHook"); return ensureRouteForDatagram(datagram); }
        bool isGPSRDatagram(INetworkDatagram * datagram);
        Result ensureRouteForDatagram(INetworkDatagram * datagram);

        // notifications
        virtual void receiveChangeNotification(int category, const cObject * details);
};

#endif
