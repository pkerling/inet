//
// Copyright (C) 2004 Andras Varga
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

package inet.nodes.inet;

import inet.networklayer.arp.GlobalARP;
import inet.networklayer.generic.GenericNetworkProtocol;
import inet.networklayer.generic.EchoProtocol;


//
// TODO
//
module GenericNetworkLayer like INetworkLayer
{
    parameters:
        @display("i=block/fork");
    gates:
        input ifIn[] @labels(INetworkDatagram);
        input tcpIn @labels(TCPSegment,INetworkControlInfo/down);
        input udpIn @labels(UDPPacket,INetworkControlInfo/down);
        input sctpIn @labels(INetworkControlInfo/down,SCTPPacket);
        input rsvpIn @labels(INetworkControlInfo/down);
        input ospfIn @labels(INetworkControlInfo/down);
        input pingIn[];
        input manetIn;
        input igmpIn;
        output ifOut[] @labels(INetworkDatagram);
        output tcpOut @labels(TCPSegment,INetworkControlInfo/up);
        output udpOut @labels(UDPPacket,INetworkControlInfo/up);
        output sctpOut @labels(INetworkControlInfo/up,SCTPPacket);
        output rsvpOut @labels(INetworkControlInfo/up);
        output ospfOut @labels(INetworkControlInfo/up);
        output pingOut[];
        output manetOut;
        output igmpOut;

    submodules:
        np: GenericNetworkProtocol {
            parameters:
                protocolMapping = "6:0,17:1,1:2,2:3,46:4,89:5,132:6,48:7,138:7"; // see: http://www.iana.org/assignments/protocol-numbers
                @display("p=85,95;q=queue");
            gates:
                transportIn[8];
                transportOut[8];
                queueIn[sizeof(ifIn)];
        }
        echo: EchoProtocol {
            parameters:
                @display("p=160,63");
        }
        arp: GlobalARP {
            parameters:
                @display("p=202,142;q=pendingQueue");
            gates:
                nicOut[sizeof(ifOut)];
        }
    connections allowunconnected:
        // transport Layer
        np.transportOut[0] --> { @display("m=n"); } --> tcpOut;
        np.transportIn[0] <-- { @display("m=n"); } <-- tcpIn;

        np.transportOut[1] --> { @display("m=n"); } --> udpOut;
        np.transportIn[1] <-- { @display("m=n"); } <-- udpIn;

        np.transportOut[2] --> echo.localIn;
        np.transportIn[2] <-- echo.sendOut;

        np.transportOut[4] --> { @display("m=n"); } --> rsvpOut;
        np.transportIn[4] <-- { @display("m=n"); } <-- rsvpIn;

        np.transportOut[5] --> { @display("m=n"); } --> ospfOut;
        np.transportIn[5] <-- { @display("m=n"); } <-- ospfIn;

        np.transportOut[6] --> { @display("m=n"); } --> sctpOut;
        np.transportIn[6] <-- { @display("m=n"); } <-- sctpIn;

        np.transportOut[7] --> { @display("m=n"); } --> manetOut;
        np.transportIn[7] <-- { @display("m=n"); } <-- manetIn;

        for i=0..sizeof(pingOut)-1 {
            echo.pingOut++ --> { @display("m=n"); } --> pingOut[i];
        }
        for i=0..sizeof(pingIn)-1 {
            echo.pingIn++ <-- { @display("m=n"); } <-- pingIn[i];
        }

        np.queueOut --> arp.ipIn;

        for i=0..sizeof(ifIn)-1 {
            ifIn[i] --> { @display("m=s"); } --> np.queueIn[i];
        }
        for i=0..sizeof(ifOut)-1 {
            arp.nicOut[i] --> { @display("m=s"); } --> ifOut[i];
        }
}
