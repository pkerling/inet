//
// Copyright (C) 2012 Andras Varga
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


#include "NotifierConsts.h"
#include "NotificationBoard.h"
#include "IRoutingTable.h"
#include "IPv4Route.h"
#include "IInterfaceTable.h"
#include "IPv4InterfaceData.h"
#include "RoutingTableRecorder.h"


Define_Module(RoutingTableRecorder);

#define LL INT64_PRINTF_FORMAT  // for eventnumber_t


// We need this because we want to know which NotificationBoard the notification comes from
// (INotifiable::receiveChangeNotification() doesn't have NotificationBoard* as arg).
class RoutingTableRecorderListener : public INotifiable
{
private:
    NotificationBoard *nb;
    RoutingTableRecorder *recorder;
public:
    RoutingTableRecorderListener(RoutingTableRecorder *recorder, NotificationBoard *nb) {this->recorder = recorder; this->nb = nb;}
    virtual void receiveChangeNotification(int category, const cObject *details) {recorder->receiveChangeNotification(nb, category, details);}
};

RoutingTableRecorder::RoutingTableRecorder()
{
}

RoutingTableRecorder::~RoutingTableRecorder()
{
}

void RoutingTableRecorder::initialize(int stage)
{
    if (par("enabled").boolValue())
        hookListeners();
}

void RoutingTableRecorder::handleMessage(cMessage *)
{
    throw cRuntimeError(this, "This module doesn't process messages");
}

void RoutingTableRecorder::hookListeners()
{
    // hook existing notification boards (we won't cover dynamically created hosts/routers, but oh well)
    for (int id = 0; id < simulation.getLastModuleId(); id++)
    {
        NotificationBoard *nb = dynamic_cast<NotificationBoard *>(simulation.getModule(id));
        if (nb)
        {
            INotifiable *listener = new RoutingTableRecorderListener(this, nb);
            nb->subscribe(listener, NF_INTERFACE_CREATED);
            nb->subscribe(listener, NF_INTERFACE_DELETED);
            nb->subscribe(listener, NF_INTERFACE_CONFIG_CHANGED);
            nb->subscribe(listener, NF_INTERFACE_IPv4CONFIG_CHANGED);
            //nb->subscribe(listener, NF_INTERFACE_IPv6CONFIG_CHANGED);
            //nb->subscribe(listener, NF_INTERFACE_STATE_CHANGED);

            nb->subscribe(listener, NF_IPv4_ROUTE_ADDED);
            nb->subscribe(listener, NF_IPv4_ROUTE_DELETED);
            nb->subscribe(listener, NF_IPv4_ROUTE_CHANGED);
        }
    }
}

void RoutingTableRecorder::receiveChangeNotification(NotificationBoard *nb, int category, const cObject *details)
{
    cModule *host = nb->getParentModule();
    if (category==NF_IPv4_ROUTE_ADDED || category==NF_IPv4_ROUTE_DELETED || category==NF_IPv4_ROUTE_CHANGED)
        recordRouteChange(host, check_and_cast<const IPv4Route *>(details), category);
    else
        recordInterfaceChange(host, check_and_cast<const InterfaceEntry *>(details), category);
}

void RoutingTableRecorder::recordInterfaceChange(cModule *host, const InterfaceEntry *ie, int category)
{
    // Note: ie->getInterfaceTable() may be NULL (entry already removed from its table)

    const char *tag;
    switch (category) {
    case NF_INTERFACE_CREATED: tag = "+I"; break;
    case NF_INTERFACE_DELETED: tag = "-I"; break;
    case NF_INTERFACE_CONFIG_CHANGED: tag = "*I"; break;
    case NF_INTERFACE_IPv4CONFIG_CHANGED: tag = "*I"; break;
    default: throw cRuntimeError("Unexpected notification category %d", category);
    }

    // time, moduleId, ifname, address
    std::stringstream content;
    content << tag << " " << host->getId() << " " << ie->getName() << " ";
    content << (ie->ipv4Data()!=NULL ? ie->ipv4Data()->getIPAddress().str() : IPv4Address().str());
    simulation.getEnvir()->customEventlogEntry("RT", content.str().c_str());
}

void RoutingTableRecorder::recordRouteChange(cModule *host, const IPv4Route *route, int category)
{
    IRoutingTable *rt = route->getRoutingTable(); // may be NULL! (route already removed from its routing table)

    const char *tag;
    switch (category) {
    case NF_IPv4_ROUTE_ADDED: tag = "+R"; break;
    case NF_IPv4_ROUTE_CHANGED: tag = "*R"; break;
    case NF_IPv4_ROUTE_DELETED: tag = "-R"; break;
    default: throw cRuntimeError("Unexpected notification category %d", category);
    }

    // time, moduleId, routerID, dest, dest netmask, nexthop
    std::stringstream content;
    content << tag << " " << host->getId() << " " << (rt ? rt->getRouterId().str() : "*") << " ";
    content << route->getDestination().str() << " " << route->getNetmask().str() << " ";
    content << route->getGateway().str();
    simulation.getEnvir()->customEventlogEntry("RT", content.str().c_str());
}

//TODO: routerID change
//    // moduleId, routerID
//    content << getParentModule()->getId() << " "; //XXX we assume routing table is direct child of the node compound module
//    content << a.str();
