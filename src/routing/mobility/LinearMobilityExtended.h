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

#ifndef __SIMULTE_LINEARMOBILITYEXTENDED_H_
#define __SIMULTE_LINEARMOBILITYEXTENDED_H_



#include "inet/common/INETDefs.h"

#include "inet/mobility/base/MovingMobilityBase.h"

namespace inet {

/**
 * @brief Linear movement model. See NED file for more info.
 *
 * @ingroup mobility
 * @author Emin Ilker Cetinbas
 */
class INET_API LinearMobilityExtended : public MovingMobilityBase
{
  protected:
    double speed;    ///< speed of the host
    double angle;    ///< angle of linear motion
    double acceleration;    ///< acceleration of linear motion

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }

    /** @brief Initializes mobility model parameters.*/
    virtual void initialize(int stage) override;

    /** @brief Move the host*/
    virtual void move() override;

  public:
    virtual double getMaxSpeed() const override { return speed; }
    //AddedS
    double getAngle() { return angle; }
    LinearMobilityExtended();
};

} // namespace inet

#endif // ifndef __INET_LINEARMOBILITY_H

