//
// This program is property of its copyright holder. All rights reserved.
// 

package inet.examples.manetrouting.gpsr;

import inet.networklayer.autorouting.ipv4.IPv4NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.networklayer.routing.gpsr.GPSR;
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
        gpsr: GPSR {
            @display("p=527,287");
        }
    connections:
        gpsr.ipOut --> networkLayer.manetIn;
        gpsr.ipIn <-- networkLayer.manetOut;
}

//
// TODO
//
network GPSRNetworkRandom
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

network GPSRNetworkManual
{
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
        host0: AdhocHost {
            parameters:
                @display("i=device/pocketpc_s;r=,,#707070;p=37,300");
        }
        host1: AdhocHost {
            parameters:
                @display("i=device/pocketpc_s;r=,,#707070;p=723,300");
        }
        host2: AdhocHost {
            parameters:
                @display("i=device/pocketpc_s;r=,,#707070;p=185,300");
        }
        host3: AdhocHost {
            parameters:
                @display("i=device/pocketpc_s;r=,,#707070;p=345,300");
        }
        host4: AdhocHost {
            parameters:
                @display("i=device/pocketpc_s;r=,,#707070;p=345,157");
        }
        host5: AdhocHost {
            parameters:
                @display("i=device/pocketpc_s;r=,,#707070;p=346,26");
        }
        host6: AdhocHost {
            parameters:
                @display("i=device/pocketpc_s;r=,,#707070;p=446,26");
        }
        host7: AdhocHost {
            parameters:
                @display("i=device/pocketpc_s;r=,,#707070;p=346,446");
        }
        host8: AdhocHost {
            parameters:
                @display("i=device/pocketpc_s;r=,,#707070;p=345,572");
        }
        host9: AdhocHost {
            parameters:
                @display("i=device/pocketpc_s;r=,,#707070;p=446,572");
        }
        host10: AdhocHost {
            parameters:
                @display("i=device/pocketpc_s;r=,,#707070;p=642,100");
        }
}
