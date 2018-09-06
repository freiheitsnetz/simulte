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

#ifndef __SIMULTE_RESIDUALLINKLIFETIME_H_
#define __SIMULTE_RESIDUALLINKLIFETIME_H_

#include <omnetpp.h>

#include "SimpleNeighborDiscovery.h"
#include "NeighborLinkTimeTable.h"
#include "SimpleNeighborDiscovery.h"
#include "L3Address.h"
#include "CDF.h"

using namespace inet;

/**
 * This module calculates (predicts) the residual link lifetime when needed.
 * calcResidualLinklifetime(address of neighbor module) can be called from external
 * to get the residual link lifetime
 * It needs a  NeighborLinkTimeTable modul to access the link lifetimes.
 * It needs a neighbordiscovery module for IP to ram address conversion.
 * Detailed information of methods can be found in the corresponding .CC-File
 */
class ResidualLinklifetime : public cSimpleModule
{
  protected:

    enum Mode {SELF, GIVEN, FUNCTION} mode;
    int tau;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg);
    cXMLElement* tempInputbin;
    cXMLElement* tempInputvalue;
    int oor_counter_simttime=0; //For debugging
    int oor_counter_RLL=0;//For debugging
    int oor_counter_Dist=0;//For debugging
    int maxERLL; //maximum value, the predicted residual link lifetime can be set to. By default 24 hours

    std::map<int,double> InputLinkDist;//safe the read in LinkDistribution (Unused)
    NeighborLinkTimeTable* LinkTimeTable=nullptr;
    SimpleNeighborDiscovery* neighborModule=nullptr;
    CDF* cdfModule=nullptr;
    int calcRLLviaInput(cModule* neighbor);
    int calcRLLviaTable(cModule* neighbor);
    int calcRLLviaFunction(cModule* neighbor);
    void setDistbyInput();
    simtime_t calcResidualLinklifetime(cModule* neighbor);
    simtime_t getResidualLinklifetime (inet::L3Address IPaddress);
    void setInitialLLVector();


  public:


    simtime_t getMetrik(inet::L3Address IPaddress);//Just for modularity. It simply calls getResidualLinklifetime(IPaddress);


};

#endif
