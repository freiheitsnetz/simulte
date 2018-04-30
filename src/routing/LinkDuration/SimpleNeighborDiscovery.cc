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

void SimpleNeighborDiscovery::initialize(int stage)
{

    if(stage == INITSTAGE_NETWORK_LAYER_3){
    LinkTimeTable = getModuleFromPar<NeighborLinkTimeTable>(par("neighborLinkTimeTable"), this,1);

    transmissionRange=par("txRange");
    updateInterval = par("updateInterval");
    ownAddress= getContainingNode(this);
    numHosts= par("numHosts");
    setAllUEsAddresses();
    updateNodeDistanceEntries();
    updateConnectionVector();
    WATCH_MAP(neighborConnection);
    WATCH_MAP(nodeDistance);


    }
    if(stage == INITSTAGE_NETWORK_LAYER_3+1){
        setAddresstoIPMap();
        update= new cMessage("Update");
        sec= new cMessage("Second");
        scheduleAt(simTime()+updateInterval,update);
        scheduleAt(simTime()+1,sec);
    }



}

void SimpleNeighborDiscovery::handleMessage(cMessage *msg)
{
    if(msg==update){
        updateNodeDistanceEntries();
        updateConnectionVector();
    scheduleAt(simTime()+updateInterval,update);
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
        sprintf(buf, "ueD2D[%d]", i);
        cModule* temp = getContainingNode(getModuleByPath(buf));
        if (temp!=ownAddress)
            otherAddressVector.push_back(temp);

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
            if(iter->second==1)
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
void SimpleNeighborDiscovery::setAddresstoIPMap(){


    for(std::vector<cModule*>::iterator it= otherAddressVector.begin();it!=otherAddressVector.end();++it){
        IPv4RoutingTable* tempTable = check_and_cast<IPv4RoutingTable*>((*it)->getSubmodule("routingTable"));
        IPv4Address tempAddress = tempTable->getRouterId();
        addressToIP[(*it)]=tempAddress;
        }
    IPv4RoutingTable* tempTable = check_and_cast<IPv4RoutingTable*>(ownAddress->getSubmodule("routingTable"));
    ownIP=tempTable->getRouterId();

    }
cModule* SimpleNeighborDiscovery::getAddressFromIP(L3Address IPaddress){

    for(std::map<cModule*,L3Address>::iterator it= addressToIP.begin();it!=addressToIP.end();++it){
        if(it->second==IPaddress)
            return it->first;



    }
/*    IPv4Address test1=IPaddress.toIPv4();
            IPv4Address test2=ownIP.toIPv4();
    bool test=(IPaddress.toIPv4()==ownIP.toIPv4());
     if(IPaddress==ownIP)
        return ownAddress;
        */

    throw cRuntimeError("Couldn't find module address for given IP address");
    return nullptr;
   }


    SimpleNeighborDiscovery::~SimpleNeighborDiscovery(){
        delete sec;
        delete update;


    }

std::map<cModule*,bool> SimpleNeighborDiscovery::getConnectionVector(){
return neighborConnection;
}

}