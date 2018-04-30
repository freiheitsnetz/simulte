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
#include <string.h>
#include <math.h>

//Delete after debuggin
#include <iostream>
#include <fstream>

Define_Module(ResidualLinklifetime);

void ResidualLinklifetime::initialize(int stage)
{
    if(stage == INITSTAGE_NETWORK_LAYER_3){



    neighborModule = inet::getModuleFromPar<inet::SimpleNeighborDiscovery>(par("neighborDiscoveryModule"), this);
    LinkTimeTable = inet::getModuleFromPar<NeighborLinkTimeTable>(par("neighborLinkTimeTable"), this);
    cdfModule = check_and_cast<CDF*>(getSimulation()->getModuleByPath("CDF"));

        /*Calculate distribution themselves: NOT IMPLEMENTED*/
        if(par("selfMode").boolValue()== true)
            mode=SELF;
        /*xml input file for mapping*/
        else if(par("givenMode").boolValue()==true){
            mode=GIVEN;
            setDistbyInput();
        }
        /*Use mapping function*/
        else if(par("functionMode").boolValue()==true){
            mode=FUNCTION;
            setInitialLLVector();
            tau=cdfModule->getTau();
            WATCH(oor_counter_simttime);
            WATCH(oor_counter_RLL);
            WATCH(oor_counter_Dist);


    }
        else
            throw cRuntimeError("Mode must be 'selfMode' OR(!) 'givenMode'");
    }
}


void ResidualLinklifetime::handleMessage(cMessage *msg)
{
    // Nothing to do here
}

simtime_t ResidualLinklifetime::calcResidualLinklifetime(cModule* neighbor)
{
    switch(mode){
        case SELF: return calcRLLviaTable(neighbor); //Not implemented yet
        case GIVEN: return calcRLLviaInput(neighbor);//Not implemented yet
        case FUNCTION: return calcRLLviaFunction(neighbor);
        default: throw cRuntimeError("Mode must be 'selfMode' OR(!) 'given'");
    }
}
//TODO->
int ResidualLinklifetime::calcRLLviaInput(cModule* neighbor){


    return 0;
}
//TODO ->
int ResidualLinklifetime::calcRLLviaTable(cModule* neighbor){


    return 0; //avoiding error

}
/*From "Topology Characterization of High Density Airspace
Aeronautical Ad Hoc Networks" by Daniel Medina, Felix Hoffmann, Serkan Ayaz, Munich,Germany*/
int ResidualLinklifetime::calcRLLviaFunction(cModule* neighbor){

    //TODO simplifications okay?
    int t=LinkTimeTable->getNeighborLinkTime(neighbor); //get link lifetime from NeighborLinkTimeTable
    if(t==1)
        t=1.0000000001; //Slight shift due to log undefined at 0
    double DistofLL=cdfModule->returnCDFvalue(t);
    int tmpLL=(tau-DistofLL)/(1-DistofLL)-t;
    if(DistofLL<0)
        oor_counter_Dist++;

    if(t<0){//t cannot be smaller than 0. if it happens: wrap around of int
        oor_counter_RLL++;
    }
    if(tmpLL>9223372||tmpLL<0){ //MAX value simtime_t
        oor_counter_simttime++;
        return 9223372-1000;

    }
    return tmpLL;

}
/*Untested*/
void ResidualLinklifetime::setDistbyInput(){

    cXMLElementList parameters =par("inputDist").xmlValue()->getElementsByTagName("param");

    for (cXMLElementList::const_iterator it = parameters.begin(); it != parameters.end(); it++)
        {

        const char* bin = (*it)->getAttribute("bin");
        const char* value = (*it)->getAttribute("value");
        InputLinkDist[atoi(bin)]=strtod(value,0);

        }
}

simtime_t ResidualLinklifetime::getResidualLinklifetime (inet::L3Address IPaddress){

    return calcResidualLinklifetime(neighborModule->getAddressFromIP(IPaddress));

}

simtime_t ResidualLinklifetime::getMetrik (inet::L3Address IPaddress){

    return getResidualLinklifetime(IPaddress);

}

void ResidualLinklifetime::setInitialLLVector(){

    std::ofstream myfile;
    myfile.open ("intialLL.txt");
    volatile double randValue=0;
    std::map<cModule*,bool> tmpConnection =neighborModule->getConnectionVector();
    int rng=0;
    for(std::map<cModule*,bool>::iterator it=tmpConnection.begin();it!=tmpConnection.end();++it){
        if(it->second==1){
            randValue=uniform(0,1,rng);
            LinkTimeTable->setNeighborLinkTime(it->first,cdfModule->getClosestT_value(randValue));
        }
        myfile <<cdfModule->getClosestT_value(randValue) <<" " << it->first << " "<< randValue<<" " << it->second << " " << LinkTimeTable->getNeighborLinkTime(it->first)<< "\n";
        randValue=0;
        }
    myfile.close();

}



