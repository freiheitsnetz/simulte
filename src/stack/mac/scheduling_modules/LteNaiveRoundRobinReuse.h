/*
 * LteNaiveRoundRobinReuse.h
 *
 *  Created on: Oct 18, 2017
 *      Author: kunterbunt
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTENAIVEROUNDROBINREUSE_H_
#define STACK_MAC_SCHEDULING_MODULES_LTENAIVEROUNDROBINREUSE_H_

#include "stack/mac/scheduling_modules/LteSchedulerBase.h"
#include "common/oracle/Oracle.h"
#include <random>

class LteNaiveRoundRobinReuse : public virtual LteNaiveRoundRobin {
public:
	virtual void schedule(std::set<MacCid>& connections) override {
		EV << NOW << " LteNaiveRoundRobinReuse::schedule" << std::endl;

//		cout << NOW << " " << connections.size() << " connections: ";
//		for (auto iterator = connections.begin(); iterator != connections.end(); iterator++)
//			cout << Oracle::get()->getName(MacCidToNodeId(*iterator)) << " ";
//		cout << endl;

		std::map<MacCid, std::vector<Band>> schedulingMap = getSchedulingMap(connections);

		std::map<MacCid, std::vector<Band>> schedulingMap_reuse = getSchedulingMap_reuse(connections, schedulingMap);

		for (ActiveSet::iterator iterator = connections.begin(); iterator != connections.end(); iterator++) {
			MacCid connection = *iterator;
			const std::vector<Band>& resources = schedulingMap[connection];
			if (!resources.empty()) {
//				cout << NOW << " Schedule ";
				for (const Band& resource : resources) {
					scheduleUeReuse(connection, resource);
//					cout << resource << " ";
				}
//				cout << "to node " << MacCidToNodeId(connection) << endl;
			}

			const std::vector<Band>& resources_reuse = schedulingMap_reuse[connection];
			if (!resources_reuse.empty()) {
//				cout << NOW << " Reuse ";
				for (const Band& resource_reuse : resources_reuse) {
					scheduleUeReuse(connection, resource_reuse);
//					cout << resource_reuse << " ";
				}
//				cout << "to node " << MacCidToNodeId(connection) << endl;
			}
		}



	}

protected:
	std::map<MacCid, std::vector<Band>> getSchedulingMap_reuse(const std::set<MacCid>& connections, const std::map<MacCid, std::vector<Band>>& schedulingMap) {
		std::map<MacCid, std::vector<Band>> map;
		for (ActiveSet::iterator iterator = connections.begin(); iterator != connections.end(); iterator++) {
			MacCid connection = *iterator;
			// Add empty resource list to scheduling map.
			map[connection] = std::vector<Band>();
		}

		// No chance of finding a UE that hasn't been scheduled a resource if there's only one.
		if (connections.size() == 1)
			return map;

		vector<MacCid> cids;
		for (std::map<MacCid, std::vector<Band>>::const_iterator it = schedulingMap.begin(); it != schedulingMap.end(); it++)
			cids.push_back((*it).first);

		std::random_device rd; // Obtain a random number from hardware.
		std::mt19937 eng(rd()); // Use Mersenne-Twister as generator, give it the random number as seed.
		std::uniform_int_distribution<> distr(0, connections.size() - 1); // Define the range.
		for (Band resource = 0; resource < Oracle::get()->getNumRBs(); resource++) {
			bool alreadyScheduled = true;
			unsigned int random;
			MacCid cid;
			do {
				random = distr(eng);
				// Make sure we've not randomly picked that UE that was already scheduled this resource.
				cid = cids.at(random);
				const std::vector<Band>& resources = schedulingMap.at(cid);

				bool found = false;
				for (const Band& scheduled_resource : resources) {
					if (scheduled_resource == resource) {
						found = true;
						break;
					}
				}
				if (found)
					continue;
				else
					alreadyScheduled = false;
			} while (alreadyScheduled);

			map[cids.at(random)].push_back(resource);
		}

		return map;
	}
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTENAIVEROUNDROBINREUSE_H_ */
