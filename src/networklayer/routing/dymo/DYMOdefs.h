//
// This program is property of its copyright holder. All rights reserved.
//

#ifndef DYMODEFS_H_
#define DYMODEFS_H_

#define DYMO_NAMESPACE_BEGIN namespace DYMO {
#define DYMO_NAMESPACE_END }

DYMO_NAMESPACE_BEGIN

// TODO: use generic MANET port
#define DYMO_UDP_PORT 269

typedef uint32_t DYMOSequenceNumber;

// TODO: metric types are defined in a separate [RFC 6551]
enum DYMOMetricType {
    HOP_COUNT = 3, // Hop Count has Metric Type assignment 3
};

enum DYMORouteState {
    ACTIVE,
    IDLE,
    EXPIRED,
    BROKEN,
    TIMED
};

DYMO_NAMESPACE_END

#endif /* DYMODEFS_H_ */
