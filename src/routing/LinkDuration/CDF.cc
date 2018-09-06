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

#include "CDF.h"

Define_Module(CDF);

void CDF::initialize()
{
    speed= par("speed"); //From Km/h to Km/s
    speed=speed/3600;
    Tx_Range=par("Tx_Range"); //In Km
    step=par("step");
    start=par("start");
    limit=par("limit");
    precision=par("precision");
    tau=Tx_Range/speed; //In s
    EV_INFO << "Before CDF";
    calcCDF();
    EV_INFO << "CDF Init Done ";
    WATCH_VECTOR(initialCDF);
    WATCH_VECTOR(t_values);
    WATCH(tau);
}

void CDF::handleMessage(cMessage *msg)
{
    // Nothing to do here
}

/*Calculates the CDF of the PDF with variable bin size which can be defined in omnetpp.ini
 *"initialCDF" is used for drawing a number out of PDF. Inversion of CDF is needed but complex to calculate.
 *Therefore values of CDF(t) are calculated in calcCDF() to offer a mapping from CDF(t)->t.
 *Use own compLog (Complex Logarithm) in case of negative values. tau and t are always positiv
 */
void CDF::calcCDF(){

for (double t=start*tau;t<=limit*tau;t=t+step*tau){
    t_values.push_back(t);
    std::complex<double> CDFtemp;
    std::complex<double> templog1;
    std::complex<double> templog2;
    if((1-t/tau)<0)
        templog1=compLog(1-t/tau);
    else
        templog1=log(1-t/tau);

    if((t/tau-1)<0)
        templog2=compLog(t/tau-1);
    else
        templog2=log (t/tau-1);

    CDFtemp= 2/pow(M_PI,2)*(-dilog(-t,tau,precision)+dilog(t,tau,precision)+log(t)*(templog1-templog2));
    initialCDF.push_back(CDFtemp.real());
    }

}
/*Returns  t_value(Link Lifetime) to given uniform(0,1). Draw a number out of the distribution
 * In other words: Since inversion of original CDF is not possible, this function maps
 * CDF(t)->t, where CDF(t) are precalutclated values calculated in "CalcCFD())*/
double CDF::getClosestT_value(double input){

    for (u_int i=0;i<initialCDF.size();i++){
        if(!std::isnan(initialCDF[i])){
            if(initialCDF[i]>=input){
                if(i==0)
                    return t_values[i];
                else if(fabs(initialCDF[i]-input) < fabs(initialCDF[i-1]-input))
                    return t_values[i];
                else
                    return t_values[i-1];
            }

            }
        else
            return (t_values[i+1]+t_values[i-1])/2;// Linear regression
        }
    return t_values[initialCDF.size()-1]+tau; //In the case of no match add tau once to the highest value (no higher values have been precalucated)
    }



/*Returns the Dilog Value (Polylog(2,...) or Li_2) with given Link Lifetime LL, ratio tau and the limit of the sum iteration (normally to infinity)*/
/*Precision around 100 is enough*/
std::complex<double> CDF::dilog(double LL, double tau, int precision){

    std::complex<double> dilogValue=0;
    std::complex<double> outTemp=0;

    if(fabs(LL/tau)>1.0){
        if(-LL/tau>0)
            outTemp=(-pow(M_PI,2))/6-(0.5)*(pow(log(-LL/tau),2));
        else
            outTemp=(-pow(M_PI,2))/6-(0.5)*(pow(compLog(-LL/tau),2));

        for(int i=1; i<=precision;i++){
            dilogValue=dilogValue+(pow((LL/tau),-i))/pow(i,2);
        }
        return(outTemp-dilogValue);
    }
    else if(fabs(LL/tau)<1.0){
        for(int i=1; i<=precision;i++){
            dilogValue=dilogValue+(pow((LL/tau),i))/pow(i,2);
        }
        return dilogValue;
    }
        else
            return NAN;
}
/*Complex logarithm to make log(-x) possible*/
/*input is negative*/
std::complex<double> CDF::compLog(double input) {

    std::complex<double> tempValue=log(-input)+imagNum*M_PI;
    return  tempValue;
}


double CDF::returnCDFvalue(double input){

    input=input;
    std::complex<double> CDFtemp;
    std::complex<double> templog1;
    std::complex<double> templog2;
    if((1-input/tau)<0)
        templog1=compLog(1-input/tau);
    else
        templog1=log(1-input/tau);

    if((input/tau-1)<0)
        templog2=compLog(input/tau-1);
    else
        templog2=log (input/tau-1);

    CDFtemp= 2/pow(M_PI,2)*(-dilog(-input,tau,precision)+dilog(input,tau,precision)+log(input)*(templog1-templog2));
    return CDFtemp.real();

}

double CDF::getTau(){
    return tau;

}
