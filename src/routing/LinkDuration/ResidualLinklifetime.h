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

#ifndef __SIMULTE_RESIDUALLINKLIFETIME_H_
#define __SIMULTE_RESIDUALLINKLIFETIME_H_

#include <omnetpp.h>
#include <LinkDuration/NeighborLinkTimeTable.h>

using namespace omnetpp;

/**
 * This module calculates (predicts) the residual link lifetime when needed.
 * calcResidualLinklifetime(address of neighbor module) can be called from external
 * to get the residual link lifetime
 * It needs a  NeighborLinkTimeTable modul to access the link lifetimes.
 */
class ResidualLinklifetime : public cSimpleModule
{
  protected:

    enum Mode {SELF, GIVEN} mode;
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
    cXMLElement* tempInputbin;
    cXMLElement* tempInputvalue;
    std::map<int,double> InputLinkDist;//safe the read in LinkDistribution
    NeighborLinkTimeTable* LinkTimeTable=nullptr;
    int calcRLLviaInput(cModule* neighbor);
    int calcRLLviaTable(cModule* neighbor);
    void setDistbyInput();
  public:
    simtime_t calcResidualLinklifetime(cModule* neighbor);


};

#endif
