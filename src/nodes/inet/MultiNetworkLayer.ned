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

import inet.nodes.ipv6.IPv6NetworkLayer;


simple XXXMultiplexer
{
    gates:
        input tcpIn @labels(TCPSegment,INetworkControlInfo/down);
        input udpIn @labels(UDPPacket,INetworkControlInfo/down);
        input sctpIn @labels(INetworkControlInfo/down,SCTPPacket);
        input rsvpIn @labels(INetworkControlInfo/down);
        input ospfIn @labels(INetworkControlInfo/down);
        input pingIn[];
        input manetIn;
        input igmpIn;
        output tcpOut @labels(TCPSegment,INetworkControlInfo/up);
        output udpOut @labels(UDPPacket,INetworkControlInfo/up);
        output sctpOut @labels(INetworkControlInfo/up,SCTPPacket);
        output rsvpOut @labels(INetworkControlInfo/up);
        output ospfOut @labels(INetworkControlInfo/up);
        output pingOut[];
        output manetOut;
        output igmpOut;
}

simple YYYMultiplexer
{
    gates:
        input npIn[] @labels(INetworkDatagram);
        output npOut[] @labels(INetworkDatagram);
        input ifIn[] @labels(INetworkDatagram);
        output ifOut[] @labels(INetworkDatagram);
}

//
// TODO
//
module MultiNetworkLayer like INetworkLayer
{
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
        gn: GenericNetworkLayer {
            @display("p=100,200");
        }
        ipv4: IPv4NetworkLayer {
            @display("p=200,200");
        }
        ipv6: IPv6NetworkLayer {
            @display("p=300,200");
        }
        xxx: XXXMultiplexer {
            @display("p=200,100");
        }
        yyy: YYYMultiplexer {
            @display("p=200,300");
        }
    connections:
        manetIn --> xxx.manetIn;
        xxx.manetOut --> manetOut;

        // generic
        for i=0..sizeof(gn.pingIn)-1 {
            xxx.pingOut++ --> gn.pingIn[i];
        }
        for i=0..sizeof(gn.pingOut)-1 {
            gn.pingOut[i] --> xxx.pingIn++;
        }
        // ipv4
        for i=0..sizeof(ipv4.pingIn)-1 {
            xxx.pingOut++ --> ipv4.pingIn[i];
        }
        for i=0..sizeof(ipv4.pingOut)-1 {
            ipv4.pingOut[i] --> xxx.pingIn++;
        }
        // ipv6
        for i=0..sizeof(ipv6.pingIn)-1 {
            xxx.pingOut++ --> ipv6.pingIn[i];
        }
        for i=0..sizeof(ipv6.pingOut)-1 {
            ipv6.pingOut[i] --> xxx.pingIn++;
        }

		// xxx
        for i=0..sizeof(xxx.pingIn)-1 {
            pingIn[i] --> { @display("m=n"); } --> xxx.pingIn[i];
        }
        for i=0..sizeof(yyy.ifOut)-1 {
            xxx.pingOut[i] --> { @display("m=n"); } --> pingOut[i];
        }

        // generic
        for i=0..sizeof(ifIn)-1 {
            yyy.npOut++ --> gn.ifIn[i];
        }
        for i=0..sizeof(ifOut)-1 {
            gn.ifOut[i] --> yyy.npIn++;
        }
        // ipv4
        for i=0..sizeof(ipv4.ifIn)-1 {
            yyy.npOut++ --> ipv4.ifIn[i];
        }
        for i=0..sizeof(ipv4.ifOut)-1 {
            ipv4.ifOut[i] --> yyy.npIn++;
        }
        // ipv6
        for i=0..sizeof(ipv6.ifIn)-1 {
            yyy.npOut++ --> ipv6.ifIn[i];
        }
        for i=0..sizeof(ipv6.ifOut)-1 {
            ipv6.ifOut[i] --> yyy.npIn++;
        }

		// yyy
        for i=0..sizeof(yyy.ifIn)-1 {
            ifIn[i] --> { @display("m=s"); } --> yyy.ifIn[i];
        }
        for i=0..sizeof(yyy.ifOut)-1 {
            yyy.ifOut[i] --> { @display("m=s"); } --> ifOut[i];
        }
}
