//
// This library is free software, you can redistribute it
// and/or modify
// it under  the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation;
// either version 2 of the License, or any later version.
// The library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU Lesser General Public License for more details.
//
// Copyright 2004 Andras Varga
//

#include <vector>
#include <string>

#include "INETDefs.h"

#include "InterfaceEntry.h"
#include "IPv4.h"
#include "IPv4Datagram.h"


class INET_API IPv4HookInfo : public cSimpleModule, public IPv4::Hook
{
  protected:
    IPv4 *ipLayer;

  protected:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
    virtual void finish();

  public:
    /**
    * called before a packet arriving from the network is routed
    */
    virtual IPv4::Hook::Result datagramPreRoutingHook(const IPv4Datagram* datagram, const InterfaceEntry* inIE, const IPv4* ipLayer);

    /**
    * called before a packet arriving from the network is delivered locally
    */
    virtual IPv4::Hook::Result datagramLocalInHook(const IPv4Datagram* datagram, const InterfaceEntry* inIE, const IPv4* ipLayer);

    /**
    * called before a packet arriving from the network is delivered via the network
    */
    virtual IPv4::Hook::Result datagramForwardHook(const IPv4Datagram* datagram, const InterfaceEntry* inIE, const InterfaceEntry* outIE, const IPv4Address& nextHopAddr, const IPv4* ipLayer);

    /**
    * called before a packet is delivered via the network
    */
    virtual IPv4::Hook::Result datagramPostRoutingHook(const IPv4Datagram* datagram, const InterfaceEntry* inIE, const InterfaceEntry* outIE, const IPv4Address& nextHopAddr, const IPv4* ipLayer);

    /**
    * called before a packet arriving locally is delivered
    */
    virtual IPv4::Hook::Result datagramLocalOutHook(const IPv4Datagram* datagram, const InterfaceEntry* outIE, const IPv4* ipLayer);
};


Define_Module(IPv4HookInfo);


void IPv4HookInfo::initialize()
{
    ipLayer = check_and_cast<IPv4*>(findModuleWhereverInNode("ip", this));
    ipLayer->registerHook(0, this);
}

void IPv4HookInfo::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module can not receive messages");
}


IPv4::Hook::Result IPv4HookInfo::datagramPreRoutingHook(const IPv4Datagram* datagram, const InterfaceEntry* inIE, const IPv4* ipLayer)
{
    EV << "HOOK " << getFullPath() << ": PREROUTING packet=" << datagram->getName()
       << " inIE=" << (inIE ? inIE->getName() : "NULL")
       << endl;
    return IPv4::Hook::ACCEPT;
}

IPv4::Hook::Result IPv4HookInfo::datagramLocalInHook(const IPv4Datagram* datagram, const InterfaceEntry* inIE, const IPv4* ipLayer)
{
    EV << "HOOK " << getFullPath() << ": LOCAL IN: packet=" << datagram->getName()
       << " inIE=" << (inIE ? inIE->getName() : "NULL")
       << endl;
    return IPv4::Hook::ACCEPT;
}

IPv4::Hook::Result IPv4HookInfo::datagramLocalOutHook(const IPv4Datagram* datagram, const InterfaceEntry* outIE, const IPv4* ipLayer)
{
    EV << "HOOK " << getFullPath() << ": LOCAL OUT: packet=" << datagram->getName()
       << " outIE=" << (outIE ? outIE->getName() : "NULL")
       << endl;
    return IPv4::Hook::ACCEPT;
}

IPv4::Hook::Result IPv4HookInfo::datagramForwardHook(const IPv4Datagram* datagram, const InterfaceEntry* inIE, const InterfaceEntry* outIE, const IPv4Address& nextHopAddr, const IPv4* ipLayer)
{
    EV << "HOOK " << getFullPath() << ": FORWARD: packet=" << datagram->getName()
       << " inIE=" << (inIE ? inIE->getName() : "NULL")
       << " outIE=" << (outIE ? outIE->getName() : "NULL")
       << " nextHop=" << nextHopAddr
       << endl;
    return IPv4::Hook::ACCEPT;
}

IPv4::Hook::Result IPv4HookInfo::datagramPostRoutingHook(const IPv4Datagram* datagram, const InterfaceEntry* inIE, const InterfaceEntry* outIE, const IPv4Address& nextHopAddr, const IPv4* ipLayer)
{
    EV << "HOOK " << getFullPath() << ": POSTROUTING packet=" << datagram->getName()
       << " inIE=" << (inIE ? inIE->getName() : "NULL")
       << " outIE=" << (outIE ? outIE->getName() : "NULL")
       << " nextHop=" << nextHopAddr
       << endl;
    return IPv4::Hook::ACCEPT;
}


void IPv4HookInfo::finish()
{
    ipLayer->unregisterHook(0, this);
}

