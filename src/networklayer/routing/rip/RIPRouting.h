//
// Copyright (C) 2013 Opensim Ltd.
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

#ifndef __INET_RIPROUTING_H_
#define __INET_RIPROUTING_H_

#include "INETDefs.h"
#include "IGenericRoute.h"
#include "IGenericRoutingTable.h"
#include "IInterfaceTable.h"
#include "UDPSocket.h"

#define RIP_INFINITE_METRIC 16
#define RIP_UPDATE_INTERVAL 30
#define RIP_UDP_PORT 520
#define RIP_IPV4_MULTICAST_ADDRESS "224.0.0.9"
#define RIP_ROUTE_EXPIRY_TIME 180
#define RIP_ROUTE_PURGE_TIME 120
#define RIP_TRIGGERED_UPDATE_DELAY_MIN 1
#define RIP_TRIGGERED_UPDATE_DELAY_MAX 5

/* RIPRoute:
 *   destination address
 *   metric
 *   next hop address (missing if destination is directly connected)
 *   bool routeChangeFlag;
 *   timers: expiryTime (180s after update), purgeTime (120s after expiry)
 *
 * Initial routes:
 *   directly connected networks
 *   static routes
 *
 * Outside a subnetted network only the network routes are advertised (merging subnet routes) (RFC 2453 3.7)
 *
 * Default routes (with 0.0.0.0 address) are added to BGP routers and are propagated by RIP.
 * Routes involving 0.0.0.0 should not leave the boundary of an AS. (RFC 2453 3.7)
 *
 * Split horizon: do not send route for a destination network to the neighbors from which the route was learned
 * Poisoned split horizon: send them, but with metric 16 (infinity).
 */

/**
 * Implementation of the Routing Information Protocol v2 (RFC 2453).
 */
class INET_API RIPRouting : public cSimpleModule
{
    struct RIPInterfaceEntry
    {
        InterfaceEntry *ie;
        int metric;
        RIPInterfaceEntry(InterfaceEntry *ie, int metric) : ie(ie), metric(metric) {}
    };
    typedef std::vector<RIPInterfaceEntry> InterfaceVector;

    IInterfaceTable *ift;
    IGenericRoutingTable *rt;
    InterfaceVector ripInterfaces;
    UDPSocket socket;               // bound to RIP_UDP_PORT
    cMessage *updateTimer;          // for sending unsolicited Response messages in every ~30 seconds.
    cMessage *triggeredUpdateTimer; // scheduled when there are pending changes
    Address allRipRoutersGroup;     // multicast address, e.g. 224.0.0.9 or FF02::9
    // parameters
    bool usePoisonedSplitHorizon;
  public:
    RIPRouting();
    ~RIPRouting();
  private:
    RIPInterfaceEntry *findInterfaceEntryById(int interfaceId);
    IGenericRoute *findRoute(const Address &destAddress, const Address &subnetMask);
    bool isNeighbour(const Address &address);
    bool isOwnAddress(const Address &address);
  protected:
    virtual int numInitStages() const  {return 5;}
    virtual void initialize(int stage);
    virtual void handleMessage(cMessage *msg);

    /**
     * Requests the whole routing table from all neighboring RIP routers.
     */
    virtual void sendInitialRequests();

    /**
     * Processes a RIP request, i.e. sends the requested routing entries to a peer.
     */
    virtual void processRequest(RIPPacket *packet);

    /**
     * Called by triggered updates.
     */
    virtual void processTriggeredUpdate();

    /**
     * Called by regular updates.
     */
    virtual void processRegularUpdate();

    /**
     * Sends routes of the routing table to the specified address.
     * If changedOnly is true, only the changed routes are sent.
     * Split Horizon check is performed by this method.
     * It can send multiple RIPPackets.
     */
    virtual void sendRoutes(const Address &address, int port, InterfaceEntry *ie, bool changedOnly);

    /**
     * Processes a RIP response, i.e. updates the routing table with the learned routes.
     */
    virtual void processResponse(RIPPacket *packet);

    /**
     * Validates a RIP response.
     */
    virtual bool isValidResponse(RIPPacket *packet);

    /**
     * Add the new entry to the routing table and triggers an update.
     */
    virtual void addRoute(const Address &dest, const Address &subnetMask, const Address &nextHop, int metric);

    /**
     * Updates an existing route with the information learned from a RIP packet.
     * If the metric is infinite (16), then the route is invalidated.
     * It triggers an update, so neighbor routers are notified about the change.
     */
    virtual void updateRoute(IGenericRoute *route, const Address &nextHop, int metric);

    /**
     * Sets the update timer to trigger an update in the [1s,5s] interval.
     * If the update is already scheduled, it does nothing.
     */
    virtual void triggerUpdate();

    /**
     * Invalidates the route, i.e. marks it invalid, but keeps it in the routing table for 120s,
     * so the neighbors are notified about the broken route in the next update.
     */
    virtual void invalidateRoute(IGenericRoute *route);

    /**
     * Removes the route from the routing table.
     */
    virtual void purgeRoute(IGenericRoute *route);

    /**
     * Should be called regularly to handle expiry and purge of routes.
     * Returns the route if it is valid.
     */
    virtual IGenericRoute *checkRoute(IGenericRoute *route);

    /**
     * Sends the packet to the specified UDP dest/port.
     * If the dest is a multicast address, then the outgoing interface must be specified.
     */
    virtual void sendPacket(RIPPacket *packet, const Address &dest, int port, InterfaceEntry *ie);
};

#endif
