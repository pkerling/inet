//
// Copyright (C) 2004 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#ifndef __INET_IP_H
#define __INET_IP_H

#include <omnetpp.h>
#include "INETDefs.h"
#include "INetfilter.h"
#include "ICMPAccess.h"
#include "IPv4FragBuf.h"
#include "ProtocolMap.h"
#include "QueueBase.h"
#include "IPv4Datagram.h"


class ARPPacket;
class ICMPMessage;
class IInterfaceTable;
class IIPv4RoutingTable;

// ICMP type 2, code 4: fragmentation needed, but don't-fragment bit set
const int ICMP_FRAGMENTATION_ERROR_CODE = 4;


/**
 * Implements the IPv4 protocol.
 */
class INET_API IPv4 : public QueueBase, public INetfilter
{
  public:
    /**
     * Implements a Netfilter-like datagram hook
     */
    class Hook {
      public:
        virtual ~Hook() {};

        /**
        * called before a packet arriving from the network is routed
        */
        virtual IHook::Result datagramPreRoutingHook(IPv4Datagram* datagram, const InterfaceEntry* inIE) = 0;

        /**
        * called before a packet arriving from the network is delivered locally
        */
        virtual IHook::Result datagramLocalInHook(IPv4Datagram* datagram, const InterfaceEntry* inIE) = 0;

        /**
        * called before a packet arriving from the network is delivered via the network
        */
        virtual IHook::Result datagramForwardHook(IPv4Datagram* datagram, const InterfaceEntry* inIE, const InterfaceEntry*& outIE, IPv4Address& nextHopAddr) = 0;

        /**
        * called before a packet is delivered via the network
        */
        virtual IHook::Result datagramPostRoutingHook(IPv4Datagram* datagram, const InterfaceEntry* inIE, const InterfaceEntry*& outIE, IPv4Address& nextHopAddr) = 0;

        /**
        * called before a packet arriving locally is delivered
        */
        virtual IHook::Result datagramLocalOutHook(IPv4Datagram* datagram, const InterfaceEntry*& outIE) = 0;
    };

    class GenericHookAdapter : public Hook {
        private:
            INetfilter::IHook *hook;
        public:
            GenericHookAdapter(INetfilter::IHook *hook) { this->hook = hook; }
            virtual IHook::Result datagramPreRoutingHook(IPv4Datagram * datagram, const InterfaceEntry * inputInterfaceEntry) { return hook->datagramPreRoutingHook(datagram, inputInterfaceEntry);}
            virtual IHook::Result datagramLocalInHook(IPv4Datagram * datagram, const InterfaceEntry * inputInterfaceEntry) { return hook->datagramLocalInHook(datagram, inputInterfaceEntry); }
            virtual IHook::Result datagramForwardHook(IPv4Datagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry *& outputInterfaceEntry, IPv4Address & nextHopAddress) {
                Address address(nextHopAddress);
                IHook::Result result = hook->datagramForwardHook(datagram, inputInterfaceEntry, outputInterfaceEntry, address);
                nextHopAddress = address.toIPv4();
                return result;
            }
            virtual IHook::Result datagramPostRoutingHook(IPv4Datagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry *& outputInterfaceEntry, IPv4Address & nextHopAddress) {
                Address address(nextHopAddress);
                IHook::Result result = hook->datagramPostRoutingHook(datagram, inputInterfaceEntry, outputInterfaceEntry, address);
                nextHopAddress = address.toIPv4();
                return result;
            }
            virtual IHook::Result datagramLocalOutHook(IPv4Datagram * datagram, const InterfaceEntry *& outputInterfaceEntry) { return hook->datagramLocalOutHook(datagram, outputInterfaceEntry); }
    };

    /**
     * Represents an IPv4Datagram, queued by a Hook
     */
    class QueuedDatagramForHook {
      public:
        enum HookType {
          PREROUTING,
          LOCALIN,
          FORWARD,
          POSTROUTING,
          LOCALOUT
        };

        QueuedDatagramForHook(IPv4Datagram* datagram, const InterfaceEntry* inIE, const InterfaceEntry* outIE, const IPv4Address& nextHopAddr, HookType hookType) :
                datagram(datagram), inIE(inIE), outIE(outIE), nextHopAddr(nextHopAddr), hookType(hookType) {}
        virtual ~QueuedDatagramForHook() {}

