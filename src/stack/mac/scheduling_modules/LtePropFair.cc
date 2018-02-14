/*
 * LtePropFair.cc
 *
 *  Created on: Jan 27, 2018
 *      Author: kunterbunt
 */

#include "LtePropFair.h"
#include "stack/mac/scheduling_modules/LteTUGame/src/FlowClassUpdater.h"
#include "stack/mac/scheduling_modules/LteTUGame/src/EXP_PF_Rule/ExpPfRuleCalculator.h"
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

		std::map<unsigned short, const TUGameUser*> allocationMap = ExpPfRule::apply_propfair(users, Oracle::get()->getNumRBs(),
                [this](unsigned int connectionId, unsigned int numBytes) {
                    for (TUGameUser* user : users)
                        if (user->getConnectionId() == connectionId) {
                            user->updateBytesAllocated(numBytes);
                            return;
                        }
                });
		// Apply Proportional Fair rule.
		for (Band resource  = 0; resource < Oracle::get()->getNumRBs(); resource++) {
			const TUGameUser* user = allocationMap.at(resource);
			// Schedule UE.
			scheduleUe(user->getConnectionId(), resource);
//			cout << NOW << " RB " << resource << " -> "<< user->toString() << "." << endl;
		}
	}
}

void LtePropFair::commitSchedule() {
    LteSchedulerBase::commitSchedule();
    for (TUGameUser* user : users)
        user->onTTI();
}
