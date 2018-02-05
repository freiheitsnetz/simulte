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
#include <fstream>

using namespace std;

class LteTUGame : public LteSchedulerBase {
public:
	LteTUGame() : LteSchedulerBase() {
		d2dPenalty = Oracle::get()->getD2DPenalty();
	}

	virtual ~LteTUGame() {
		for (User* user : users) {
//			if (user->getNodeId() == 1027) {
//				ofstream myfile;
//				myfile.open("schedule_record", std::ios_base::app);
//				const std::vector<unsigned short>& scheduleVec = user->getScheduledVec();
//				myfile << "p=" << d2dPenalty << endl;
//				for (size_t i = 0; i < scheduleVec.size(); i++) {
//					myfile << scheduleVec.at(i) << (i < scheduleVec.size() - 1 ? ", " : "");
//				}
//				myfile << endl;
//				myfile.close();
//			}
			delete user;
		}
	}

	/**
	 * @return The user's type depending on the application it is running.
	 */
	static User::Type getUserType(const MacNodeId& nodeId) {
		string appName = Oracle::get()->getApplicationName(nodeId);
		EV << NOW << " LteTUGame::getUserType(" << appName << ")" << endl;
		if (appName == "VoIPSender" || appName == "inet::SimpleVoIPSender" || appName == "VoIPReceiver")
			return User::Type::VOIP;
		else if (appName == "inet::UDPBasicApp" || appName == "inet::UDPSink" || appName == "inet::TCPSessionApp" || appName == "inet::TCPSinkApp")
			return User::Type::CBR;
		else if (appName == "inet::UDPVideoStreamCli" || appName == "inet::UDPVideoStreamSvr")
			return User::Type::VIDEO;
		else
			throw invalid_argument("getUserType(" + appName + ") not supported.");
	}

	/**
	 * Sets user's maximum delay and bitrate for VoIP or Video Streaming applications, taken from the paper on Transferable Utility games.
	 */
	static void setRealtimeValues(User* user) {
		// Set user type.
		if (user->getType() == User::Type::VOIP) {
			user->setRealtimeTarget(50/*ms*/, 1.05/*byte per TTI*/);
			EV << NOW << " LteTUGame::setRealtimeValues(" << user->toString() << ") realtime values set to VoIP." << endl;
		} else if (user->getType() == User::Type::VIDEO) {
			user->setRealtimeTarget(100/*ms*/, 30.25/*byte per TTI*/);
			EV << NOW << " LteTUGame::setRealtimeValues(" << user->toString() << ") realtime values set to Video." << endl;
		} else
			throw invalid_argument("LteTUGame::setRealtimeValues(" + user->toString() + ") not supported.");
	}

	static void setD2D(User* user) {
		user->setD2D(Oracle::get()->isD2DFlow(user->getConnectionId()));
	}

	void updateUserAllocatedBytes(MacCid connectionId, unsigned int numBytes) {
		for (User* user: users) {
			if (user->getConnectionId() == connectionId) {
				user->updateBytesAllocated(numBytes);
				return;
			}
		}
		throw logic_error("LteTUGame::updateUserAllocatedBytes(" + to_string(connectionId) + ", " + to_string(numBytes) + ") couldn't find user in 'users' list.");
	}

	bool haveWritten = false;