        IPv4Datagram* datagram;
        const InterfaceEntry* inIE;
        const InterfaceEntry* outIE;
        IPv4Address nextHopAddr;
        const HookType hookType;
    };

  protected:
    IIPv4RoutingTable *rt;
    IInterfaceTable *ift;
    ICMPAccess icmpAccess;
    cGate *queueOutGate; // the most frequently used output gate

    // config
    int defaultTimeToLive;
    int defaultMCTimeToLive;
    simtime_t fragmentTimeoutTime;
    bool forceBroadcast;

    // working vars
    long curFragmentId; // counter, used to assign unique fragmentIds to datagrams
    IPv4FragBuf fragbuf;  // fragmentation reassembly buffer
    simtime_t lastCheckTime; // when fragbuf was last checked for state fragments
    ProtocolMapping mapping; // where to send packets after decapsulation

    // statistics
    int numMulticast;
    int numLocalDeliver;
    int numDropped;  // forwarding off, no outgoing interface, too large but "don't fragment" is set, TTL exceeded, etc
    int numUnroutable;
    int numForwarded;

    // hooks
    typedef std::multimap<int, Hook*> HookList;
    HookList hooks;
    typedef std::list<QueuedDatagramForHook> DatagramQueueForHooks;
    DatagramQueueForHooks queuedDatagramsForHooks;

  protected:
    // utility: look up interface from getArrivalGate()
    virtual const InterfaceEntry *getSourceInterfaceFrom(cPacket *msg);

    // utility: look up route to the source of the datagram and return its interface
    virtual const InterfaceEntry *getShortestPathInterfaceToSource(IPv4Datagram *datagram);

    // utility: show current statistics above the icon
    virtual void updateDisplayString();

    /**
     * Encapsulate packet coming from higher layers into IPv4Datagram, using
     * the given control info. Override if you subclassed controlInfo and/or
     * want to add options etc to the datagram.
     */
    virtual IPv4Datagram *encapsulate(cPacket *transportPacket, IPv4ControlInfo *controlInfo);

    /**
     * Creates a blank IPv4 datagram. Override when subclassing IPv4Datagram is needed
     */
    virtual IPv4Datagram *createIPv4Datagram(const char *name);

    /**
     * Handle IPv4Datagram messages arriving from lower layer.
     * Decrements TTL, then invokes routePacket().
     */
    virtual void handlePacketFromNetwork(IPv4Datagram *datagram, const InterfaceEntry *fromIE);

    // called after PREROUTING Hook (used for reinject, too)
    virtual void preroutingFinish(IPv4Datagram *datagram, const InterfaceEntry *fromIE);

    /**
     * Handle messages (typically packets to be send in IPv4) from transport or ICMP.
     * Invokes encapsulate(), then routePacket().
     */
    virtual void handleMessageFromHL(cPacket *msg);

    /**
     * Routes and sends datagram received from higher layers.
     * Invokes datagramLocalOutHook(), then routePacket().
     */
    virtual void datagramLocalOut(IPv4Datagram* datagram, const InterfaceEntry* destIE);

    /**
     * Handle incoming ARP packets by sending them over "queueOut" to ARP.
     */
    virtual void handleARP(ARPPacket *msg);

    /**
     * Handle incoming ICMP messages.
     */
    virtual void handleReceivedICMP(ICMPMessage *msg);

    /**
     * Performs unicast routing. Based on the routing decision, it sends the
     * datagram through the outgoing interface.
     */
    virtual void routeUnicastPacket(IPv4Datagram *datagram, const InterfaceEntry *fromIE, const InterfaceEntry *destIE);

    /**
     * Broadcasts the datagram on the specified interface.
     * When destIE is NULL, the datagram is broadcasted on each interface.
     */
    virtual void routeLocalBroadcastPacket(IPv4Datagram *datagram, const InterfaceEntry *destIE);

    /**
     * Determines the output interface for the given multicast datagram.
     */
    virtual const InterfaceEntry *determineOutgoingInterfaceForMulticastDatagram(IPv4Datagram *datagram, const InterfaceEntry *multicastIFOption);

    /**
     * Forwards packets to all multicast destinations, using fragmentAndSend().
     */
    virtual void forwardMulticastPacket(IPv4Datagram *datagram, const InterfaceEntry *fromIE);

