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

class LteMacUeD2D;
class LteSchedulerUeAutoD2D;

class LteUnassistedHeuristic: public virtual LteScheduler {

public:

    // last execution time
    simtime_t lastExecutionTime_;

    /// MAC module, used to get parameters from NED
    LteMacUeD2D *mac_;

    /// Associated LteSchedulerUeUl (it is the one who creates the LteScheduler)
    LteSchedulerUeAutoD2D* ueScheduler_;

    // schedule List - returned by reference on scheduler invocation
    ScheduleList scheduleList_;

    /// Cid List
    typedef std::list<MacCid> CidList;

    enum SchedulingResult {
        OK = 0, TERMINATE, INACTIVE, INELIGIBLE
    };

    LteUnassistedHeuristic();

    virtual ~LteUnassistedHeuristic();

    /**
     * Apply the algorithm on temporal object 'activeConnectionTempSet_'.
     */
    virtual void prepareSchedule() override;

    /**
     * Put the results from prepareSchedule() into the actually-used object 'activeConnectionSet_'.
     */
    virtual void commitSchedule() override;

    /**
     * When the LteSchedulerEnb learns of an active connection it notifies the LteScheduler.
     * It is essential to save this information. (I think this should be the default behaviour and be done in the LteScheduler class)
     */
    void notifyActiveConnection(MacCid cid) override {
        EV_STATICCONTEXT
        ;
        Oracle* oracle = Oracle::get();
        std::string name = (
                oracle == nullptr ?
                        std::to_string(MacCidToNodeId(cid)) :
                        Oracle::get()->getUeName(MacCidToNodeId(cid)));
        EV << NOW << " LteReassignment::notifyActiveConnection(" << name << ")"
                  << std::endl;
        activeConnectionSet_.insert(cid);
    }

    /**
     * When the LteSchedulerEnb learns of a connection going inactive it notifies the LteScheduler.
     */
    void removeActiveConnection(MacCid cid) override {
        EV_STATICCONTEXT
        ;
        Oracle* oracle = Oracle::get();
        std::string name = (
                oracle == nullptr ?
                        std::to_string(MacCidToNodeId(cid)) :
                        Oracle::get()->getUeName(MacCidToNodeId(cid)));
        EV << NOW << " LteReassignment::removeActiveConnection(" << name << ")"
                  << std::endl;
        activeConnectionSet_.erase(cid);
    }

protected:
    Oracle* mOracle = nullptr;
    SchedulingMemory* memory = nullptr;

    MaxDatarateSorter* sortBandsByDatarate(SchedulingMemory* memory);

    std::vector<Band> phase1_cellular(MaxDatarateSorter* sorter,
            SchedulingMemory* memory);

    void phase1_d2d(MaxDatarateSorter* sorter, SchedulingMemory* memory,
            std::vector<Band>& alreadyAssignedBands);

    /**
     * Double-assigns resource blocks to devices that left the first round empty-handed.
     * Corresponds to the algorithm's second step.
     */
    void phase2(MaxDatarateSorter* sorter, SchedulingMemory* memory);

    /**
     * Determines first match for a D2D endpoint for this node
     * according to the mode selection map provided by the MaxDatarate mode selector.
     * @TODO Find a better way of finding the endpoint. Right now only one D2D peer is considered.
     * @param srcNode D2D channel starting node.
     * @return D2D channel end node.
     */
    MacNodeId determineD2DEndpoint(MacNodeId srcNode) const;

    /**
     * Helper method that requests a grant for the specified connection.
     */
    LteUnassistedHeuristic::SchedulingResult schedule(MacCid connectionId,
            Band band);

    /**
     * Score-based schedulers descriptor.
     */
    template<typename T, typename S>
    struct SortedDesc {
        /// Connection identifier.
        T x_;
        /// Score value.
        S score_;

        /// Comparison operator to enable sorting.
        bool operator<(const SortedDesc& y) const {
            return score_ < y.score_;
        }

    public:
        SortedDesc(const T x, const S score) {
            x_ = x;
            score_ = score;
        }
    };

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
    ScheduleList& schedule(unsigned int availableBytes, Direction grantDir);
};

#endif /* STACK_MAC_SCHEDULING_MODULES_MAXDATARATE_LTEMAXDATARATE_H_ */
