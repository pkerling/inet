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

//
// TODO:
//
moduleinterface INetworkLayer
{
    parameters:
        @display("i=block/fork");
    gates:
        // TODO: it should not have so many different gates
        // TODO: we should rather have one upper and one lower input and output gates
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
}
