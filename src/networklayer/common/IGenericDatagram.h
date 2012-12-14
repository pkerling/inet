//
// This program is property of its copyright holder. All rights reserved.
// 

#ifndef IGENERICDATAGRAM_H_
#define IGENERICDATAGRAM_H_

#include "Address.h"

class IGenericDatagram {
public:
    virtual const Address getSourceAddress() const = 0;
    virtual void setSourceAddress(const Address & address) = 0;
    virtual const Address getDestinationAddress() const = 0;
    virtual void setDestinationAddress(const Address & address) = 0;
};

#endif /* IGENERICDATAGRAM_H_ */
