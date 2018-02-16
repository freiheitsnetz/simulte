#include "stack/mac/scheduling_modules/LteStackelbergGame/LteStackelbergGame.h"
#include "stack/mac/scheduling_modules/LteStackelbergGame/src/StackelbergGame.h"
#include "stack/mac/scheduling_modules/LteStackelbergGame/src/StackelbergUser.h"
#include "common/oracle/Oracle.h"

using namespace std;

/** Schedule leader UEs with Round Robin. */
const std::string LteStackelbergGame::LEADER_SCHEDULER_RR = "RR";
/** Schedule leader UEs with Transferable Utility. */
const std::string LteStackelbergGame::LEADER_SCHEDULER_TU = "TU";
/** Schedule follower UEs with Stackelberg. */
const std::string LteStackelbergGame::FOLLOWER_SCHEDULER_STA = "STA";
/** Schedule follower UEs randomly. */
const std::string LteStackelbergGame::FOLLOWER_SCHEDULER_RAND = "RAND";

LteStackelbergGame::LteStackelbergGame() {
    pair<double, double> d2dPowerLimits = Oracle::get()->stackelberg_getD2DPowerLimits();
    d2dTxPower_max = d2dPowerLimits.first;
    d2dTxPower_min = d2dPowerLimits.second;
    beta = Oracle::get()->stackelberg_getBeta();
    delta = Oracle::get()->stackelberg_getDelta();
    shouldSetTxPower = Oracle::get()->stackelberg_shouldSetTxPower();
    scheduleFollowersRandomly = Oracle::get()->stackelberg_getFollowerScheduler() == FOLLOWER_SCHEDULER_RAND;

    string leaderSchedulingDiscipline = Oracle::get()->stackelberg_getLeaderScheduler();
    if (leaderSchedulingDiscipline == LteStackelbergGame::LEADER_SCHEDULER_RR) {
        scheduler_rr = new LteNaiveRoundRobin();
        scheduleLeaders = [this](const std::set<MacCid>& connections) {
              return this->scheduler_rr->getSchedulingMap(connections);
        };
    } else if (leaderSchedulingDiscipline == LteStackelbergGame::LEADER_SCHEDULER_TU) {
        scheduler_tu = new LteTUGame();
        scheduler_tu->setD2DPenalty(Oracle::get()->getD2DPenalty());
        scheduler_tu->getRBsRequired = [&](const MacCid& connection, const unsigned int& numBytes) {return getRBDemand(connection, numBytes);};
        scheduler_tu->getBytesOnBandFunc = [&](const MacNodeId& nodeId, const Band& band, const unsigned int& numBlocks, const Direction& dir) {return getBytesOnBand(nodeId, band, numBlocks, dir);};
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
    } else
        throw invalid_argument("LteStackelbergGame can't recognize leader scheduling discipline: '" + leaderSchedulingDiscipline + "'.");

    if (scheduleFollowersRandomly)
    	scheduler_rr_reuse = new LteNaiveRoundRobinReuse();

    cout << "d2dTxPower_max=" << d2dTxPower_max << " d2dTxPower_min=" << d2dTxPower_min << " beta=" << beta
    		<< " delta=" << delta << " leader_scheduler=" << leaderSchedulingDiscipline << " follower_scheduler="
			<< (scheduleFollowersRandomly ? "randomly" : "Stackelberg") << endl;
}

LteStackelbergGame::~LteStackelbergGame() {
    delete scheduler_rr;
    delete scheduler_tu;
    delete scheduler_rr_reuse;
}

