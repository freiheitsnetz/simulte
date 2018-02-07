/*
 * LteStackelbergGame.h
 *
 *  Created on: Dec 19, 2017
 *      Author: seba
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTESTACKELBERGGAME_LTESTACKELBERGGAME_H_
#define STACK_MAC_SCHEDULING_MODULES_LTESTACKELBERGGAME_LTESTACKELBERGGAME_H_

#include "stack/mac/scheduling_modules/LteSchedulerBase.h"
#include "common/oracle/Oracle.h"
#include "stack/mac/scheduling_modules/LteStackelbergGame/src/StackelbergUser.h"

using namespace std;

class LteStackelbergGame : public LteSchedulerBase {
public:
    LteStackelbergGame();

    virtual void schedule(std::set<MacCid>& connections) override;

protected:
    /** Limits for transmission power settings of D2D users. */
    double d2dTxPower_max = 0.0, d2dTxPower_min = 0.0;
    /** Adjusts how much the leader gains from the reuse fee. */
    double beta = 2.0;
    /** Adjusts how much past TTIs influence the priority of a follower (fairness). */
    double delta = 0.04;
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTESTACKELBERGGAME_LTESTACKELBERGGAME_H_ */
