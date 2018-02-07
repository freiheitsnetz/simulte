#include "stack/mac/scheduling_modules/LteStackelbergGame/LteStackelbergGame.h"

using namespace std;

LteStackelbergGame::LteStackelbergGame() {
//    setUserFilter([](User* user) {
//        string appName = Oracle::get()->getApplicationName(user->getNodeId());
//        // Cellular UDPSink UEs occasionally want to send data, which they shouldn't. Filter them out.
//        if (appName == "inet::UDPSink")
//            return true;
//        else
//            return false;
//    });
}

void LteStackelbergGame::schedule(std::set<MacCid>& connections) {
    EV << NOW << " LteStackelbergGame::schedule" << std::endl;


    vector<User*> leaders, followers;
    unsigned long numRBsScheduled = 0, totalRBs = Oracle::get()->getNumRBs();
    for (User* user : getUserManager().getActiveUsers()) {
        // Simply assign RBs sequentially to cellular UEs.
        if (!user->isD2D()) {
            if (numRBsScheduled < totalRBs) {
                scheduleUeReuse(user->getConnectionId(), Band(numRBsScheduled++));
                leaders.push_back(user);
            }
        // And remember all D2D UEs as followers.
        } else {
            followers.push_back(user);
        }
    }

    unsigned long numRBsReused = 0;
    for (User* follower : followers) {
        if (numRBsReused < totalRBs)
            scheduleUeReuse(follower->getConnectionId(), Band(numRBsReused++));
    }
}
