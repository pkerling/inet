//
// Copyright (C) 2012 Opensim Ltd.
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

#ifndef __GENERICNETWORKPROTOCOLCONTROLINFO_H
#define __GENERICNETWORKPROTOCOLCONTROLINFO_H

#include "INETDefs.h"

#include "Address.h"
#include "INetworkProtocolControlInfo.h"

class INET_API GenericNetworkProtocolControlInfo : public INetworkProtocolControlInfo
{
    private:
        short protocol;
        Address sourceAddress;
        Address destinationAddress;
        int interfaceId;
        short hopLimit;

    public:
        virtual ~GenericNetworkProtocolControlInfo() { }
        virtual short getProtocol() const { return protocol; }
        virtual void setProtocol(short protocol) { this->protocol = protocol;}
        virtual const Address getSourceAddress() const { return sourceAddress; }
        virtual void setSourceAddress(const Address & address) { this->sourceAddress = address; }
        virtual const Address getDestinationAddress() const { return destinationAddress; }
        virtual void setDestinationAddress(const Address & address) { destinationAddress = address; }
        virtual int getInterfaceId() const { return interfaceId; }
        virtual void setInterfaceId(int interfaceId) { this->interfaceId = interfaceId; }
        virtual short getHopLimit() const { return hopLimit; }
        virtual void setHopLimit(short hopLimit) { this->hopLimit = hopLimit; }
};

#endif

