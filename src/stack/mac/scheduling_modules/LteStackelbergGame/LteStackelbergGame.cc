#include "stack/mac/scheduling_modules/LteStackelbergGame/LteStackelbergGame.h"

using namespace std;

LteStackelbergGame::LteStackelbergGame() {
    pair<double, double> d2dPowerLimits = Oracle::get()->stackelberg_getD2DPowerLimits();
    d2dTxPower_max = d2dPowerLimits.first;
    d2dTxPower_min = d2dPowerLimits.second;
    beta = Oracle::get()->stackelberg_getBeta();
    delta = Oracle::get()->stackelberg_getDelta();

    cout << "d2dTxPower_max=" << d2dTxPower_max << " d2dTxPower_min=" << d2dTxPower_min << " beta=" << beta << " delta=" << delta << endl;
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
