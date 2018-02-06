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

#include <routing/NeighborLinkTimeTable/NeighborLinkTimeTable.h>

Define_Module(NeighborLinkTimeTable);

void NeighborLinkTimeTable::initialize()
{
    //NeighborLinkTimeMap[getId()]=0;
}

void NeighborLinkTimeTable::handleMessage(cMessage *msg)
{
    //No message can occur
}

int NeighborLinkTimeTable::getNeighborLinkTime(int neighborID)
{
    auto it=NeighborLinkTimeMap.find(neighborID);
        if( it != NeighborLinkTimeMap.end() )
            return it->second;
        else return 0;

}
void NeighborLinkTimeTable::setNeighborLinkTime(int neighborID,int linkLifetime)
{
    NeighborLinkTimeMap[neighborID]=linkLifetime; //This will create if non-existent or set the new value
}

void NeighborLinkTimeTable::deleteNeighborLinkTimeEntry(int neighborID)
{
    auto it=NeighborLinkTimeMap.find(neighborID);
        if( it != NeighborLinkTimeMap.end() )
             NeighborLinkTimeMap.erase(it);
}
