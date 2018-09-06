// Author: John-Torben Reimers
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

#ifndef __SIMULTE_NEIGHBORLINKTIMETABLE_H_
#define __SIMULTE_NEIGHBORLINKTIMETABLE_H_

#include <omnetpp.h>
#include "inet/common/INETDefs.h"


using namespace omnetpp;

/*
 *A table to store addresses of network nodes and respective current link lifetimes.
 * Functions are only called by residual link lifetime modul
 * or neighbordiscovery modules.
 * Since this modul does not know if a neighbor is still a neighbor,
 * the link lifetimes are incremented by neighbordiscovery modul.
 * The functions are selfexplaining
 */
class INET_API NeighborLinkTimeTable : public cSimpleModule
{
  protected:
    void initialize();
    void handleMessage(cMessage *msg);
    std::map <cModule*,int > NeighborLinkTimeMap; //First: ID Second:Current Linklifetime
    cLongHistogram measuredLinkDurations; //collects linklifetimes when a link expires (UNUSED)
    std::vector<int> measuredLinkDurationsVec; //same value as added to hist.


  public:
    int getNeighborLinkTime(cModule*);
    void setNeighborLinkTime(cModule*,int);
    void deleteNeighborLinkTimeEntry(cModule*);
    /*Called when a link expires due to distance*/
    void updateLinkDurationHist(int linkDuration);


};

#endif
