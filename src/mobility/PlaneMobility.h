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

#ifndef MOBILITY_PLANEMOBILITY_H_
#define MOBILITY_PLANEMOBILITY_H_


#include "inet/mobility/base/LineSegmentsMobilityBase.h"

namespace inet {

class INET_API PlaneMobility : public MovingMobilityBase{

protected:

    float speed;
    float angle;

public:
    PlaneMobility();
    virtual ~PlaneMobility();
    virtual void initialize(int stage) override;
    void move() override;

};
}
#endif /* MOBILITY_PLANEMOBILITY_H_ */
