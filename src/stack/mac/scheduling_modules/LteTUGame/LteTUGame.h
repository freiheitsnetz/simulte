/*
 * LteTUGame.h
 *
 *  Created on: Dec 19, 2017
 *      Author: seba
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTETUGAME_LTETUGAME_H_
#define STACK_MAC_SCHEDULING_MODULES_LTETUGAME_LTETUGAME_H_

#include "stack/mac/scheduling_modules/LteSchedulerBase.h"
#include "common/oracle/Oracle.h"
#include "stack/mac/scheduling_modules/LteTUGame/src/User.h"
#include "stack/mac/scheduling_modules/LteTUGame/src/FlowClassUpdater.h"
#include "stack/mac/scheduling_modules/LteTUGame/src/shapley/shapley.h"

using namespace std;

class LteTUGame : public LteSchedulerBase {
public:
	/**
	 * @return The user's type depending on the application it is running.
	 */
	static User::Type getUserType(const MacNodeId& nodeId) {
		string appName = Oracle::get()->getApplicationName(nodeId);
		EV << NOW << " LteTUGame::getUserType(" << appName << ")" << endl;
		if (appName == "VoIPSender")
			return User::Type::VOIP;
		else if (appName == "inet::UDPBasicApp")
			return User::Type::CBR;
		else
			throw invalid_argument("getUserType(" + appName + ") not supported.");
	}

	unsigned int getBytesPerBlock(const MacCid& connection) {
		MacNodeId nodeId = MacCidToNodeId(connection);

		unsigned int totalAvailableRBs = 0,
					 availableBytes = 0;

		// Determine direction.
		Direction dir;
		if (direction_ == UL)
			dir = (MacCidToLcid(connection) == D2D_SHORT_BSR) ? D2D : (MacCidToLcid(connection) == D2D_MULTI_SHORT_BSR) ? D2D_MULTI : UL;
		else
			dir = DL;

		// For each antenna...
		const UserTxParams& info = eNbScheduler_->mac_->getAmc()->computeTxParams(nodeId, dir);
		for (std::set<Remote>::iterator antennaIt = info.readAntennaSet().begin(); antennaIt != info.readAntennaSet().end(); antennaIt++) {
			// For each resource...
			for (Band resource = 0; resource != Oracle::get()->getNumRBs(); resource++) {
				// Determine number of RBs.
				unsigned int availableRBs = eNbScheduler_->readAvailableRbs(nodeId, *antennaIt, resource);
				totalAvailableRBs += availableRBs;
				if (availableRBs > 1)
					cerr << NOW << " LteTUGame::getBytesPerBlock(" << nodeId << ") with availableRBs==" << availableRBs << "!" << endl;
				// Determine number of bytes on this 'logical band' (which is a resource block if availableRBs==1).
				availableBytes += eNbScheduler_->mac_->getAmc()->computeBytesOnNRbs(nodeId, resource, availableRBs, dir);
			}
		}

		// current user bytes per slot
		return (totalAvailableRBs > 0) ? (availableBytes / totalAvailableRBs) : 0;
	}

	/**
	 * @return The total demand in bytes of 'connection' found by scanning the BSR buffer at the eNodeB.
	 */
	unsigned int getTotalDemand(const MacCid& connection) {
		LteMacBuffer* bsrBuffer = eNbScheduler_->bsrbuf_->at(connection);
		unsigned int bytesInBsrBuffer = bsrBuffer->getQueueOccupancy();
		return bytesInBsrBuffer;
	}

	/**
	 * @return Number of bytes the UE wants to send according to the first BSR stored.
	 */
	unsigned int getCurrentDemand(const MacCid& connection) {
		LteMacBuffer* bsrBuffer = eNbScheduler_->bsrbuf_->at(connection);
		return bsrBuffer->front().first;
	}

    virtual void schedule(std::set<MacCid>& connections) override {
        EV << NOW << " LteTUGame::schedule" << std::endl;
        // Update player list - adds new players and updates their active status.
        EV << NOW << " LteTUGame::updatePlayers" << std::endl;
        FlowClassUpdater::updatePlayers(connections, users, LteTUGame::getUserType);

        // Update classes to contain all corresponding active players.
        EV << NOW << " LteTUGame::updateClasses" << std::endl;
        FlowClassUpdater::updateClasses(users, classCbr, classVoip, classVid);

        // Print status.
		EV << "\t" << classVid.size() << " video flows:\n\t";
		for (const User* user : classVid.getMembers())
			EV << user->toString() << " ";
		EV << endl;
		EV << "\t" << classVoip.size() << " VoIP flows:\n\t";
		for (const User* user : classVoip.getMembers())
			EV << user->toString() << " ";
		EV << endl;
		EV << "\t" << classCbr.size() << " CBR flows:\n\t";
		for (const User* user : classCbr.getMembers())
			EV << user->toString() << " ";
		EV << endl;

		try {
			if (NOW >= 1.0 && NOW <= 1.1 )
				for (const User* user : users)
					cout << NOW << " bsrBuffer(" << user->toString() << ")=" << getTotalDemand(user->getConnectionId()) << endl;
		} catch (const exception& e) {

		}
    }

    virtual ~LteTUGame() {
    	for (User* user : users)
    		delete user;
    	users.clear();
    }

protected:
    std::vector<User*> users;
    Shapley::Coalition<User> classCbr, classVoip, classVid;
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTETUGAME_LTETUGAME_H_ */
