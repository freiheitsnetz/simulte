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
#include "stack/mac/scheduling_modules/LteTUGame/TUGame.h"

using namespace std;

class LteTUGame : public LteSchedulerBase {
public:
    virtual void schedule(std::set<MacCid>& connections) override {
        EV << NOW << " LteTUGame::schedule" << std::endl;
        // Update player list - adds new players and updates their active status.
        // Also ensures the coalitions correspond to the active TTI.
        setPlayers(connections, players, videoClass, voipClass, cbrClass);
        EV << NOW << " LteTUGame::setUserClasses" << std::endl;
		EV << "\t" << videoClass.size() << " video flows." << std::endl;
		EV << "\t" << voipClass.size() << " VoIP flows." << std::endl;
		EV << "\t" << cbrClass.size() << " CBR flows." << std::endl;
    }

    virtual ~LteTUGame() {
    	for (TUGamePlayer* player : players)
    		delete player;
    	players.clear();
    }

protected:
    Shapley::Coalition<TUGamePlayer> videoClass, voipClass, cbrClass;
    double videoDemand = 242.0, voipDemand = 8.4, cbrDemand = 2;
    std::string videoAppName = "video", voipAppName = "voip", cbrAppName = "inet::UDPBasicApp";
    std::vector<TUGamePlayer*> players;

    /**
     * Goes through 'connections' to determine which players are active this TTI.
     * Keeps 'players' and the classes up to date.
     */
    void setPlayers(const std::set<MacCid>& connections, std::vector<TUGamePlayer*>& players,
    		Shapley::Coalition<TUGamePlayer>& videoClass, Shapley::Coalition<TUGamePlayer>& voipClass, Shapley::Coalition<TUGamePlayer>& cbrClass) {
    	EV << NOW << " LteTUGame::setPlayers" << std::endl;
    	// Clear all containers.
		videoClass.clear();
		voipClass.clear();
		cbrClass.clear();

    	// Assume everyone is inactive.
    	for (TUGamePlayer* player : players)
    		player->setActive(false);
    	// Go through all current connections...
    	for (const MacCid& connection : connections) {
    		MacNodeId id = MacCidToNodeId(connection);
    		// ... check if already in player list?
    		bool found = false;
    		for (size_t i = 0; !found && i < players.size(); i++) {
    			TUGamePlayer* player = players.at(i);
    			// Then set to active.
    			if (player->getNodeId() == id) {
    				found = true;
    				player->setActive(true);
    				// And add it to the corresponding coalition.
					if (player->getType() == TUGamePlayer::PlayerType::CBR)
						cbrClass.add(player);
					else if (player->getType() == TUGamePlayer::PlayerType::VOIP)
						voipClass.add(player);
					else if (player->getType() == TUGamePlayer::PlayerType::VIDEO)
						videoClass.add(player);
					else
						throw std::runtime_error("LteTUGame::setUserClasses has a player with unknown type: " + TUGamePlayer::typeToString(player->getType()));
    				EV << NOW << "\t" << TUGamePlayer::typeToString(player->getType()) << " player '" << player->getName() << " is active this TTI." << std::endl;
    			}
    		}
    		// Otherwise add it to the player list.
    		if (!found) {
    			std::string appName = Oracle::get()->getApplicationName(id);
    			// Constant Bitrate.
    			if (appName.compare(cbrAppName) == 0) {
    				TUGamePlayer* player = new TUGamePlayer(cbrDemand, connection, id);
    				player->setName(Oracle::get()->getName(id));
    				player->setType(TUGamePlayer::PlayerType::CBR);
    				players.push_back(player);
    				cbrClass.add(player);
    				EV << NOW << "\t added " << TUGamePlayer::typeToString(player->getType()) << " player '" << player->getName() << "'." << std::endl;
				// Voice over IP.
    			} else if (appName.compare(voipAppName) == 0) {
    				TUGamePlayer* player = new TUGamePlayer(voipDemand, connection, id);
    				player->setName(Oracle::get()->getName(id));
    				player->setType(TUGamePlayer::PlayerType::VOIP);
					players.push_back(player);
					voipClass.add(player);
					EV << NOW << "\t added " << TUGamePlayer::typeToString(player->getType()) << " player '" << player->getName() << "'." << std::endl;
				// Video Streaming.
    			} else if (appName.compare(videoAppName) == 0) {
    				TUGamePlayer* player = new TUGamePlayer(videoDemand, connection, id);
    				player->setName(Oracle::get()->getName(id));
    				player->setType(TUGamePlayer::PlayerType::VIDEO);
					players.push_back(player);
					videoClass.add(player);
					EV << NOW << "\t added " << TUGamePlayer::typeToString(player->getType()) << " player '" << player->getName() << "'." << std::endl;
    			} else
    				throw std::runtime_error("LteTUGame::setPlayers couldn't recognize " + Oracle::get()->getName(id) + "'s application type: " + appName);
    		}
    	}
    }

};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTETUGAME_LTETUGAME_H_ */
