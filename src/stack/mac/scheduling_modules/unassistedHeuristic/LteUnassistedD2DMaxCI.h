/*
 * LteUnassistedD2DMaxCI.cc
 *
 *  Created on: Aug 21, 2017
 *      Author: Sunil Upardrasta
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_UNASSISTEDHEURISTIC_LteUnassistedD2DMaxCI_H_
#define STACK_MAC_SCHEDULING_MODULES_UNASSISTEDHEURISTIC_LteUnassistedD2DMaxCI_H_

#include <stack/mac/scheduling_modules/unassistedHeuristic/LteUnassistedD2DSchedulingAgent.h>

class LteUnassistedD2DMaxCI: public virtual LteUnassistedD2DSchedulingAgent {
protected:

    typedef SortedDesc<MacCid, unsigned int> ScoreDesc;
    typedef std::priority_queue<ScoreDesc> ScoreList;

public:
    LteUnassistedD2DMaxCI(LteSchedulerUeUnassistedD2D* lteSchedulerUeUnassistedD2D_) {

        ueScheduler_ = lteSchedulerUeUnassistedD2D_;
    }

    ~LteUnassistedD2DMaxCI() {
    }

    virtual void prepareSchedule();

    virtual void commitSchedule();

    // *****************************************************************************************

    void notifyActiveConnection(MacCid cid);

    void removeActiveConnection(MacCid cid);

    void updateSchedulingInfo();
};

#endif /* STACK_MAC_SCHEDULING_MODULES_UNASSISTEDHEURISTIC_LteUnassistedD2DMaxCI_H_ */
