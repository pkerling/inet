//
// Copyright (C) 2004 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

package inet.networklayer.routing.gpsr;

//
// http://www.eecs.harvard.edu/~htk/publication/2000-mobi-karp-kung.pdf
//
simple GPSR
{
    parameters:
        // properties
        @display("i=block/routing");

        // context parameters
        string routingTableModuleName = default("routingTable");
        string networkProtocolModuleName = default("ip");

        // gpsr parameters
        int planarizationMode @enum(0, 1) = default(0); // 0 for GG, 1 for RNG
        string interfaces = default("*");
        double beaconInterval @unit("s") = default(10s);
		double maxJitter @unit("s") = default(1s);
        double neighborValidityInterval @unit("s") = default(30s);

    gates:
        input ipIn;
        output ipOut;
}
