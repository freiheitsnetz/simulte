//
// Copyright (C) 2014 OpenSim Ltd.
// Author: Benjamin Seregi
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

#ifndef __INET_AODVLDROUTEDATA_H
#define __INET_AODVLDROUTEDATA_H

#include <set>
#include "inet/networklayer/common/L3Address.h"
#include "inet/common/INETDefs.h"

namespace lte {

class INET_API AODVLDRouteData : public cObject
{
  protected:
    std::set<L3Address> precursorList;
    bool active;
    bool repariable;
    bool beingRepaired;
    bool validDestNum;
    unsigned int destSeqNum;
    unsigned int minimumResidualLinklifetime;
    simtime_t lifeTime;    // expiration or deletion time of the route

  public:

    AODVLDRouteData()
    {
        active = true;
        repariable = false;
        beingRepaired = false;
        validDestNum = true;
        lifeTime = SIMTIME_ZERO;
        destSeqNum = 0;
        minimumResidualLinklifetime = 0;//Todo wirklich NULL?
    }

    virtual ~AODVLDRouteData() {}

    unsigned int getDestSeqNum() const { return destSeqNum; }
    void setDestSeqNum(unsigned int destSeqNum) { this->destSeqNum = destSeqNum; }
    bool hasValidDestNum() const { return validDestNum; }
    void setHasValidDestNum(bool hasValidDestNum) { this->validDestNum = hasValidDestNum; }
    bool isBeingRepaired() const { return beingRepaired; }
    void setIsBeingRepaired(bool isBeingRepaired) { this->beingRepaired = isBeingRepaired; }
    bool isRepariable() const { return repariable; }
    void setIsRepariable(bool isRepariable) { this->repariable = isRepariable; }
    const simtime_t& getLifeTime() const { return lifeTime; }
    void setLifeTime(const simtime_t& lifeTime) { this->lifeTime = lifeTime; }
    bool isActive() const { return active; }
    void setIsActive(bool active) { this->active = active; }
    void addPrecursor(const L3Address& precursorAddr) { precursorList.insert(precursorAddr); }
    const std::set<L3Address>& getPrecursorList() const { return precursorList; }
    unsigned int getMinimumResidualLinklifetime() const { return minimumResidualLinklifetime; }
    void setMinimumResidualLinklifetime (unsigned int minimumResidualLinklifetime) { this->minimumResidualLinklifetime = minimumResidualLinklifetime; }
};

std::ostream& operator<<(std::ostream& out, const AODVLDRouteData *data);

} // namespace inet

#endif    // ifndef AODVLDROUTEDATA_H_

