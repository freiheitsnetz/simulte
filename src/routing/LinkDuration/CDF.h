// Author:S John-Torben Reimers
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

#ifndef __SIMULTE_CDF_H_
#define __SIMULTE_CDF_H_

#include <omnetpp.h>
#include <math.h>
#include <complex>


using namespace omnetpp;

/**
 * Precalulates CDF values (From Medina's PDF) and offers a map from CDF(t)->t (Inversion work around)
 * Detailed information of methods can be found in the corresponding .CC-File
 */
class CDF : public cSimpleModule
{
  protected:
    double tau;
    double speed;
    double Tx_Range;
    double step;
    double start;
    double limit;
    int precision;
    std::complex<double>imagNum{0.0,1.0};
    std::vector<double>initialCDF;
    std::vector<double>t_values;
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
    void calcCDF();
    std::complex<double> compLog(double);
  public:
    double getClosestT_value(double); //map from y to x
    std::complex<double> dilog(double LL, double tau, int precision);
    double returnCDFvalue(double input); //calculation from x to y
    double getTau();
};

#endif
