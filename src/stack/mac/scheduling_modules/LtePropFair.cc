/*
 * LtePropFair.cc
 *
 *  Created on: Jan 27, 2018
 *      Author: kunterbunt
 */

#include "LtePropFair.h"
#include "stack/mac/scheduling_modules/LteTUGame/src/TransferableUtilityGame/FlowClassUpdater.h"
#include "stack/mac/scheduling_modules/LteTUGame/src/TransferableUtilityGame/EXP_PF_Rule/ExpPfRuleCalculator.h"
#include "common/oracle/Oracle.h"

using namespace std;

LtePropFair::LtePropFair() : LteSchedulerBase() {}

LtePropFair::~LtePropFair() {}

void LtePropFair::schedule(std::set<MacCid>& connections) {
	FlowClassUpdater::updatePlayers(connections, users);
	if (!users.empty()) {
		// Estimate data rates on all RBs for all users.
		for (TUGameUser* user : users) {
			vector<double> expectedDatarateVec;
			for (Band band = 0; band < Oracle::get()->getNumRBs(); band++) {
				double bytesOnBand = (double) getBytesOnBand(user->getNodeId(), band, 1, getDirection(user->getConnectionId()));
				expectedDatarateVec.push_back(bytesOnBand);
			}
			user->setExpectedDatarateVec(expectedDatarateVec);
		}

		// Apply Proportional Fair rule.
		for (Band resource  = 0; resource < Oracle::get()->getNumRBs(); resource++) {
			map<const TUGameUser*, double> map = ExpPfRule::calculate_propfair(users, resource);
			TUGameUser* best = users.at(0);
			for (TUGameUser* user : users) {
				if (map[user] > map[best])
					best = user;
			}
			// Schedule UE.
			scheduleUe(best->getConnectionId(), resource);
			// Update bytes allocated.
			best->updateBytesAllocated(best->getExpectedDatarateVec().at(resource));
//			cout << NOW << " RB " << resource << " -> "<< best->toString() << "." << endl;
		}
	}
}

void LtePropFair::reactToSchedulingResult(const SchedulingResult& result, unsigned int numBytesGranted, const MacCid& connection) {
    	for (TUGameUser* user : users) {
    		if (user->getConnectionId() == connection) {
    			user->updateDelay(numBytesGranted);
    			break;
    		}
    	}
    }
