//
// This program is property of its copyright holder. All rights reserved.
// 

package inet.examples.manetrouting.dymo;

import inet.networklayer.autorouting.ipv4.IPv4NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.networklayer.routing.dymo.xDYMO;
import inet.nodes.inet.WirelessHost;
import inet.world.radio.ChannelControl;


//
// TODO
//
module AdhocHost extends WirelessHost
{
    parameters:
        @display("i=device/cellphone");
        IPForward = true;
        wlan[*].mgmtType = default("Ieee80211MgmtAdhoc");
    submodules:
        dymo: xDYMO {
            @display("p=527,287");
        }
    connections:
        dymo.ipOut --> networkLayer.manetIn;
        dymo.ipIn <-- networkLayer.manetOut;
}

//
// TODO
//
network DYMONetwork
{
    parameters:
        int numHosts;
    submodules:
        channelControl: ChannelControl {
            parameters:
                @display("p=50,50");
        }
        configurator: IPv4NetworkConfigurator {
            parameters:
                config = xml("<config><interface hosts='*' address='145.236.x.x' netmask='255.255.0.0'/></config>");
                @display("p=50,100");
        }
        routingTableRecorder: RoutingTableRecorder {
            parameters:
                @display("p=50,150");
        }
        host[numHosts]: AdhocHost {
            parameters:
                @display("i=device/pocketpc_s;r=,,#707070");
        }
    connections allowunconnected:
}