    virtual void schedule(std::set<MacCid>& connections) override {
        EV << NOW << " LteTUGame::schedule" << std::endl;

        // Update player list - adds new players and updates their active status.
        EV << NOW << " LteTUGame::updatePlayers" << std::endl;
        FlowClassUpdater::updatePlayers(connections, users, LteTUGame::getUserType, LteTUGame::setRealtimeValues, LteTUGame::setD2D);

        // Update classes to contain all corresponding active players.
        EV << NOW << " LteTUGame::updateClasses" << std::endl;
        FlowClassUpdater::updateClasses(users, classCbr, classVoip, classVid);

//        if (!haveWritten && direction_ == UL) {
//        	ofstream myfile;
//			myfile.open("channel_gain", std::ios_base::app);
////			cout << Oracle::get()->getName(1025) << endl;
////			cout << "att=" << Oracle::get()->getAttenuation(Oracle::get()->getEnodeBID(), 1025) << endl;
//			myfile << Oracle::get()->getChannelGain(Oracle::get()->getEnodeBID(), 1025) << endl;
//			myfile.close();
//			haveWritten = true;
//        }

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
			unsigned int byteDemand = getByteDemand(user);
			unsigned int rbDemand = getRBDemand(user->getConnectionId(), byteDemand);
			classDemandCbr += rbDemand;
		}
		// Voice-over-IP users.
		for (const User* user : classVoip.getMembers()) {
			unsigned int byteDemand = getByteDemand(user);
			unsigned int rbDemand = getRBDemand(user->getConnectionId(), byteDemand);
			classDemandVoip += rbDemand;
		}
		// Video streaming users.
		for (const User* user : classVid.getMembers()) {
			unsigned int byteDemand = getByteDemand(user);
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
		// Post-processing ensures that all resource blocks are distributed, and no fractions of resource blocks are set.
		TUGame_Shapley::postProcess(players, shapleyValues, numRBs);

		unsigned int totalBandsToAllocate = shapleyValues[&shapley_cbr] + shapleyValues[&shapley_voip] + shapleyValues[&shapley_vid];

		if (totalBandsToAllocate > 0) {
			assert(totalBandsToAllocate == numRBs);

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
			EV << NOW << " LteTUGame " << dirToA(direction_) << " Resource Block Distribution... " << endl;
			std::map<unsigned short, const User*> allocationMap = ExpPfRule::apply(classCbr, classVoip, classVid,
					shapleyValues[&shapley_cbr], shapleyValues[&shapley_voip], shapleyValues[&shapley_vid], numRBs, d2dPenalty, std::bind(&LteTUGame::updateUserAllocatedBytes, this, std::placeholders::_1, std::placeholders::_2));

			EV << "\tDistributing " << shapleyValues[&shapley_vid] << "/" << numRBs << "RBs to " << classVid.size() << " Video flows that require " << classDemandVid << "." << endl;
			EV << "\tDistributing " << shapleyValues[&shapley_voip] << "/" << numRBs << "RBs to " << classVoip.size() << " VoIP flows that require " << classDemandVoip << "." << endl;
			EV << "\tDistributing " << shapleyValues[&shapley_cbr] << "/" << numRBs << "RBs to " << classCbr.size() << " CBR flows that require " << classDemandCbr << "." << endl;
			for (Band resource = 0; resource < numRBs; resource++) {
				scheduleUe(allocationMap[resource]->getConnectionId(), resource);
			}
		}
    }

    virtual void reactToSchedulingResult(const SchedulingResult& result, unsigned int numBytesGranted, const MacCid& connection) override {
    	for (User* user : users) {
    		if (user->getConnectionId() == connection) {
    			// Remember number of bytes served so that future metric computation takes it into account.
    			user->updateDelay(numBytesGranted);
    			break;
    		}
    	}
    }

    virtual void commitSchedule() override {
    	if (direction_ == UL) {
			for (auto iterator = schedulingDecisions.begin(); iterator != schedulingDecisions.end(); iterator++) {
				const pair<MacCid, std::vector<Band>> pair = *iterator;
				for (User* user : users) {
					if (user->getConnectionId() == pair.first) {
						user->addRBsScheduledThisTTI(pair.second.size());
					}
				}
			}

			for (User* user : users)
				user->onTTI();
    	}
    	LteSchedulerBase::commitSchedule();
    }

protected:
    std::vector<User*> users;
    Shapley::Coalition<User> classCbr, classVoip, classVid;
    double d2dPenalty = 0.5;

	unsigned int getByteDemand(const User* user) {
		switch (user->getType()) {
			case User::Type::VIDEO: {
				return 30250; // Bytes per second.
			}
			case User::Type::VOIP: {
				return 1050;
			}
			case User::Type::CBR: {
				return 250;
			}
			default: {
				throw invalid_argument("getByteDemand(" + user->toString() + ") not supported.");
			}
		}
	}
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTETUGAME_LTETUGAME_H_ */