    /**
     * Perform reassembly of fragmented datagrams, then send them up to the
     * higher layers using sendToHL().
     */
    virtual void reassembleAndDeliver(IPv4Datagram *datagram);

    // called after LOCAL_IN Hook (used for reinject, too)
    virtual void reassembleAndDeliverFinish(IPv4Datagram *datagram);

    /**
     * Decapsulate and return encapsulated packet after attaching IPv4ControlInfo.
     */
    virtual cPacket *decapsulate(IPv4Datagram *datagram);

    /**
     * Call PostRouting Hook and continue with fragmentAndSend() if accepted
     */
    virtual void fragmentPostRouting(IPv4Datagram *datagram, const InterfaceEntry *ie, IPv4Address nextHopAddr);

    /**
     * Fragment packet if needed, then send it to the selected interface using
     * sendDatagramToOutput().
     */
    virtual void fragmentAndSend(IPv4Datagram *datagram, const InterfaceEntry *ie, IPv4Address nextHopAddr);

    /**
     * Last TTL check, then send datagram on the given interface.
     */
    virtual void sendDatagramToOutput(IPv4Datagram *datagram, const InterfaceEntry *ie, IPv4Address nextHopAddr);

  public:
    IPv4() { rt = NULL; ift = NULL; queueOutGate = NULL; }

  protected:
    /**
     * Initialization
     */
    virtual void initialize();

    /**
     * Processing of IPv4 datagrams. Called when a datagram reaches the front
     * of the queue.
     */
    virtual void endService(cPacket *msg);

    // NetFilter functions:
  protected:
    /**
     * called before a packet arriving from the network is routed
     */
    IHook::Result datagramPreRoutingHook(IPv4Datagram* datagram, const InterfaceEntry* inIE);

    /**
     * called before a packet arriving from the network is delivered locally
     */
    IHook::Result datagramLocalInHook(IPv4Datagram* datagram, const InterfaceEntry* inIE);

    /**
     * called before a packet arriving from the network is delivered via the network
     */
    IHook::Result datagramForwardHook(IPv4Datagram* datagram, const InterfaceEntry* inIE, const InterfaceEntry*& outIE, IPv4Address& nextHopAddr);

    /**
     * called before a packet is delivered via the network
     */
    IHook::Result datagramPostRoutingHook(IPv4Datagram* datagram, const InterfaceEntry* inIE, const InterfaceEntry*& outIE, IPv4Address& nextHopAddr);

    /**
     * called before a packet arriving locally is delivered
     */
    IHook::Result datagramLocalOutHook(IPv4Datagram* datagram, const InterfaceEntry*& outIE);

  public:
    /**
     * registers a Hook to be executed during datagram processing
     */
    void registerHook(int priority, IPv4::Hook* hook);

    /**
     * unregisters a Hook to be executed during datagram processing
     */
    void unregisterHook(int priority, IPv4::Hook* hook);

    /**
     * get queued infos for a previously queued datagram
     *
     * you must use it, if you want to change output interface, next hop address, etc.
     * before use reinjectDatagram()
     */
    QueuedDatagramForHook& getQueuedDatagramForHook(const IPv4Datagram* datagram);

    /**
     * TODO
     */
    void insertDatagramToHookQueue(IPv4Datagram* datagram, const InterfaceEntry* inIE, const InterfaceEntry* outIE, const IPv4Address& nextHopAddr, QueuedDatagramForHook::HookType hook);

    /**
     * drop a previously queued datagram
     */
    void dropQueuedDatagram(const IPv4Datagram* datagram);

    /**
     * re-injects a previously queued datagram
     */
    void reinjectDatagram(const IPv4Datagram* datagram);

    virtual void registerHook(int priority, INetfilter::IHook * hook) { registerHook(priority, new GenericHookAdapter(hook)); }
    virtual void unregisterHook(int priority, INetfilter::IHook * hook) { } // TODO: iterate
    void dropQueuedDatagram(const INetworkDatagram * datagram) { dropQueuedDatagram(dynamic_cast<const IPv4Datagram *>(datagram)); }
    void reinjectQueuedDatagram(const INetworkDatagram * datagram) { reinjectDatagram(dynamic_cast<const IPv4Datagram *>(datagram)); }

    IIPv4RoutingTable* getRoutingTable() const { return rt; };
    IInterfaceTable* getInterfaceTable() const { return ift; };
};

#endif

