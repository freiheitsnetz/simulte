/*
 * LteNaiveRoundRobinReuse.h
 *
 *  Created on: Oct 18, 2017
 *      Author: kunterbunt
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTENAIVEROUNDROBINREUSE_H_
#define STACK_MAC_SCHEDULING_MODULES_LTENAIVEROUNDROBINREUSE_H_

#include "stack/mac/scheduling_modules/LteSchedulerBase.h"

class LteNaiveRoundRobinReuse : public virtual LteSchedulerBase {
public:
	virtual void schedule(std::set<MacCid>& connections) override {
	// Add all new connections to the history.
	for (ActiveSet::iterator iterator = connections.begin(); iterator != connections.end(); iterator++) {
		MacCid connection = *iterator;
		if (schedulingHistory.find(connection) == schedulingHistory.end())
			schedulingHistory[connection] = 0;
	}

	EV << NOW << " LteNaiveRoundRobinReuse::schedule" << std::endl;

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

		schedulingDecisions[connection].push_back(band);
		schedulingHistory[connection]++;
	}
}

protected:
	std::map<MacCid, size_t> schedulingHistory;
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTENAIVEROUNDROBINREUSE_H_ */
