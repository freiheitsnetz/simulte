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
#include <SchedulingMemory.h>
#include <OmniscientEntity.h>

LteReassignment::LteReassignment() {

}

LteReassignment::~LteReassignment() {

}

void LteReassignment::prepareSchedule() {
    EV_STATICCONTEXT;
    EV << NOW << " LteReassignment::prepareSchedule" << std::endl;
    // Copy currently active connections to a working copy.
    activeConnectionTempSet_ = activeConnectionSet_;

    SchedulingMemory* memory = new SchedulingMemory();

    // Go through all active connections.
    int counter = 0;
    for (ActiveSet::iterator iterator = activeConnectionTempSet_.begin(); iterator != activeConnectionTempSet_.end(); iterator++) {
        MacCid currentConnection = *iterator;
        MacNodeId nodeId = MacCidToNodeId(currentConnection);
        if (getBinder()->getOmnetId(nodeId) == 0) {
            EV << NOW << "LteReassignment::prepareSchedule removing node " << nodeId << " because it's ID is unknown" << std::endl;
            activeConnectionTempSet_.erase(currentConnection);
            continue;
        }
        // Assign band 0.
        SchedulingResult result = schedule(currentConnection, Band(0));
        memory->put(nodeId, Band(0), (counter == 0 ? false : true));
        EV << NOW << " LteReassignment::prepareSchedule Scheduling node " << nodeId << " on band 0: " << schedulingResultToString(result) << std::endl;

        if (result == SchedulingResult::INACTIVE) {
            EV << NOW << "LteReassignment::prepareSchedule removing node " << nodeId << " because it is now INACTIVE" << std::endl;
            activeConnectionTempSet_.erase(currentConnection);
        }
        counter++;
    }
    if (direction_ == UL)
        OmniscientEntity::get()->recordSchedulingRound(*memory);
    delete memory;
}

void LteReassignment::commitSchedule() {
    EV_STATICCONTEXT;
    EV << NOW << " LteReassignment::commitSchedule" << std::endl;
    activeConnectionSet_ = activeConnectionTempSet_;
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
