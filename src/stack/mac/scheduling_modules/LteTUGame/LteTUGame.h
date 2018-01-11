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
#include "stack/mac/scheduling_modules/LteTUGame/src/shapley/TUGame.h"
#include "stack/mac/scheduling_modules/LteTUGame/src/EXP_PF_Rule/ExpPfRuleCalculator.h"

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

	/**
	 * Sets user's maximum delay and bitrate for VoIP or Video Streaming applications, taken from the paper on Transferable Utility games.
	 */
	static void setRealtimeValues(User* user) {
		if (user->getType() == User::Type::VOIP) {
			user->setRealtimeTarget(100/*ms*/, 8/*bit per TTI*/);
			EV << NOW << " LteTUGame::setRealtimeValues(" << user->toString() << ") realtime values set to VoIP." << endl;
		} else if (user->getType() == User::Type::VIDEO) {
			user->setRealtimeTarget(100/*ms*/, 242/*bit per TTI*/);
			EV << NOW << " LteTUGame::setRealtimeValues(" << user->toString() << ") realtime values set to Video." << endl;
		} else
			throw invalid_argument("LteTUGame::setRealtimeValues(" + user->toString() + ") not supported.");
	}

    virtual void schedule(std::set<MacCid>& connections) override {
        EV << NOW << " LteTUGame::schedule" << std::endl;

        // Update player list - adds new players and updates their active status.
        EV << NOW << " LteTUGame::updatePlayers" << std::endl;
        FlowClassUpdater::updatePlayers(connections, users, LteTUGame::getUserType, LteTUGame::setRealtimeValues);

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

		/** Demand in resource blocks.*/
		unsigned long classDemandCbr = 0,
					  classDemandVoip = 0,
					  classDemandVid = 0;
		// Constant Bitrate users.
		for (const User* user : classCbr.getMembers()) {
			unsigned int byteDemand = getTotalDemand(user->getConnectionId());
			unsigned int rbDemand = getRBDemand(user->getConnectionId(), byteDemand);
			classDemandCbr += rbDemand;
		}
		// Voice-over-IP users.
		for (const User* user : classVoip.getMembers()) {
			unsigned int byteDemand = getTotalDemand(user->getConnectionId());
			unsigned int rbDemand = getRBDemand(user->getConnectionId(), byteDemand);
			classDemandVoip += rbDemand;
		}
		// Video streaming users.
		for (const User* user : classVid.getMembers()) {
			unsigned int byteDemand = getTotalDemand(user->getConnectionId());
			unsigned int rbDemand = getRBDemand(user->getConnectionId(), byteDemand);
			classDemandVid += rbDemand;
		}

		// Apply Shapley's value to find fair division of available resources to our user classes.
		TUGame_Shapley::TUGamePlayer shapley_cbr(classDemandCbr),
									 shapley_voip(classDemandVoip),
									 shapley_vid(classDemandVid);
		Shapley::Coalition<TUGame_Shapley::TUGamePlayer> players;
		players.add(&shapley_cbr);
		players.add(&shapley_voip);
		players.add(&shapley_vid);
		unsigned int numRBs = Oracle::get()->getNumRBs();
		std::map<const TUGame_Shapley::TUGamePlayer*, double> shapleyValues = TUGame_Shapley::play(players, numRBs);

		// Estimate data rates on all RBs for all users.
		for (User* user : users) {
			vector<double> expectedDatarateVec;
			for (Band band = 0; band < Oracle::get()->getNumRBs(); band++) {
				double bytesOnBand = (double) getBytesOnBand(user->getNodeId(), band, 1, getDirection(user->getConnectionId()));
				expectedDatarateVec.push_back(bytesOnBand);
			}
			user->setExpectedDatarateVec(expectedDatarateVec);
		}

		// For each user class, distribute the RBs provided by Shapley among the flows in the class according to the EXP-PF-Rule.
		nextBandToAllocate = 0;
		unsigned int totalBandsToAllocate = shapleyValues[&shapley_cbr] + shapleyValues[&shapley_voip] + shapleyValues[&shapley_vid];
		if (totalBandsToAllocate > 0) {
			EV << NOW << " LteTUGame Resource Block Distribution... " << endl;
			EV << "\tDistributing " << shapleyValues[&shapley_cbr] << "/" << numRBs << "RBs to " << classCbr.size() << " CBR flows that require " << classDemandCbr << "... " << endl;
			applyEXP_PF_Rule(classCbr, shapleyValues[&shapley_cbr]);

			EV << "\tDistributing " << shapleyValues[&shapley_voip] << "/" << numRBs << "RBs to " << classVoip.size() << " VoIP flows that require " << classDemandVoip << "... " << endl;
			applyEXP_PF_Rule(classVoip, shapleyValues[&shapley_voip]);

			EV << "\tDistributing " << shapleyValues[&shapley_vid] << "/" << numRBs << "RBs to " << classVid.size() << " Video flows that require " << classDemandVid << "... " << endl;
			applyEXP_PF_Rule(classVid, shapleyValues[&shapley_vid]);
		}
    }

    virtual void reactToSchedulingResult(const SchedulingResult& result, unsigned int numBytesGranted, const MacCid& connection) override {
    	for (User* user : users) {
    		if (user->getConnectionId() == connection) {
    			// Remember number of bytes served so that future metric computation takes it into account.
    			user->onTTI(numBytesGranted);
    			break;
    		}
    	}
    }

    virtual ~LteTUGame() {
    	for (User* user : users)
    		delete user;
    }

