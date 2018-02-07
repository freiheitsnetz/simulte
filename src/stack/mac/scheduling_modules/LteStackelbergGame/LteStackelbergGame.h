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
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTESTACKELBERGGAME_LTESTACKELBERGGAME_H_ */
