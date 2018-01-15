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
		if (appName == "VoIPSender" || appName == "inet::SimpleVoIPSender")
			return User::Type::VOIP;
		else if (appName == "inet::UDPBasicApp" || appName == "inet::UDPSink")
			return User::Type::CBR;
		else
			throw invalid_argument("getUserType(" + appName + ") not supported.");
	}

	/**
	 * Sets user's maximum delay and bitrate for VoIP or Video Streaming applications, taken from the paper on Transferable Utility games.
	 */
	static void setRealtimeValues(User* user) {
		// Set user type.
		if (user->getType() == User::Type::VOIP) {
			user->setRealtimeTarget(100/*ms*/, 8/*bit per TTI*/);
			EV << NOW << " LteTUGame::setRealtimeValues(" << user->toString() << ") realtime values set to VoIP." << endl;
		} else if (user->getType() == User::Type::VIDEO) {
			user->setRealtimeTarget(100/*ms*/, 242/*bit per TTI*/);
			EV << NOW << " LteTUGame::setRealtimeValues(" << user->toString() << ") realtime values set to Video." << endl;
		} else
			throw invalid_argument("LteTUGame::setRealtimeValues(" + user->toString() + ") not supported.");
	}

	static void setD2D(User* user) {
		user->setD2D(Oracle::get()->isD2DFlow(user->getConnectionId()));
	}

    virtual void schedule(std::set<MacCid>& connections) override {
        EV << NOW << " LteTUGame::schedule" << std::endl;

        // Update player list - adds new players and updates their active status.
        EV << NOW << " LteTUGame::updatePlayers" << std::endl;
        FlowClassUpdater::updatePlayers(connections, users, LteTUGame::getUserType, LteTUGame::setRealtimeValues, LteTUGame::setD2D);

        for (const User* user : users) {
        	cout << NOW << " " << dirToA(direction_) << " " << user->toString() << " demands " << getTotalDemand(user->getConnectionId()) << " bytes = " << getRBDemand(user->getConnectionId(), getTotalDemand(user->getConnectionId())) << " RBs." << endl;
//        		for (Band band = 0; band < Oracle::get()->getNumRBs(); band++)
//        			scheduleUe(user->getConnectionId(), band);
        }


//        // Update classes to contain all corresponding active players.
//        EV << NOW << " LteTUGame::updateClasses" << std::endl;
//        FlowClassUpdater::updateClasses(users, classCbr, classVoip, classVid);
//
//        // Print status.
//		EV << "\t" << classVid.size() << " video flows:\n\t";
//		for (const User* user : classVid.getMembers())
//			EV << user->toString() << " ";
//		EV << endl;
//		EV << "\t" << classVoip.size() << " VoIP flows:\n\t";
//		for (const User* user : classVoip.getMembers())
//			EV << user->toString() << " ";
//		EV << endl;
//		EV << "\t" << classCbr.size() << " CBR flows:\n\t";
//		for (const User* user : classCbr.getMembers())
//			EV << user->toString() << " ";
//		EV << endl;
//
//		/** Demand in resource blocks.*/
//		unsigned long classDemandCbr = 0,
//					  classDemandVoip = 0,
//					  classDemandVid = 0;
//		// Constant Bitrate users.
//		for (const User* user : classCbr.getMembers()) {
//			unsigned int byteDemand = getCurrentDemand(user->getConnectionId());
//			unsigned int rbDemand = getRBDemand(user->getConnectionId(), byteDemand);
//			classDemandCbr += rbDemand;
//		}
//		// Voice-over-IP users.
//		for (const User* user : classVoip.getMembers()) {
//			unsigned int byteDemand = getCurrentDemand(user->getConnectionId());
//			unsigned int rbDemand = getRBDemand(user->getConnectionId(), byteDemand);
//			classDemandVoip += rbDemand;
//		}
//		// Video streaming users.
//		for (const User* user : classVid.getMembers()) {
//			unsigned int byteDemand = getCurrentDemand(user->getConnectionId());
//			unsigned int rbDemand = getRBDemand(user->getConnectionId(), byteDemand);
//			classDemandVid += rbDemand;
//		}
//
//		// Apply Shapley's value to find fair division of available resources to our user classes.
//		TUGame_Shapley::TUGamePlayer shapley_cbr(classDemandCbr),
//									 shapley_voip(classDemandVoip),
//									 shapley_vid(classDemandVid);
//		Shapley::Coalition<TUGame_Shapley::TUGamePlayer> players;
//		players.add(&shapley_cbr);
//		players.add(&shapley_voip);
//		players.add(&shapley_vid);
//		unsigned int numRBs = Oracle::get()->getNumRBs();
//		std::map<const TUGame_Shapley::TUGamePlayer*, double> shapleyValues = TUGame_Shapley::play(players, numRBs);
//		// Post-processing ensures that all resource blocks are distributed, and no fractions of resource blocks are set.
//		TUGame_Shapley::postProcess(players, shapleyValues, numRBs);
//
//		unsigned int totalBandsToAllocate = shapleyValues[&shapley_cbr] + shapleyValues[&shapley_voip] + shapleyValues[&shapley_vid];
//		if (totalBandsToAllocate > 0) {
//			assert(totalBandsToAllocate == numRBs);
//
//			// Estimate data rates on all RBs for all users.
//			for (User* user : users) {
//				vector<double> expectedDatarateVec;
//				for (Band band = 0; band < Oracle::get()->getNumRBs(); band++) {
//					double bytesOnBand = (double) getBytesOnBand(user->getNodeId(), band, 1, getDirection(user->getConnectionId()));
//					expectedDatarateVec.push_back(bytesOnBand);
//				}
//				user->setExpectedDatarateVec(expectedDatarateVec);
//			}
//
//			// For each user class, distribute the RBs provided by Shapley among the flows in the class according to the EXP-PF-Rule.
//			nextBandToAllocate = 0;
//
//			if (classCbr.size() > 0 && classVoip.size() > 0) {
//			cout << NOW << " LteTUGame Resource Block Distribution... " << endl;
//			cout << "\tDistributing " << shapleyValues[&shapley_cbr] << "/" << numRBs << "RBs to " << classCbr.size() << " CBR flows that require " << classDemandCbr << "... " << endl;
//			applyEXP_PF_Rule(classCbr, shapleyValues[&shapley_cbr]);
//
//			cout << "\tDistributing " << shapleyValues[&shapley_voip] << "/" << numRBs << "RBs to " << classVoip.size() << " VoIP flows that require " << classDemandVoip << "... " << endl;
//			applyEXP_PF_Rule(classVoip, shapleyValues[&shapley_voip]);
//
//			cout << "\tDistributing " << shapleyValues[&shapley_vid] << "/" << numRBs << "RBs to " << classVid.size() << " Video flows that require " << classDemandVid << "... " << endl;
//			applyEXP_PF_Rule(classVid, shapleyValues[&shapley_vid]);
//			}
//		}
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
    double d2dPenalty = 0.5;

	void applyEXP_PF_Rule(const Shapley::Coalition<User>& flowClass, unsigned int numRBs) {
		// Allocate CBR flows.
		for (unsigned int numAllocated = 0; numAllocated < numRBs; numAllocated++) {
			assert(nextBandToAllocate < Oracle::get()->getNumRBs());
			// Get metrics for all flows.
			map<const User*, double> metricsMap = ExpPfRule::calculate(flowClass.getMembers(), nextBandToAllocate, d2dPenalty);
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
			cout << "\t\t" << userWithLargestMetric->toString() << " got RB " << nextBandToAllocate << " with metric of " << largestMetric << endl;
			nextBandToAllocate++;
		}
	}
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTETUGAME_LTETUGAME_H_ */
