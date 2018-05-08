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
simple ResidualLinklifetime
{
    parameters:
        @display("bgb=920,362;i=block/segm_l");
        bool selfMode = default (false);
        bool givenMode =default (false);
        bool functionMode =default (true);
        string neighborLinkTimeTable = default("^.NeighborLinkTimeTable");
        string neighborDiscoveryModule = default("^.SimpleNeighborDiscovery");
        int nodeSpeed=default(100);
        
        //24 Hours. Residual Linklifetimes longer than 24 hours will be set to 24 hours (simulation will be shorter anyway)
        //Reason: simtime_t unit has a limit of 9223372s
        int maxERLL @unit("s") = default(86400s);
 //       xml inputDist = default(xmldoc("inputDist.xml"));
        
}
