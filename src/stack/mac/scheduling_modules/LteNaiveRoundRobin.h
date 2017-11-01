/*
 * LteNaiveRoundRobin.h
 *
 *  Created on: Oct 18, 2017
 *      Author: kunterbunt
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTENAIVEROUNDROBIN_H_
#define STACK_MAC_SCHEDULING_MODULES_LTENAIVEROUNDROBIN_H_

#include <omnetpp.h>
#include "stack/mac/scheduling_modules/LteSchedulerBase.h"
#include "common/LteCommon.h"
#include <map>
#include <vector>

/**
 * A pure (non-deficit-)round-robin scheduler implementation: the available resources are distributed per user.
 * Note that this implementation assumes that transmitters are called ueD2DTx - the maximum ID that a transmitter can have must
 * be determined beforehand (see first part of prepareSchedule()).
 */
class LteNaiveRoundRobin : public virtual LteSchedulerBase {
public:
	virtual void prepareSchedule() override {
		// Copy currently active connections to a working copy.
		activeConnectionTempSet_ = activeConnectionSet_;
		numTTIs++;
		if (activeConnectionTempSet_.size() == 0) {
			numTTIsWithNoActives++;
			return;
		}

		// Prepare this round's scheduling decision map.
		schedulingDecisions.clear();
		// Add all new connections to the history.
		for (ActiveSet::iterator iterator = activeConnectionTempSet_.begin(); iterator != activeConnectionTempSet_.end(); iterator++) {
			MacCid connection = *iterator;
			schedulingDecisions[connection] = std::vector<Band>();
			if (schedulingHistory.find(connection) == schedulingHistory.end())
				schedulingHistory[connection] = 0;
		}

		EV << NOW << " LteNaiveRoundRobin::prepareSchedule" << std::endl;

		for (Band band = 0; band < getBinder()->getNumBands(); band++) {
			// Initiate with first connection.
			MacCid connection = *activeConnectionTempSet_.begin();
			MacNodeId id = MacCidToNodeId(connection);
			// Find that active connection that has received fewest RBs so far.
			for (ActiveSet::iterator iterator = activeConnectionTempSet_.begin(); iterator != activeConnectionTempSet_.end(); iterator++) {
				MacCid currentConnection = *iterator;
				MacNodeId currentId = MacCidToNodeId(currentConnection);
				if (schedulingHistory[currentConnection] < schedulingHistory[connection]) {
					connection = currentConnection;
					id = currentId;
				}
			}

			schedulingDecisions[connection].push_back(band);
			schedulingHistory[connection]++;
		}
	}

	virtual void commitSchedule() override {
		EV_STATICCONTEXT;
		EV << NOW << " LteNaiveRoundRobin::commitSchedule" << std::endl;
		activeConnectionSet_ = activeConnectionTempSet_;
		for (auto const &item : schedulingDecisions) {
		    MacCid connection = item.first;
		    std::vector<Band> resources = item.second;
		    SchedulingResult result = schedule(connection, resources);
		}
	}

protected:
	std::map<MacCid, size_t> schedulingHistory;
	std::map<MacCid, std::vector<Band>> schedulingDecisions;
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTENAIVEROUNDROBIN_H_ */
