//
// Copyright (C) 2004, 2009 Andras Varga
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

#ifndef __INET_SCMP_H
#define __INET_SCMP_H

#include "INETDefs.h"
#include "Address.h"
#include "SCMPPacket_m.h"

class PingPayload;

/**
 * Simple Control Message Protocol
 * TODO
 */
class INET_API SCMP : public cSimpleModule
{
  protected:
    typedef std::map<long,int> PingMap;
    PingMap pingMap;

  protected:
    virtual void processPacket(SCMPPacket * packet);
    virtual void processEchoRequest(SCMPPacket * packet);
    virtual void processEchoReply(SCMPPacket * packet);
    virtual void sendEchoRequest(PingPayload * packet);

  protected:
    virtual void handleMessage(cMessage *msg);
};

#endif
