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
	virtual void schedule(std::set<MacCid>& connections) override {
		// Add all new connections to the history.
		for (ActiveSet::iterator iterator = connections.begin(); iterator != connections.end(); iterator++) {
			MacCid connection = *iterator;
			if (schedulingHistory.find(connection) == schedulingHistory.end())
				schedulingHistory[connection] = 0;
		}

		EV << NOW << " LteNaiveRoundRobin::schedule" << std::endl;

		for (Band band = 0; band < getBinder()->getNumBands(); band++) {
			// Initiate with first connection.
			MacCid connection = *connections.begin();
			MacNodeId id = MacCidToNodeId(connection);
			// Find that active connection that has received fewest RBs so far.
			for (ActiveSet::iterator iterator = connections.begin(); iterator != connections.end(); iterator++) {
				MacCid currentConnection = *iterator;
				MacNodeId currentId = MacCidToNodeId(currentConnection);
				if (schedulingHistory[currentConnection] < schedulingHistory[connection]) {
					connection = currentConnection;
					id = currentId;
				}
			}

			scheduleUe(connection, band);
			schedulingHistory[connection]++;
		}

		for (ActiveSet::iterator iterator = connections.begin(); iterator != connections.end(); iterator++) {
			MacCid connection = *iterator;
			cout << NOW << " " << dirToA(direction_) << ": " << MacCidToNodeId(connection) << ":" << getNumRequiredRBs(connection) << endl;
		}
	}

protected:
	std::map<MacCid, size_t> schedulingHistory;
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTENAIVEROUNDROBIN_H_ */
