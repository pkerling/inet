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
    bool hasValidityTime;
    simtime_t validityTime;
    bool hasMetric;
    double metric;
    bool hasMetricType;
    DYMOMetricType metricType;
    bool hasSequenceNumber;
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
    int hopLimit;
}

packet RteMsg extends DYMOPacket {
    int hopCount;
    AddressBlock originatorNode;
    AddressBlock targetNode;
    AddressBlock addedNode[];
}

packet RREQ extends RteMsg {
}

packet RREP extends RteMsg {
}

packet RREP_ACK extends DYMOPacket {
}

packet RERR extends DYMOPacket {
    AddressBlock unreachableNode[];
}
