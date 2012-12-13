//
// This program is property of its copyright holder. All rights reserved.
//

cplusplus {{
#include "Address.h"
#include "DYMOdefs.h"
}}

class noncobject Address;

class noncobject DYMOSequenceNumber;

class noncobject DYMOMetricType;

namespace DYMO;

//
// classes
//

class AddressBlock {
    Address address;
    int prefixLength;
    DYMOSequenceNumber sequenceNumber;
}

//
// timer messages
//

message RREQTimer {
    Address target;
}

message RREQWaitRREPTimer extends RREQTimer {
    int retryCount;
}

message RREQBackoffTimer extends RREQTimer {
    int retryCount;
}

message RREQHolddownTimer extends RREQTimer {
}

//
// DYMO packets
//

packet DYMOPacket {
}

packet RteMsg extends DYMOPacket {
    int hopCount;
    int hopLimit;
    double metric;
    DYMOMetricType metricType;
    DYMOSequenceNumber sequenceNumber;
    AddressBlock origNode;
    AddressBlock targetNode;
}

packet RREQ extends RteMsg {
}

packet RREP extends RteMsg {
}

packet RREP_ACK extends DYMOPacket {
}

packet RERR extends DYMOPacket {
}
