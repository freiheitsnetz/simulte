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

/**
 * Implementation of the algorithm proposed by
 *  Ma, Ruofei
 *  Xia, Nian
 *  Chen, Hsiao-Hwa
 *  Chiu, Chun-Yuan
 *  Yang, Chu-Sing
 * from the National Cheng Kung University in Taiwan,
 * from their paper 'Mode Selection, Radio Resource Allocation, and Power Coordination in D2D Communications',
 * published in 'IEEE Wireless Communications' in 2017.
 */
class LteUnassistedHeuristic : public virtual LteScheduler {
public:
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
        EV_STATICCONTEXT;
        Oracle* oracle = Oracle::get();
        std::string name = (oracle == nullptr ? std::to_string(MacCidToNodeId(cid)) : Oracle::get()->getUeName(MacCidToNodeId(cid)));
        EV << NOW << " LteReassignment::notifyActiveConnection(" << name << ")" << std::endl;
        activeConnectionSet_.insert(cid);
    }

    /**
     * When the LteSchedulerEnb learns of a connection going inactive it notifies the LteScheduler.
     */
    void removeActiveConnection(MacCid cid) override {
        EV_STATICCONTEXT;
        Oracle* oracle = Oracle::get();
        std::string name = (oracle == nullptr ? std::to_string(MacCidToNodeId(cid)) : Oracle::get()->getUeName(MacCidToNodeId(cid)));
        EV << NOW << " LteReassignment::removeActiveConnection(" << name << ")" << std::endl;
        activeConnectionSet_.erase(cid);
    }

protected:
    Oracle* mOracle = nullptr;
    SchedulingMemory* memory = nullptr;

    MaxDatarateSorter* sortBandsByDatarate(SchedulingMemory* memory);

    std::vector<Band> phase1_cellular(MaxDatarateSorter* sorter, SchedulingMemory* memory);

    void phase1_d2d(MaxDatarateSorter* sorter, SchedulingMemory* memory, std::vector<Band>& alreadyAssignedBands);

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
    LteUnassistedHeuristic::SchedulingResult schedule(MacCid connectionId, Band band);
};

#endif /* STACK_MAC_SCHEDULING_MODULES_MAXDATARATE_LTEMAXDATARATE_H_ */
