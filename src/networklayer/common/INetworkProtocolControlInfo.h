//
// This program is property of its copyright holder. All rights reserved.
// 

#ifndef INETWORKPROTOCOLCONTROLINFO_H_
#define INETWORKPROTOCOLCONTROLINFO_H_

#include "Address.h"

class INetworkProtocolControlInfo {
  public:
    virtual ~INetworkProtocolControlInfo() { }
    virtual short getProtocol() const = 0;
    virtual void setProtocol(short protocol) = 0;
    virtual const Address getSourceAddress() const = 0;
    virtual void setSourceAddress(const Address & address) = 0;
    virtual const Address getDestinationAddress() const = 0;
    virtual void setDestinationAddress(const Address & address) = 0;
    virtual int getInterfaceId() const = 0;
    virtual void setInterfaceId(int interfaceId) = 0;
    virtual short getHopLimit() const = 0;
    virtual void setHopLimit(short hopLimit) = 0;
};

#endif /* INETWORKPROTOCOLCONTROLINFO_H_ */
