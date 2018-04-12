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

Define_Module(ResidualLinklifetime);

void ResidualLinklifetime::initialize(int stage)
{
    if(stage == INITSTAGE_NETWORK_LAYER_3){

    neighborModule = inet::getModuleFromPar<inet::SimpleNeighborDiscovery>(par("neighborDiscoveryModule"), this);
    LinkTimeTable = inet::getModuleFromPar<NeighborLinkTimeTable>(par("neighborLinkTimeTable"), this);

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
            constantSpeed= par("nodeSpeed");
            transmissionRange= neighborModule->getTransmssionRange();
            tau=transmissionRange/constantSpeed;


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

int ResidualLinklifetime::calcRLLviaInput(cModule* neighbor){


    int tempLinkDuration=LinkTimeTable->getNeighborLinkTime(neighbor); //get link lifetime from NeighborLinkTimeTable
    auto it = InputLinkDist.find(tempLinkDuration);//find same value in distribution
    // Calculating mean value of shifted and normalized distribution function in discrete time domain. (Integration in nominator and denominator, just like in the formula)
    std::pair<float,float> fraction;
    for (std::map<int,double>::iterator i = it; i!= InputLinkDist.end();++i)
    {
       fraction.first+= i->first*i->second;
       fraction.second+= i->second;

    }
    return fraction.first/fraction.second-it->second;
}
//TODO ->
int ResidualLinklifetime::calcRLLviaTable(cModule* neighbor){

    LinkTimeTable->getNeighborLinkTime(neighbor);
    return 0; //avoiding error

}
/*From "Topology Characterization of High Density Airspace
Aeronautical Ad Hoc Networks" by Daniel Medina, Felix Hoffmann, Serkan Ayaz, Munich,Germany*/
int ResidualLinklifetime::calcRLLviaFunction(cModule* neighbor){
    int t=LinkTimeTable->getNeighborLinkTime(neighbor); //get link lifetime from NeighborLinkTimeTable
    int RLL=0;
    std::pair<float,float>fraction;




    return RLL;

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

void ResidualLinklifetime::estimateInitialLL(inet::L3Address IPaddress){
/* An alternative to really invert the function to draw a number from*/
    /* It sets the inital Link lifetime at the beginning of the simulation from the PDF of Medina*/
    double randValue=uniform(0,1);
    for(u_int i=0;i<t_value.size();i++)
    {
        if(randValue<=initialCDF[i])
            LinkTimeTable->setNeighborLinkTime(neighborModule->getAddressFromIP(IPaddress),initialCDF[i]);
        break;
    }



}

void ResidualLinklifetime::calcCDF(){


    for(float i=0.01;i<=5;i++){
        t_value.push_back(i);
    }
    /*The function is calculating the CDF out of a PDF
    From "Topology Characterization of High Density Airspace
    Aeronautical Ad Hoc Networks" by Daniel Medina, Felix Hoffmann, Serkan Ayaz, Munich,Germany*/

    /*First Normalize PDF by calculating the total amount of values. This should be one. If not devide every PDF value by this counter*/
    long initialPDFTotalCount=0;

    for(u_int it=0;it<=t_value.size();++it){

            int tempCount=abs(2/pow(M_PI,2)*t_value[it]*atanh((2*(t_value[it]/tau))/(1+pow((t_value[it]/tau),2))));
            if(!std::isinf(tempCount))
                initialPDFTotalCount=tempCount+initialPDFTotalCount;
    }
    for(u_int it=0;it<=t_value.size();++it){
        int tempCount=abs(2/pow(M_PI,2)*t_value[it]*atanh((2*(t_value[it]/tau))/(1+pow((t_value[it]/tau),2))));
        if(!std::isinf(tempCount))
        initialPDF[it]=tempCount/initialPDFTotalCount;

    }



    initialCDF[0]=initialPDF[0];

    for(u_int it=0;it<=t_value.size();++it){
        initialCDF[it]=initialCDF[it]+initialCDF[it-1];

    }

}


