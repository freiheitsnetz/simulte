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
    virtual void schedule(std::set<MacCid>& connections) override {
        EV << NOW << " LteStackelbergGame::schedule" << std::endl;

        StackelbergUser user(1, 1);
        for (std::set<MacCid>::const_iterator iterator = connections.begin(); iterator != connections.end(); iterator++) {
            MacCid connection = *iterator;
            for (Band band = 0; band < Oracle::get()->getNumRBs(); band++) {
                scheduleUeReuse(connection, band);
            }
        }
    }
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTESTACKELBERGGAME_LTESTACKELBERGGAME_H_ */
