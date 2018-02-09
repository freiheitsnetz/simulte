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

#include <ModuleAccess.h>
#include <IMobility.h>
#include <LinkDuration/SimpleNeighborDiscovery.h>
#include <NeighborLinkTimeTable.h>

namespace inet{

Define_Module(SimpleNeighborDiscovery);

void SimpleNeighborDiscovery::initialize()
{
    updateTimer = par("updateTimer");
    ownAddress= getContainingNode(this);
    setAllUEsAddresses();
    updateNodeDistanceEntries();
    updateConnectionVector();
    scheduleAt(simTime()+updateTimer,update);
    update= new cMessage("Update");
    sec= new cMessage("Second");
    LinkTimeTable = inet::getModuleFromPar<NeighborLinkTimeTable>(par("neighborLinkTimeTable"), this);


}

void SimpleNeighborDiscovery::handleMessage(cMessage *msg)
{
    if(msg==update){
        updateNodeDistanceEntries();
        updateConnectionVector();
    scheduleAt(simTime()+updateTimer,update);
    }
    if(msg==sec){
        incrementLinklifetime();
        scheduleAt(simTime()+1,sec);

    }

}

void SimpleNeighborDiscovery::updateNodeDistanceEntries(){

    nodeDistance.clear();
    cModule *self = ownAddress;
    IMobility *selfMobility = check_and_cast<IMobility *>(self->getSubmodule("mobility"));
    Coord tempCoordOwn = selfMobility->getCurrentPosition();

    for (std::vector<cModule*>::iterator it = otherAddressVector.begin(); it!=otherAddressVector.end(); ++it){

        cModule *host = *it;
        IMobility *otherMobility = check_and_cast<IMobility *>(host->getSubmodule("mobility"));
        Coord tempCoordOther = otherMobility->getCurrentPosition();

        //Pythagoras of distances(x,y,z) and then round to int
        nodeDistance[host]=round(sqrt(pow(abs(tempCoordOwn.x -tempCoordOther.x),2)+
                               pow(abs(tempCoordOwn.y -tempCoordOther.y),2)+
                               pow(abs(tempCoordOwn.z -tempCoordOther.z),2)));
}

}
void SimpleNeighborDiscovery::setAllUEsAddresses()
{
    otherAddressVector.clear();
    for (int i=0; i<numHosts; i++ )
    {
        char buf[20];
        sprintf(buf, "UE%d", i);
        cModule* temp = getContainingNode(getModuleByPath(buf));
        if (temp!=ownAddress)
            otherAddressVector[i]=temp;

    }
}
void SimpleNeighborDiscovery::updateConnectionVector(){
    std::map<cModule*,bool> previousConnection=neighborConnection;
    neighborConnection.clear();
    for(std::map<cModule*,int>::iterator it= nodeDistance.begin(); it!=nodeDistance.end();it++){
        bool tempConnection=isInConnectionRange(transmissionRange, it->second);
        neighborConnection[it->first]=tempConnection;
        /*
         * Lost connection must be documented in histogram
         * previous connection must  have been 1
         */
        if(tempConnection==0){
        std::map<cModule*,bool>::iterator iter= previousConnection.find(it->first);
            if(iter->second==1);
            LinkTimeTable->updateLinkDurationHist(LinkTimeTable->getNeighborLinkTime(iter->first));

        }
    }
}
bool SimpleNeighborDiscovery::isInConnectionRange(int txRange, int nodeDistance){
    return txRange>=nodeDistance;
}
void SimpleNeighborDiscovery::incrementLinklifetime(){

    for(std::map<cModule*,bool>::iterator it= neighborConnection.begin();it!=neighborConnection.end();++it){
        if(it->second==1){
            //Just incrementing
            LinkTimeTable->setNeighborLinkTime(it->first,LinkTimeTable->getNeighborLinkTime(it->first)+1);
        }
        else if (it->second==0){
            LinkTimeTable->deleteNeighborLinkTimeEntry(it->first);

        }

    }
}


}
