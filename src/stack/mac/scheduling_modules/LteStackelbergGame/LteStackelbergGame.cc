#include "stack/mac/scheduling_modules/LteStackelbergGame/LteStackelbergGame.h"
#include "stack/mac/scheduling_modules/LteStackelbergGame/src/StackelbergGame.h"
#include "stack/mac/scheduling_modules/LteStackelbergGame/src/StackelbergUser.h"
#include "common/oracle/Oracle.h"

using namespace std;

const std::string LteStackelbergGame::LEADER_SCHEDULER_RR = "RR";

LteStackelbergGame::LteStackelbergGame() {
    pair<double, double> d2dPowerLimits = Oracle::get()->stackelberg_getD2DPowerLimits();
    d2dTxPower_max = d2dPowerLimits.first;
    d2dTxPower_min = d2dPowerLimits.second;
    beta = Oracle::get()->stackelberg_getBeta();
    delta = Oracle::get()->stackelberg_getDelta();

    string leaderSchedulingDiscipline = Oracle::get()->stackelberg_getLeaderScheduler();
    if (leaderSchedulingDiscipline == LteStackelbergGame::LEADER_SCHEDULER_RR) {
        scheduler_rr = new LteNaiveRoundRobin();
        scheduleLeaders = [this](const std::set<MacCid>& connections) {
              return this->scheduler_rr->getSchedulingMap(connections);
        };
    } else
        throw invalid_argument("LteStackelbergGame can't recognize leader scheduling discipline: '" + leaderSchedulingDiscipline + "'.");

    cout << "d2dTxPower_max=" << d2dTxPower_max << " d2dTxPower_min=" << d2dTxPower_min << " beta=" << beta << " delta=" << delta << " scheduler=" << leaderSchedulingDiscipline << endl;
}

LteStackelbergGame::~LteStackelbergGame() {
    delete scheduler_rr;
}

void LteStackelbergGame::schedule(std::set<MacCid>& connections) {
    EV << NOW << " " << dirToA(direction_) << " LteStackelbergGame::schedule" << std::endl;

    // First define our leaders and followers.
    vector<StackelbergUser*> leaders, followers;
    unsigned long numRBsScheduled = 0, totalRBs = Oracle::get()->getNumRBs();
    for (User* user : getUserManager().getActiveUsers()) {
        // Keep track of cellular UEs (leaders).
        if (!user->isD2D()) {
            StackelbergUser* leader = new StackelbergUser(user->getNodeId(), user->getConnectionId());
            leader->setPartnerId(user->getPartnerId());
            leaders.push_back(leader);
        // And remember all D2D UEs as followers.
        } else {
            StackelbergUser* follower = new StackelbergUser(user->getNodeId(), user->getConnectionId());
            follower->setPartnerId(user->getPartnerId());
            followers.push_back(follower);
        }
    }

    // Schedule leaders.
    set<MacCid> leaderConnectionIds;
    for (size_t i = 0; i < leaders.size(); i++)
        leaderConnectionIds.insert(leaders.at(i)->getConnectionId());
    map<MacCid, vector<Band>> schedulingMap_leaders = scheduleLeaders(leaderConnectionIds);
    for (auto iterator = schedulingMap_leaders.begin(); iterator != schedulingMap_leaders.end(); iterator++) {
        MacCid connection = (*iterator).first;
        vector<Band> resources = (*iterator).second;
        for (Band resource : resources)
            scheduleUeReuse(connection, resource);
    }

    // Schedule followers.
    if (!followers.empty()) {
        // Set up each game's fixed parameters.
        StackelbergGame game;
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
                const vector<Band>& resources = schedulingMap_leaders[leader->getConnectionId()];
                double g_ki = Oracle::get()->getChannelGain(leader->getNodeId(), follower->getPartnerId(), resources);
                leader->setChannelGain_d2d(follower->getPartnerId(), g_ki);
            }
        }

        // Now we can play the Stackelberg games.
        map<const StackelbergUser*, const StackelbergUser*> schedulingMap_followers = game.schedule(leaders, followers);
        for (auto iterator = schedulingMap_followers.begin(); iterator != schedulingMap_followers.end(); iterator++) {
            const StackelbergUser* leader = (*iterator).first;
            const StackelbergUser* follower = (*iterator).second;
            const vector<Band>& resources = schedulingMap_leaders[leader->getConnectionId()];
//            cout << Oracle::get()->getName(leader->getNodeId()) << " shares RBs [";
            for (size_t i = 0; i < resources.size(); i++) {
                const Band& resource = resources.at(i);
//                cout << resource << (i < resources.size() - 1 ? " " : "] ");
                scheduleUeReuse(follower->getConnectionId(), resource);
            }
//            cout << "with " << Oracle::get()->getName(follower->getNodeId()) << endl;

        }
    }

}