void LteStackelbergGame::schedule(std::set<MacCid>& connections) {
    EV << NOW << " " << dirToA(direction_) << " LteStackelbergGame::schedule" << std::endl;
    assert(direction_ != Direction::DL && "Stackelberg Game only works for uplink!");

    if (scheduler_tu != nullptr) {
        if (scheduler_tu->getEnbSchedulerPtr() == nullptr)
            scheduler_tu->setEnbSchedulerPtr(eNbScheduler_);
        scheduler_tu->setDirection(direction_);
    }

    // First define our leaders and followers.
    vector<StackelbergUser*> leaders, followers;
    for (User* user : getUserManager().getActiveUsers()) {
    	StackelbergUser* stackeluser = new StackelbergUser(*user);
    	if (shouldSetTxPower) {
    		stackeluser->setTxPower(defaultTxPower);
    		Oracle::get()->setUETxPower(stackeluser->getNodeId(), stackeluser->isD2D(), defaultTxPower);
    	}

        // Keep track of cellular UEs (leaders).
        if (!user->isD2D()) {
            leaders.push_back(stackeluser);
        // And remember all D2D UEs as followers.
        } else {
            followers.push_back(stackeluser);
        }
    }

//    cout << NOW << " " << dirToA(direction_) << " Leaders: ";
//    for (const StackelbergUser* leader : leaders)
//    	cout << Oracle::get()->getName(leader->getNodeId()) << " ";
//    cout << endl << NOW << " " << dirToA(direction_) << " Followers: ";
//    for (const StackelbergUser* follower : followers)
//    	cout << Oracle::get()->getName(follower->getNodeId()) << " ";
//    cout << endl;

    // Schedule leaders.
    if (!leaders.empty()) {
		set<MacCid> leaderConnectionIds;
		for (size_t i = 0; i < leaders.size(); i++)
			leaderConnectionIds.insert(leaders.at(i)->getConnectionId());

//		cout << NOW << " Scheduling " << leaderConnectionIds.size() << " leaders... ";
//		cout.flush();
		map<MacCid, vector<Band>> schedulingMap_leaders = scheduleLeaders(leaderConnectionIds);
//		cout << "done." << endl;
		for (auto iterator = schedulingMap_leaders.begin(); iterator != schedulingMap_leaders.end(); iterator++) {
			MacCid connection = (*iterator).first;
			vector<Band> resources = (*iterator).second;
//			cout << "Leader " << Oracle::get()->getName(MacCidToNodeId(connection)) << " -RBs> ";
			for (Band resource : resources) {
//				cout << (int) resource << " ";
				scheduleUeReuse(connection, resource);
			}
//			cout << endl;
		}

		// Schedule followers.
		if (!followers.empty()) {
			if (!scheduleFollowersRandomly) {
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
					if (!resources.empty()) {
						if (shouldSetTxPower) {
							double txPower_linear = follower->getTxPower();
							double txPower_dBm = linearToDBm(txPower_linear / 1000);
							Oracle::get()->setUETxPower(follower->getNodeId(), follower->isD2D(), txPower_dBm);
						}
//						cout << Oracle::get()->getName(leader->getNodeId()) << " shares RBs [";
						for (size_t i = 0; i < resources.size(); i++) {
							const Band& resource = resources.at(i);
//							cout << resource << (i < resources.size() - 1 ? " " : "] ");
							scheduleUeReuse(follower->getConnectionId(), resource);
						}
//						cout << "with " << Oracle::get()->getName(follower->getNodeId()) << endl;
					}
				}
			// Random follower scheduling.
			} else {
				set<MacCid> followerConnectionIds;
				for (size_t i = 0; i < followers.size(); i++)
					followerConnectionIds.insert(followers.at(i)->getConnectionId());

				std::map<MacCid, std::vector<Band>> schedulingMap_followers = scheduler_rr_reuse->getSchedulingMap_reuse(followerConnectionIds, schedulingMap_leaders);
				for (auto it = schedulingMap_followers.begin(); it != schedulingMap_followers.end(); it++) {
					const MacCid& connection = (*it).first;
					const vector<Band>& resources = (*it).second;
					if (!resources.empty()) {
//						cout << "Follower " << Oracle::get()->getName(MacCidToNodeId(connection)) << " (randomly) gets RBs ";
						for (const Band& resource : resources) {
//							cout << resource << " ";
							scheduleUeReuse(connection, resource);
						}
//						cout << endl;
					}
				}
			}
		}
    }

    for (size_t i = 0; i < leaders.size(); i++)
        delete leaders.at(i);
    leaders.clear();
    for (size_t i = 0; i < followers.size(); i++)
        delete followers.at(i);
    followers.clear();

}
