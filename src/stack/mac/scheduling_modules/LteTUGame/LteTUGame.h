/*
 * LteTUGame.h
 *
 *  Created on: Dec 19, 2017
 *      Author: seba
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTETUGAME_LTETUGAME_H_
#define STACK_MAC_SCHEDULING_MODULES_LTETUGAME_LTETUGAME_H_

#include "stack/mac/scheduling_modules/LteSchedulerBase.h"
#include "common/oracle/Oracle.h"
#include "stack/mac/scheduling_modules/LteTUGame/TUGame.h"

using namespace std;

class LteTUGame : public LteSchedulerBase {
public:
    virtual void schedule(std::set<MacCid>& connections) override {
        EV << NOW << " LteTUGame::schedule" << std::endl;
        for (std::set<MacCid>::const_iterator iterator = connections.begin(); iterator != connections.end(); iterator++) {
            MacCid connection = *iterator;
            for (Band band = 0; band < Oracle::get()->getNumRBs(); band++) {
                scheduleUeReuse(connection, band);
            }
        }
    }

protected:
    Shapley::Coalition<TUGamePlayer> videoClass, voipClass, cbrClass;

    /**
     * Ensures the user classes correspond to the current TTI.
     */
    void setUserClasses(const std::set<MacCid>& connections) {
        // Clear all containers.
        videoClass.clear();
        voipClass.clear();
        cbrClass.clear();

        // Go through all current connections...
        for (const MacCid& connection : connections) {
            MacNodeId id = MacCidToNodeId(connection);

        }
    }

};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTETUGAME_LTETUGAME_H_ */
