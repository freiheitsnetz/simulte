/*
 * LteStackelbergGame.h
 *
 *  Created on: Dec 19, 2017
 *      Author: seba
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTESTACKELBERGGAME_LTESTACKELBERGGAME_H_
#define STACK_MAC_SCHEDULING_MODULES_LTESTACKELBERGGAME_LTESTACKELBERGGAME_H_

#include "stack/mac/scheduling_modules/LteSchedulerBase.h"
#include "stack/mac/scheduling_modules/LteNaiveRoundRobin.h"
#include "stack/mac/scheduling_modules/LteTUGame/LteTUGame.h"

class LteStackelbergGame : public LteSchedulerBase {
public:
    const static std::string LEADER_SCHEDULER_RR, LEADER_SCHEDULER_TU;

    LteStackelbergGame();

    virtual ~LteStackelbergGame();

    virtual void schedule(std::set<MacCid>& connections) override;

protected:
    /** Limits for transmission power settings of D2D users. */
    double d2dTxPower_max = 0.0, d2dTxPower_min = 0.0;
    /** Adjusts how much the leader gains from the reuse fee. */
    double beta = 2.0;
    /** Adjusts how much past TTIs influence the priority of a follower (fairness). */
    double delta = 0.04;

    /** This function returns a scheduling map. It is assigned differently in the constructor based on the leader scheduling discipline. */
    std::function<std::map<MacCid, std::vector<Band>> (const std::set<MacCid>& connections)> scheduleLeaders;

private:
    LteNaiveRoundRobin* scheduler_rr = nullptr;
    LteTUGame* scheduler_tu = nullptr;
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTESTACKELBERGGAME_LTESTACKELBERGGAME_H_ */
