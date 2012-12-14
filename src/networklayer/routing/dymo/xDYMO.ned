//
// This program is property of its copyright holder. All rights reserved.
// 

package inet.networklayer.routing.dymo;

//
// Dynamic MANET On-demand (DYMO also known as AODVv2) Routing 
// based on draft-ietf-manet-dymo-24.
//
// Module parameters are copied from the IETF draft and renamed
// for consistency with INET naming conventions. Some parameters
// are not yet implemented.
//
simple xDYMO
{
    parameters:
        // properties
        @class("DYMO::xDYMO");
	    @display("i=block/routing");

        // context parameters
        string routingTableModuleName = default("routingTable");
        string networkProtocolModuleName = default("ip");
        
        // 1. DYMO parameter group
        // string CLIENT_ADDRESSES = default("");
        // bool USE_MULTICAST_RREP = default(false);
        // string DEFAULT_METRIC_TYPE = default("HOP_COUNT");
        // string AODVv2_INTERFACES = default("*");

        // 2. DYMO parameter group
        double activeInterval @unit("s") = default(5s); // ACTIVE_INTERVAL
        double maxIdleTime @unit("s") = default(200s); // MAX_IDLETIME 
        double maxSequenceNumberLifetime @unit("s") = default(300s); // MAX_SEQNUM_LIFETIME 
        double routeRREQWaitTime @unit("s") = default(2s); // ROUTE_RREQ_WAIT_TIME  
        // double UNICAST_MESSAGE_SENT_TIMEOUT @unit("s") = default(1s); 
        double rreqHolddownTime @unit("s") = default(10s); // RREQ_HOLDDOWN_TIME 

		// 3. DYMO parameter group
		int maxHopCount = default(20); // MAX_HOPCOUNT
		// string MAX_METRIC = default("");
		// double MAXTIME;
		int discoveryAttemptsMax = default(3); // DISCOVERY_ATTEMPTS_MAX
		// int MTU;
		
		// 4. DYMO parameter group
		// bool APPEND_INFORMATION;
        int bufferSizePackets = default(-1); // BUFFER_SIZE_PACKETS
        int bufferSizeBytes @unit("B") = default(-1B); //  BUFFER_SIZE_BYTES
        // double CONTROL_TRAFFIC_LIMIT

		// DYMO extension parameters
		bool sendIntermediateRREP = default(true);
        int minHopLimit = default(5);
        int maxHopLimit = default(10);
    
    gates:
        input udpIn @labels(UDPControlInfo/up);
        output udpOut @labels(UDPControlInfo/down);
}
