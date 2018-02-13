/*
 * LteModdedStackelebrgGame.cc
 *
 *  Created on: Feb 13, 2018
 *      Author: seba
 */

#include "stack/mac/scheduling_modules/LteStackelbergGame/LteModdedStackelbergGame.h"
#include "stack/mac/scheduling_modules/LteStackelbergGame/src/ModifiedStackelbergGame.h"
#include "stack/mac/scheduling_modules/LteStackelbergGame/src/StackelbergUser.h"

LteModdedStackelbergGame::LteModdedStackelbergGame() {
    pair<double, double> d2dPowerLimits = Oracle::get()->stackelberg_getD2DPowerLimits();
    d2dTxPower_max = d2dPowerLimits.first;
    d2dTxPower_min = d2dPowerLimits.second;
    beta = Oracle::get()->stackelberg_getBeta();
    delta = Oracle::get()->stackelberg_getDelta();

    // Only supports TU game for leader scheduling.
    scheduler_tu = new LteTUGame();
    scheduler_tu->setD2DPenalty(Oracle::get()->getD2DPenalty());
    scheduleLeaders = [this](const std::set<MacCid>& connections) {
        std::map<unsigned short, const TUGameUser*> map = this->scheduler_tu->getSchedulingMap(connections);
        std::map<MacCid, std::vector<Band>> convertedMap;
        if (!map.empty()) {
            for (Band rb = 0; rb < Oracle::get()->getNumRBs(); rb++) {
                convertedMap[map[rb]->getConnectionId()].push_back(rb);
            }
        }
        return convertedMap;
    };
}

LteModdedStackelbergGame::~LteModdedStackelbergGame() {
    delete scheduler_tu;
    scheduler_tu = nullptr; // This is important because LteStackelberg's destructor will delete this again.
}

void LteModdedStackelbergGame::schedule(std::set<MacCid>& connections) {
    EV << NOW << " " << dirToA(direction_) << " LteModdedStackelbergGame::schedule" << std::endl;
    assert(direction_ != Direction::DL && "Stackelberg Game only works for uplink!");

    if (scheduler_tu->getEnbSchedulerPtr() == nullptr)
        scheduler_tu->setEnbSchedulerPtr(eNbScheduler_);

    // Let TU game schedule all connections.
    map<MacCid, vector<Band>> schedulingMap_tu = scheduleLeaders(connections);

    // Followers are those that didn't receive any resources.
    vector<StackelbergUser*> leaders, followers;
    for (const User* user : getUserManager().getActiveUsers()) {
        const MacCid& connection = user->getConnectionId();
        // Not present in scheduling map? Then it's a follower.
        if (schedulingMap_tu.find(connection) == schedulingMap_tu.end()) {
            followers.push_back(new StackelbergUser(*user));
        } else {
            const vector<Band>& resources = schedulingMap_tu.at(connection);
            // Didn't get any resources? Follower.
            if (resources.empty()) {
                followers.push_back(new StackelbergUser(*user));
            } else { // Did get resources -> leader.
                leaders.push_back(new StackelbergUser(*user));
            }
        }
    }

    // Schedule followers.
    if (!followers.empty()) {
        // Set up each game's fixed parameters.
        ModifiedStackelbergGame game(getChannelGain);
        game.setBeta(beta);
        game.setDelta(delta);
        game.setTxPowerLimits(d2dTxPower_max, d2dTxPower_min);

        // Find all required channel gains.
        for (StackelbergUser* leader : leaders) {
            double g_ke = Oracle::get()->getChannelGain(leader->getNodeId(), Oracle::get()->getEnodeBID());
            leader->setChannelGain_enb(g_ke);
        }
        for (StackelbergUser* follower : followers) {
            double g_ii = Oracle::get()->getChannelGain(follower->getNodeId(), follower->getPartnerId());
            follower->setChannelGain_d2d(follower->getPartnerId(), g_ii);
            double g_ie = Oracle::get()->getChannelGain(follower->getNodeId(), Oracle::get()->getEnodeBID());
            follower->setChannelGain_enb(g_ie);
            for (StackelbergUser* leader : leaders) {
                const vector<Band>& resources = schedulingMap_tu[leader->getConnectionId()];
                double g_ki = Oracle::get()->getChannelGain(leader->getNodeId(), follower->getPartnerId(), resources);
                leader->setChannelGain_d2d(follower->getPartnerId(), g_ki);
            }
        }

        // Now we can play the Stackelberg games.
        map<const StackelbergUser*, const StackelbergUser*> schedulingMap_followers = game.schedule(leaders, followers);
        for (auto iterator = schedulingMap_followers.begin(); iterator != schedulingMap_followers.end(); iterator++) {
            const StackelbergUser* leader = (*iterator).first;
            const StackelbergUser* follower = (*iterator).second;
            const vector<Band>& resources = schedulingMap_tu[leader->getConnectionId()];
//            cout << Oracle::get()->getName(leader->getNodeId()) << " shares RBs [";
            for (size_t i = 0; i < resources.size(); i++) {
                const Band& resource = resources.at(i);
//                cout << resource << (i < resources.size() - 1 ? " " : "] ");
                scheduleUeReuse(follower->getConnectionId(), resource);
            }
//            cout << "with " << Oracle::get()->getName(follower->getNodeId()) << endl;

        }
    }

    for (size_t i = 0; i < leaders.size(); i++)
        delete leaders.at(i);
    leaders.clear();
    for (size_t i = 0; i < followers.size(); i++)
        delete followers.at(i);
    followers.clear();
}
