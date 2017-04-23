//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include <reassignment/LteReassignment.h>

LteReassignment::LteReassignment() {

}

LteReassignment::~LteReassignment() {

}

void LteReassignment::prepareSchedule() {
    EV_STATICCONTEXT;
    EV << NOW << " LteReassignment::prepareSchedule" << std::endl;
    // Copy currently active connections to a working copy.
    activeConnectionTempSet_ = activeConnectionSet_;

    // Go through all active connections.
    for (ActiveSet::iterator iterator = activeConnectionTempSet_.begin(); iterator != activeConnectionTempSet_.end (); ++iterator) {
        MacCid currentConnection = *iterator;
        MacNodeId nodeId = MacCidToNodeId(currentConnection);
        // Assign band 0.
        SchedulingResult result = schedule(currentConnection, Band(0));
        EV << NOW << " LteReassignment::prepareSchedule Scheduling node " << nodeId << " on band 0: " << schedulingResultToString(result) << std::endl;
    }
}

void LteReassignment::commitSchedule() {
    EV_STATICCONTEXT;
    EV << NOW << " LteReassignment::commitSchedule" << std::endl;
}

LteReassignment::SchedulingResult LteReassignment::schedule(MacCid connectionId, Band band) {
    bool terminate = false;
    bool active = true;
    bool eligible = true;

    std::vector<BandLimit> bandLimitVec;
    BandLimit bandLimit(band);
    bandLimitVec.push_back(bandLimit);

    // requestGrant(...) might alter the three bool values, so we can check them afterwards.
    unsigned int granted = requestGrant(connectionId, 4294967295U, terminate, active, eligible, &bandLimitVec);
    EV << " " << granted << " bytes granted." << std::endl;
    if (terminate)
        return LteReassignment::SchedulingResult::TERMINATE;
    else if (!active)
        return LteReassignment::SchedulingResult::INACTIVE;
    else if (!eligible)
        return LteReassignment::SchedulingResult::INELIGIBLE;
    else
        return LteReassignment::SchedulingResult::OK;
}
