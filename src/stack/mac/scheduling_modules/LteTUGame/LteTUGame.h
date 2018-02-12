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
#include "stack/mac/scheduling_modules/LteTUGame/src/TUGameUser.h"
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
		for (TUGameUser* user : users) {
			delete user;
		}
	}

	/**
	 * @return The user's type depending on the application it is running.
	 */
	static TUGameUser::Type getUserType(const MacNodeId& nodeId) {
		string appName = Oracle::get()->getApplicationName(nodeId);
		EV << NOW << " LteTUGame::getUserType(" << appName << ")" << endl;
		if (appName == "VoIPSender" || appName == "inet::SimpleVoIPSender" || appName == "VoIPReceiver")
			return TUGameUser::Type::VOIP;
		else if (appName == "inet::UDPBasicApp" || appName == "inet::UDPSink" || appName == "inet::TCPSessionApp" || appName == "inet::TCPSinkApp")
			return TUGameUser::Type::CBR;
		else if (appName == "inet::UDPVideoStreamCli" || appName == "inet::UDPVideoStreamSvr")
			return TUGameUser::Type::VIDEO;
		else
			throw invalid_argument("getUserType(" + appName + ") not supported.");
	}

	/**
	 * Sets user's maximum delay and bitrate for VoIP or Video Streaming applications, taken from the paper on Transferable Utility games.
	 */
	static void setRealtimeValues(TUGameUser* user) {
		// Set user type.
		if (user->getType() == TUGameUser::Type::VOIP) {
			user->setRealtimeTarget(user->getDelayTarget() /*ms*/, user->getByteDemand() / 1000 /*byte per TTI*/);
			EV << NOW << " LteTUGame::setRealtimeValues(" << user->toString() << ") realtime values set to VoIP." << endl;
		} else if (user->getType() == TUGameUser::Type::VIDEO) {
			user->setRealtimeTarget(user->getDelayTarget() /*ms*/, user->getByteDemand() / 1000 /*byte per TTI*/);
			EV << NOW << " LteTUGame::setRealtimeValues(" << user->toString() << ") realtime values set to Video." << endl;
		} else
			throw invalid_argument("LteTUGame::setRealtimeValues(" + user->toString() + ") not supported.");
	}

	static void setD2D(TUGameUser* user) {
		user->setD2D(Oracle::get()->isD2DFlow(user->getConnectionId()));
	}

	void updateUserAllocatedBytes(MacCid connectionId, unsigned int numBytes) {
		for (TUGameUser* user: users) {
			if (user->getConnectionId() == connectionId) {
				user->updateBytesAllocated(numBytes);
				return;
			}
		}
		throw logic_error("LteTUGame::updateUserAllocatedBytes(" + to_string(connectionId) + ", " + to_string(numBytes) + ") couldn't find user in 'users' list.");
	}

    virtual void schedule(std::set<MacCid>& connections) override {
        EV << NOW << " LteTUGame::schedule" << std::endl;

        std::map<unsigned short, const TUGameUser*> allocationMap = getSchedulingMap(connections);

        if (!allocationMap.empty()) {
            for (Band resource = 0; resource < Oracle::get()->getNumRBs(); resource++) {
                scheduleUe(allocationMap[resource]->getConnectionId(), resource);
            }
        }
    }

    virtual void commitSchedule() override {
		for (TUGameUser* user : users)
			user->onTTI();

    	LteSchedulerBase::commitSchedule();
    }

    std::map<unsigned short, const TUGameUser*> getSchedulingMap(const std::set<MacCid>& connections) {
        // Update player list - adds new players and updates their active status.
        EV << NOW << " LteTUGame::updatePlayers" << std::endl;
        FlowClassUpdater::updatePlayers(connections, users, LteTUGame::getUserType, LteTUGame::setRealtimeValues, LteTUGame::setD2D);

        // Update classes to contain all corresponding active players.
        EV << NOW << " LteTUGame::updateClasses" << std::endl;
        FlowClassUpdater::updateClasses(users, classCbr, classVoip, classVid);

        // Print status.
        EV << "\t" << classVid.size() << " video flows:\n\t";
        for (const TUGameUser* user : classVid.getMembers())
            EV << user->toString() << " ";
        EV << endl;
        EV << "\t" << classVoip.size() << " VoIP flows:\n\t";
        for (const TUGameUser* user : classVoip.getMembers())
            EV << user->toString() << " ";
        EV << endl;
        EV << "\t" << classCbr.size() << " CBR flows:\n\t";
        for (const TUGameUser* user : classCbr.getMembers())
            EV << user->toString() << " ";
        EV << endl;

        /** Demand in resource blocks.*/
        double classDemandCbr = 0,
               classDemandVoip = 0,
               classDemandVid = 0;
        // Constant Bitrate users.
        for (const TUGameUser* user : classCbr.getMembers()) {
            unsigned int byteDemand = user->getByteDemand() / 1000; // /1000 to convert from s to ms resolution.
            double rbDemand = getRBDemand(user->getConnectionId(), byteDemand);
            classDemandCbr += rbDemand;
        }
        // Voice-over-IP users.
        for (const TUGameUser* user : classVoip.getMembers()) {
            unsigned int byteDemand = user->getByteDemand() / 1000;
            double rbDemand = getRBDemand(user->getConnectionId(), byteDemand);
            classDemandVoip += rbDemand;
        }
        // Video streaming users.
        for (const TUGameUser* user : classVid.getMembers()) {
            unsigned int byteDemand = user->getByteDemand() / 1000;
            double rbDemand = getRBDemand(user->getConnectionId(), byteDemand);
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
        std::map<unsigned short, const TUGameUser*> allocationMap;

        if (totalBandsToAllocate > 0) {
            assert(totalBandsToAllocate == numRBs);

            // Estimate data rates on all RBs for all users.
            for (TUGameUser* user : users) {
                vector<double> expectedDatarateVec;
                for (Band band = 0; band < Oracle::get()->getNumRBs(); band++) {
                    double bytesOnBand = (double) getBytesOnBand(user->getNodeId(), band, 1, getDirection(user->getConnectionId()));
                    expectedDatarateVec.push_back(bytesOnBand);
                }
                user->setExpectedDatarateVec(expectedDatarateVec);
            }

            // For each user class, distribute the RBs provided by Shapley among the flows in the class according to the EXP-PF-Rule.
            EV << NOW << " LteTUGame " << dirToA(direction_) << " Resource Block Distribution... " << endl;
            allocationMap = ExpPfRule::apply(classCbr, classVoip, classVid,
                    shapleyValues[&shapley_cbr], shapleyValues[&shapley_voip], shapleyValues[&shapley_vid], numRBs, d2dPenalty, std::bind(&LteTUGame::updateUserAllocatedBytes, this, std::placeholders::_1, std::placeholders::_2));

            EV << "\tDistributing " << shapleyValues[&shapley_vid] << "/" << numRBs << "RBs to " << classVid.size() << " Video flows that require " << classDemandVid << "." << endl;
            EV << "\tDistributing " << shapleyValues[&shapley_voip] << "/" << numRBs << "RBs to " << classVoip.size() << " VoIP flows that require " << classDemandVoip << "." << endl;
            EV << "\tDistributing " << shapleyValues[&shapley_cbr] << "/" << numRBs << "RBs to " << classCbr.size() << " CBR flows that require " << classDemandCbr << "." << endl;
        }

        return allocationMap;
    }

protected:
    std::vector<TUGameUser*> users;
    Shapley::Coalition<TUGameUser> classCbr, classVoip, classVid;
    /** D2D flow metrics are multiplied by this value. So 1.0 turns off penalty application, while 0.0 disables scheduling to D2D flows. */
    double d2dPenalty = 1.0;
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTETUGAME_LTETUGAME_H_ */