protected:
    std::vector<User*> users;
    Shapley::Coalition<User> classCbr, classVoip, classVid;
    Band nextBandToAllocate = 0;

    /**
	 * @return Can be D2D, D2D_MULTI, UL, DL.
	 */
	Direction getDirection(const MacCid& connection) {
		Direction dir;
		if (direction_ == UL)
			dir = (MacCidToLcid(connection) == D2D_SHORT_BSR) ? D2D : (MacCidToLcid(connection) == D2D_MULTI_SHORT_BSR) ? D2D_MULTI : UL;
		else
			dir = DL;
		return dir;
	}

	/**
	 * @param nodeId
	 * @param band The logical band you're interested in.
	 * @param numBlocks The number of resource blocks available on the band (should be 1!).
	 * @param dir
	 */
	unsigned int getBytesOnBand(const MacNodeId& nodeId, const Band& band, const unsigned int& numBlocks, const Direction& dir) {
		return eNbScheduler_->mac_->getAmc()->computeBytesOnNRbs(nodeId, band, numBlocks, dir);
	}

	/**
	 * @return Average number of bytes available on 1 RB.
	 */
	unsigned int getAverageBytesPerBlock(const MacCid& connection) {
		MacNodeId nodeId = MacCidToNodeId(connection);

		unsigned int totalAvailableRBs = 0,
					 availableBytes = 0;

		// Determine direction.
		Direction dir = getDirection(connection);

		// For each antenna...
		const UserTxParams& info = eNbScheduler_->mac_->getAmc()->computeTxParams(nodeId, dir);
		for (std::set<Remote>::iterator antennaIt = info.readAntennaSet().begin(); antennaIt != info.readAntennaSet().end(); antennaIt++) {
			// For each resource...
			for (Band resource = 0; resource != Oracle::get()->getNumRBs(); resource++) {
				// Determine number of RBs.
				unsigned int availableRBs = eNbScheduler_->readAvailableRbs(nodeId, *antennaIt, resource);
				totalAvailableRBs += availableRBs;
				if (availableRBs > 1)
					cerr << NOW << " LteTUGame::getAverageBytesPerBlock(" << nodeId << ") with availableRBs==" << availableRBs << "!" << endl;
				// Determine number of bytes on this 'logical band' (which is a resource block if availableRBs==1).
				availableBytes += getBytesOnBand(nodeId, resource, availableRBs, dir);
			}
		}

		// Average number of bytes available on 1 RB.
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

	/**
	 * @return Number of blocks required to serve 'numBytes' for 'connection'.
	 */
	unsigned int getRBDemand(const MacCid& connection, const unsigned int& numBytes) {
		unsigned int bytesPerBlock = getAverageBytesPerBlock(connection);
		return bytesPerBlock > 0 ? numBytes / bytesPerBlock : 0;
	}

	void applyEXP_PF_Rule(const Shapley::Coalition<User>& flowClass, unsigned int numRBs) {
		// Allocate CBR flows.
		for (unsigned int numAllocated = 0; numAllocated < numRBs; numAllocated++) {
			assert(nextBandToAllocate < Oracle::get()->getNumRBs());
			// Get metrics for all flows.
			map<const User*, double> metricsMap = ExpPfRule::calculate(flowClass.getMembers(), nextBandToAllocate);
			// Find winner.
			double largestMetric = 0.0;
			const User* userWithLargestMetric = nullptr;
			for (const User* user : flowClass.getMembers()) {
				if (metricsMap[user] > largestMetric) {
					largestMetric = metricsMap[user];
					userWithLargestMetric = user;
				}
			}
			// Allocate resource to winner.
			scheduleUe(userWithLargestMetric->getConnectionId(), nextBandToAllocate);
			// Due to the 'const' I can't update the allocated bytes directly, but have to search for the corresponding user in 'users'... meh.
			for (User* user: users) {
				if (user->getConnectionId() == userWithLargestMetric->getConnectionId()) {
					user->updateBytesAllocated(userWithLargestMetric->getExpectedDatarateVec().at(nextBandToAllocate));
					break;
				}
			}
			EV << "\t\t" << userWithLargestMetric->toString() << " got RB " << nextBandToAllocate << " with metric of " << largestMetric << " > ";
			EV << endl;
			nextBandToAllocate++;
		}
	}
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTETUGAME_LTETUGAME_H_ */
