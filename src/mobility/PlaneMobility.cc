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

#include <mobility/PlaneMobility.h>


namespace inet {

Define_Module(PlaneMobility);

 void PlaneMobility::initialize(int stage)
{
    MovingMobilityBase::initialize(stage);

    EV_TRACE << "initializing PlaneMobility stage " << stage << endl;
    if (stage == INITSTAGE_LOCAL) {
        double minSpeed = par("minSpeed");
        double maxSpeed = par("maxSpeed");
        speed = uniform(minSpeed, maxSpeed);
        angle = uniform(1/360,1);
        lastSpeed.x = (cos(angle)*speed);
        lastSpeed.y = (sin(angle)*speed);

        stationary = (speed == 0);
    }
}
 void PlaneMobility::move()
 {
//Constant speed and constant direction. No changes needed
 }


PlaneMobility::PlaneMobility() {


}
}

