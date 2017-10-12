/*
 * LteUnassistedD2DSchedulingAgent.cc
 *
 *  Created on: Aug 21, 2017
 *      Author: Sunil Upardrasta
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_UNASSISTEDHEURISTIC_LTEUNASSISTEDD2DSCHEDULINGAGENT_H_
#define STACK_MAC_SCHEDULING_MODULES_UNASSISTEDHEURISTIC_LTEUNASSISTEDD2DSCHEDULINGAGENT_H_

#include <omnetpp.h>
#include <stack/mac/scheduler/LteScheduler.h>
#include "common/LteCommon.h"
#include <common/oracle/Oracle.h>
#include <common/oracle/SchedulingMemory.h>

typedef std::map<MacCid, unsigned int> ScheduleList;

class LteMacUeRealisticD2D;
class LteSchedulerUeUnassistedD2D;

class LteUnassistedD2DSchedulingAgent: public virtual LteScheduler {

public:

    // last execution time
    simtime_t lastExecutionTime_;

    // schedule List - returned by reference on scheduler invocation
    ScheduleList scheduleList_;

protected:

    /// Associated LteSchedulerUeUl (it is the one who creates the LteScheduler)
    LteSchedulerUeUnassistedD2D* ueScheduler_;

    struct StatusElem {
        unsigned int occupancy_;
        unsigned int bucket_;
        unsigned int sentData_;
        unsigned int sentSdus_;
    };

    // scheduling status map
    std::map<MacCid, StatusElem> statusMap_;

public:
    /**
     * Helper method that requests a grant for the specified connection.
     */
    ScheduleList& scheduleData(unsigned int availableBytes, Direction grantDir);
    /**
     * Helper method that requests a grant for the specified connection.
     */
    void scheduleTxD2Ds();
};

#endif /* STACK_MAC_SCHEDULING_MODULES_UNASSISTEDHEURISTIC_LTEUNASSISTEDD2DSCHEDULINGAGENT_H_ */
