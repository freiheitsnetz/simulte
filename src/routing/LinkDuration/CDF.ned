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

package lte.routing.LinkDuration;

//
// TODO auto-generated module
//
//There is a need for a central CDF for estimating the initial Link Lifetime. Otherwise every UE would have to do it on its own
simple CDF
{
    parameters:
    
    //For tau
    double speed= default(1000); //speed in Km/h
    double Tx_Range=default(80); // transmission range in Km
    //For CDF
    double start= default(0.01); //start of evaluating for loop (NOT 0)
    double step = default(0.01); //step size of for loop, also bin size (NOT 0)
    double limit = default(50); //limit of for loop (99% of values are covered with (50), use 250 for 99.9%)
    int precision =default(100); //limit of sum of dilog function (100 is fine)
    
    
}
