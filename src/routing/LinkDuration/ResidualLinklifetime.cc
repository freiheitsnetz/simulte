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

#include "ResidualLinklifetime.h"
#include <ModuleAccess.h>
#include <LinkDuration/NeighborLinkTimeTable.h>
Define_Module(ResidualLinklifetime);

void ResidualLinklifetime::initialize()
{

    cXMLElementList tempInputbinList=par("bin").xmlValue()->getChildren();
    for( auto it=tempInputbinList.begin();it!=tempInputbinList;++it){

    }

        cXMLElementList tempInputvalueList=par("value").xmlValue()->getChildren();
        for(int it=tempInputvalueList.begin();it!=tempInputvalueList;++it){

        }


    LinkTimeTable = inet::getModuleFromPar<NeighborLinkTimeTable>(par("neighborLinkTimeTable"), this);

    if((par("selfMode").boolValue()== true && par("givenMode").boolValue()== true)||(par("selfMode").boolValue()== false && par("givenMode").boolValue()== false)){

        if(par("selfMode").boolValue()== true)
            mode=SELF;
        if(par("givenMode").boolValue()==true)
            mode=GIVEN;
    }
        else
            throw cRuntimeError("Mode must be 'selfMode' OR(!) 'givenMode'");
    }




void ResidualLinklifetime::handleMessage(cMessage *msg)
{
    // TODO - Generated method body
}

int ResidualLinklifetime::calcResidualLinklifetime(cModule* neighbor)
{
    switch(mode){
        case SELF: return calcRLLviaTable(neighbor); //Not implemented yet
        case GIVEN: return calcRLLviaInput(neighbor);
        default: throw cRuntimeError("Mode must be 'selfMode' OR(!) 'given'");
    }
}

int ResidualLinklifetime::calcRLLviaInput(cModule* neighbor){
//TODO InputLinkDist is normalized, Link duration is not.

    int tempLinkDuration=LinkTimeTable->getNeighborLinkTime(neighbor); //get link lifetime from NeighborLinkTimeTable
    auto it = InputLinkDist.find(tempLinkDuration);//find same value in distribution
    // Calculating mean value of shifted and normalized distribution function in discrete time domain. (Integration in nominator and denominator, just like in the formula)
    std::pair<int,int> fraction;
    for (std::map<int,float>::iterator i = it; i!= InputLinkDist.end();++i)
    {
       fraction.first+= i->first*i->second;
       fraction.second+= i->second;

    }
    return fraction.first/fraction.second-it->second;
}
//TODO ->
int ResidualLinklifetime::calcRLLviaTable(cModule* neighbor){

    NeighborLinkTimeTable* LinkTimeTable= check_and_cast<NeighborLinkTimeTable*>(getParentModule()->getSubmodule("NeighborLinkTimeTable"));
    LinkTimeTable->getNeighborLinkTime(neighbor);
    return 0; //avoiding error

}


