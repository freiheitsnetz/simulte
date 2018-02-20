//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#ifndef __SIMULTE_SIMPLENEIGHBORDISCOVERY_H_
#define __SIMULTE_SIMPLENEIGHBORDISCOVERY_H_

#include <omnetpp.h>
#include <map>
#include "inet/common/INETDefs.h"
#include <LinkDuration/NeighborLinkTimeTable.h>
#include "inet/networklayer/contract/IRoutingTable.h"
/*
 * This module was originally made for link lifetime calculations.
 * It gets the positions (from simulation) of all other network hosts/UEs and compares it with own transmission range
 * to decide whether it is a a direct neighbor it can transmit to or not.
 * It needs a "NeighborLinkTimeTable" module to save the link lifetimes and updates it every second
 *
 */
namespace inet{


class INET_API SimpleNeighborDiscovery:  public cSimpleModule
{
  protected:
    int transmissionRange;
    std::vector<cModule*> otherAddressVector; //Of network module
    cModule* ownAddress; //Of network module
//    std::map<int,cModule*>IDsAddressMap;// Of network module
    std::map<cModule*,bool> neighborConnection;
    std::map<cModule*,int> nodeDistance;
    std::map<cModule*,L3Address>addressToIP;
    int numHosts;
    NeighborLinkTimeTable *LinkTimeTable;
    cMessage *update;
    cMessage *sec;
    simtime_t updateTimer;
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
  public:
    bool getConnectivityEntry(cModule* Address);
    bool isInConnectionRange (int txRange, int nodedistance);
    void updateNodeDistanceEntries();
    void updateConnectionVector();
    void setAllUEsAddresses(); //From External
    void incrementLinklifetime();
    void setAddresstoIPMap();
    cModule* getAddressFromIP(L3Address);

};

#endif
}
