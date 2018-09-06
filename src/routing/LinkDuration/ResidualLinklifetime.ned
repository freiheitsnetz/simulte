//
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

package lte.routing.LinkDuration;

//
// TODO auto-generated module
//
simple ResidualLinklifetime
{
    parameters:
        @display("bgb=920,362;i=block/segm_l");
        bool selfMode = default (false); //possible learning mode (to be implemented)
        bool givenMode =default (false); //possible xml input PDF mode (to be implemented)
        bool functionMode =default (true); // applying a hardcoded PDF
        string neighborLinkTimeTable = default("^.NeighborLinkTimeTable");
        string neighborDiscoveryModule = default("^.SimpleNeighborDiscovery");
        int nodeSpeed=default(100);
        
      
        int maxERLL @unit("s") = default(86400s);  //24 Hours.
        //Residual Linklifetimes longer than 24 hours will be set to 24 hours (simulation will be shorter anyway)
        //Reason: simtime_t unit has a limit of 9223372s
        
}
