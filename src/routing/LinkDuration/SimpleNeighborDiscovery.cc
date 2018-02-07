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


}

void SimpleNeighborDiscovery::handleMessage(cMessage *msg)
{
    if(msg==update)
        updateNodeDistanceEntries();
        updateConnectionVector();
    scheduleAt(simTime()+updateTimer,update);

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
        if (temp!=ownAddress)//TODO Check if path is correct later on
            otherAddressVector[i]=temp;

    }
}
void SimpleNeighborDiscovery::updateConnectionVector(){

    neighborConnection.clear();
    for(std::map<cModule*,int>::iterator it= nodeDistance.begin(); it!=nodeDistance.end();it++){
        neighborConnection[it->first]=isInConnectionRange(transmissionRange, it->second);

    }
}
bool SimpleNeighborDiscovery::isInConnectionRange(int txRange, int nodeDistance){
    return txRange>=nodeDistance;
}

}
