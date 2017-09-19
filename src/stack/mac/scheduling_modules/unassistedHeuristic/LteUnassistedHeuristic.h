/*
 * LteMaxDatarate.h
 *
 *  Created on: Jul 26, 2017
 *      Author: kunterbunt
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_MAXDATARATE_LTEMAXDATARATE_H_
#define STACK_MAC_SCHEDULING_MODULES_MAXDATARATE_LTEMAXDATARATE_H_

#include <omnetpp.h>
#include <stack/mac/scheduler/LteScheduler.h>
#include "common/LteCommon.h"
#include <common/oracle/Oracle.h>
#include <stack/mac/scheduling_modules/maxDatarate/MaxDatarateSorter.h>
#include <common/oracle/SchedulingMemory.h>

typedef std::map<MacCid, unsigned int> ScheduleList;

class LteMacUeRealisticD2D;
class LteSchedulerUeAutoD2D;

class LteUnassistedHeuristic: public virtual LteScheduler {

public:

    // last execution time
    simtime_t lastExecutionTime_;



    // schedule List - returned by reference on scheduler invocation
    ScheduleList scheduleList_;

    /// Cid List
    typedef std::list<MacCid> CidList;

    enum SchedulingResult {
        OK = 0, TERMINATE, INACTIVE, INELIGIBLE
    };

    LteUnassistedHeuristic(LteSchedulerUeAutoD2D* lteSchedulerUeAutoD2D_ );

    virtual ~LteUnassistedHeuristic();

    /**
     * Apply the algorithm on temporal object 'activeConnectionTempSet_'.
     */
    virtual void prepareSchedule() override;

    /**
     * Put the results from prepareSchedule() into the actually-used object 'activeConnectionSet_'.
     */
    virtual void commitSchedule() override;



protected:


    /// Associated LteSchedulerUeUl (it is the one who creates the LteScheduler)
    LteSchedulerUeAutoD2D* ueScheduler_;
    /**
     * Determines first match for a D2D endpoint for this node
     * according to the mode selection map provided by the MaxDatarate mode selector.
     * @TODO Find a better way of finding the endpoint. Right now only one D2D peer is considered.
     * @param srcNode D2D channel starting node.
     * @return D2D channel end node.
     */
    MacNodeId determineD2DEndpoint(MacNodeId srcNode) const;
    typedef SortedDesc<MacCid, unsigned int> ScoreDesc;
    typedef std::priority_queue<ScoreDesc> ScoreList;

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


    void notifyActiveConnection(MacCid cid);

    void removeActiveConnection(MacCid cid);
};

#endif /* STACK_MAC_SCHEDULING_MODULES_MAXDATARATE_LTEMAXDATARATE_H_ */
