/*
 * LteUnassistedD2DPF.cc
 *
 *  Created on: Aug 21, 2017
 *      Author: Sunil Upardrasta
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_UNASSISTEDHEURISTIC_LTEUNASSISTEDD2DPF_H_
#define STACK_MAC_SCHEDULING_MODULES_UNASSISTEDHEURISTIC_LTEUNASSISTEDD2DPF_H_

#include <omnetpp.h>
#include <stack/mac/scheduler/LteScheduler.h>
#include "common/LteCommon.h"
#include <common/oracle/Oracle.h>
#include <common/oracle/SchedulingMemory.h>.
#include <stack/mac/scheduling_modules/unassistedHeuristic/LteUnassistedD2DSchedulingAgent.h>

typedef std::map<MacCid, unsigned int> ScheduleList;

class LteMacUeRealisticD2D;
class LteSchedulerUeUnassistedD2D;

class LteUnassistedD2DPF: public LteUnassistedD2DSchedulingAgent {

protected:

    /// Associated LteSchedulerUeUl (it is the one who creates the LteScheduler)
    LteSchedulerUeUnassistedD2D* ueScheduler_;

    typedef std::map<MacCid, double> PfRate;
    typedef SortedDesc<MacCid, double> ScoreDesc;
    typedef std::priority_queue<ScoreDesc> ScoreList;

    //! Long-term rates, used by PF scheduling.
    PfRate pfRate_;

    //! Granted bytes
    std::map<MacCid, unsigned int> grantedBytes_;

    //! Smoothing factor for proportional fair scheduler.
    double pfAlpha_;

    //! Small number to slightly blur away scores.
    const double scoreEpsilon_;

public:
    double & pfAlpha() {
        return pfAlpha_;
    }

    ~LteUnassistedD2DPF() {
    }

    LteUnassistedD2DPF(double pfAlpha, LteSchedulerUeUnassistedD2D* lteSchedulerUeUnassistedD2D) :
        scoreEpsilon_(0.000001)
    {
        pfAlpha_ = pfAlpha;
        pfRate_.clear();
        ueScheduler_=lteSchedulerUeUnassistedD2D;
    }

    // Scheduling functions ********************************************************************

    //virtual void schedule ();

    virtual void prepareSchedule();

    virtual void commitSchedule();

    // *****************************************************************************************

    void notifyActiveConnection(MacCid cid);

    void removeActiveConnection(MacCid cid);

    void updateSchedulingInfo();
};

#endif /* STACK_MAC_SCHEDULING_MODULES_UNASSISTEDHEURISTIC_LTEUNASSISTEDD2DPF_H_ */
