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
#include <limits>

//Delete after debuggin


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
        /*xml input file for mapping: NOT IMPLEMENTED*/
        else if(par("givenMode").boolValue()==true){
            mode=GIVEN;
            setDistbyInput();
        }
        /*Use mapping function*/
        else if(par("functionMode").boolValue()==true){
            mode=FUNCTION;
            setInitialLLVector();
            tau=cdfModule->getTau();
            maxERLL=par("maxERLL");
            WATCH(oor_counter_simttime);
            WATCH(oor_counter_RLL);
            WATCH(oor_counter_Dist);


    }
        else
            throw cRuntimeError("Mode must be 'functionMode'");
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
        default: throw cRuntimeError("Mode must be 'functionMode'");
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
Aeronautical Ad Hoc Networks" by Daniel Medina, Felix Hoffmann, Serkan Ayaz, Munich,Germany
Calls the current link lifetime to use it as input for the CDF(t)
Both are needed to predict the residual link lifetime. Formula can be found in the mentioned paper*/

int ResidualLinklifetime::calcRLLviaFunction(cModule* neighbor){

    //TODO simplifications okay?
    int t=LinkTimeTable->getNeighborLinkTime(neighbor); //get link lifetime from NeighborLinkTimeTable
    if(t==1)
        t=1+std::numeric_limits< double >::min();; //Slight shift due to log undefined at 0
    double DistofLL=cdfModule->returnCDFvalue(t);
    int tmpLL=(tau-DistofLL)/(1-DistofLL)-t;
    if(DistofLL<0)
        oor_counter_Dist++;

    if(t<0){//t cannot be smaller than 0. if it happens: wrap around of int
        oor_counter_RLL++;
    }
    if(tmpLL>maxERLL||tmpLL<0){ //Avoid simtime_t to reach limit (By default 24 hours is returned)
        oor_counter_simttime++;
        return maxERLL;

    }
    return tmpLL;

}
/*Supposed to read an XML file as input distribution: Uncompleted and untested*/
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

/*Returns the current link lifetime via given IP address. Only function which can be called from outside*/
simtime_t ResidualLinklifetime::getMetrik (inet::L3Address IPaddress){

    return getResidualLinklifetime(IPaddress);

}
/*Once called at the beginning, such that the beginning current link lifetime is drawn out of the PDF
 *  to precent that all current link lifetimes start at 0*/
void ResidualLinklifetime::setInitialLLVector(){


    volatile double randValue=0;
    std::map<cModule*,bool>* tmpConnection =neighborModule->getConnectionVector();
    int rng=0;
    for(auto it=tmpConnection->begin();it!=tmpConnection->end();it++){
        if(it->second==1){
            randValue=uniform(0,1,rng);
           double temptime=cdfModule->getClosestT_value(randValue);
            //std::string temptimestr=temptime.parse();
            LinkTimeTable->setNeighborLinkTime(it->first,temptime);
        }

        randValue=0;
        }

}



