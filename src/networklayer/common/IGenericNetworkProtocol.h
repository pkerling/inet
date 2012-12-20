//
// This program is property of its copyright holder. All rights reserved.
// 

#ifndef IGENERICNETWORKPROTOCOL_H_
#define IGENERICNETWORKPROTOCOL_H_

#include <omnetpp.h>
#include "InterfaceEntry.h"
#include "IGenericDatagram.h"

class IGenericNetworkProtocol {
  public:
    class IHook {
      public:
        enum Result {
            ACCEPT, /**< allow datagram to pass to next hook */
            DROP, /**< do not allow datagram to pass to next hook, delete it */
            QUEUE, /**< queue datagram for later re-injection */
            STOLEN /**< do not allow datagram to pass to next hook, but do not delete it */
        };

        virtual ~IHook() {};
        virtual Result datagramPreRoutingHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) = 0;
        virtual Result datagramLocalInHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry) = 0;
        virtual Result datagramForwardHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry * outputInterfaceEntry, const Address & nextHopAddress) = 0;
        virtual Result datagramPostRoutingHook(IGenericDatagram * datagram, const InterfaceEntry * inputInterfaceEntry, const InterfaceEntry * outputInterfaceEntry, const Address & nextHopAddress) = 0;
        virtual Result datagramLocalOutHook(IGenericDatagram * datagram, const InterfaceEntry * outputInterfaceEntry) = 0;
    };

    virtual ~IGenericNetworkProtocol() { }
    virtual void registerHook(int priority, IHook * hook) = 0;
    virtual void unregisterHook(int priority, IHook * hook) = 0;
    virtual void reinjectDatagram(const IGenericDatagram * datagram, IHook::Result verdict) = 0;
};

#endif /* IGENERICNETWORKPROTOCOL_H_ */
