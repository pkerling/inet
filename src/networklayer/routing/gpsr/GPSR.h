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

#ifndef __INET_GPSR_H_
#define __INET_GPSR_H_

#include "INETDefs.h"
#include "Coord.h"
#include "INotifiable.h"
#include "IMobility.h"
#include "IAddressPolicy.h"
#include "INetfilter.h"
#include "IRoutingTable.h"
#include "NotificationBoard.h"
#include "PositionTable.h"
#include "UDPPacket.h"
#include "GPSR_m.h"

class INET_API GPSR : public cSimpleModule, public INotifiable, public INetfilter::IHook {
    private:
        // context parameters
        const char * routingTableModuleName; // TODO: delete when necessary functions are moved to interface table
        const char * networkProtocolModuleName;

        // gpsr parameters
        const char * interfaces;
        simtime_t beaconInterval;
        simtime_t maxJitter;
        simtime_t neighborValidityInterval;

        // context
        NotificationBoard * notificationBoard;
        IMobility * mobility;
        IAddressPolicy * addressPolicy;
        IInterfaceTable * interfaceTable;
        IRoutingTable * routingTable; // TODO: delete when necessary functions are moved to interface table
        INetfilter * networkProtocol;
        static PositionTable globalPositionTable; // KLUDGE: TODO: implement registry protocol

        // internal
        cMessage * beaconTimer;
        cMessage * purgeNeighborsTimer;
        PositionTable neighborPositionTable;

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

        // handling beacon timers
        void scheduleBeaconTimer();
        void processBeaconTimer();

        // handling purge neighbors timers
        void schedulePurgeNeighborsTimer();
        void processPurgeNeighborsTimer();

        // handling UDP packets
        void sendUDPPacket(UDPPacket * packet, double delay);
        void processUDPPacket(UDPPacket * packet);

        // handling beacons
        GPSRBeacon * createBeaconPacket();
        void sendBeaconPacket(GPSRBeacon * beacon, double delay);
        void processBeaconPacket(GPSRBeacon * beacon);

        // position
        Coord getDestinationPosition(const Address & address);
        Coord getNeighborPosition(const Address & address);

        // address
        std::string getHostName();
        Address getSelfAddress();

        // neighbor
        simtime_t getNextNeighborExpiration();
        void purgeNeighbors();

        // next hop
        Address findNextHop(const Address & destination);

        // netfilter
        virtual Result datagramPreRoutingHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry *& outputInterfaceEntry, Address & nextHop) { return routeDatagram(datagram, outputInterfaceEntry, nextHop); }
        virtual Result datagramForwardHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry *& outputInterfaceEntry, Address & nextHop) { return ACCEPT; }
        virtual Result datagramPostRoutingHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry *& outputInterfaceEntry, Address & nextHop) { return ACCEPT; }
        virtual Result datagramLocalInHook(INetworkDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) { return ACCEPT; }
        virtual Result datagramLocalOutHook(INetworkDatagram * datagram, const InterfaceEntry *& outputInterfaceEntry, Address & nextHop) { return routeDatagram(datagram, outputInterfaceEntry, nextHop); }
        bool isGPSRDatagram(INetworkDatagram * datagram);
        Result routeDatagram(INetworkDatagram * datagram, const InterfaceEntry *& outputInterfaceEntry, Address & nextHop);

        // notification
        virtual void receiveChangeNotification(int category, const cObject * details);
};

#endif
