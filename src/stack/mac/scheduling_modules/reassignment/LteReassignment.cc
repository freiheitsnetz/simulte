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

LteReassignment::LteReassignment() {

}

LteReassignment::~LteReassignment() {

}

void LteReassignment::prepareSchedule() {
    OmniscientEntity *oracle = OmniscientEntity::get();
    if (oracle == nullptr)
        return;
    std::string schedulingDirection = oracle->getReassignmentSchedulingDirection();
    EV_STATICCONTEXT;
    EV << NOW << " LteReassignment::prepareSchedule [" << schedulingDirection << "/" << dirToA(direction_) << "] for " << activeConnectionSet_.size() << " connections." << std::endl;
    if (schedulingDirection.compare("forward") != 0 && schedulingDirection.compare("backward") != 0)
        throw std::runtime_error("LteReassignment sees invalid direction: '" + schedulingDirection + "' (only 'forward' and 'backward' work)");
    // Copy currently active connections to a working copy.
    activeConnectionTempSet_ = activeConnectionSet_;

    SchedulingMemory* memory = new SchedulingMemory();

    std::vector<MacCid> connections;
    if (schedulingDirection.compare("forward") == 0) {
        for (ActiveSet::iterator iterator = activeConnectionTempSet_.begin(); iterator != activeConnectionTempSet_.end(); iterator++)
            connections.push_back(*iterator);
    } else {
        for (ActiveSet::reverse_iterator iterator = activeConnectionTempSet_.rbegin(); iterator != activeConnectionTempSet_.rend(); iterator++)
            connections.push_back(*iterator);
    }

    // Go through all active connections.
    for (int i = 0; i < ((int) connections.size()); i++) {
        MacCid currentConnection = connections.at(i);
        MacNodeId nodeId = MacCidToNodeId(currentConnection);
        EV << NOW << " LteReassignment::prepareSchedule Considerung node " << nodeId << "." << std::endl;
        if (getBinder()->getOmnetId(nodeId) == 0) {
            EV << NOW << "LteReassignment::prepareSchedule removing node " << nodeId << " because its ID is unknown" << std::endl;
            connections.erase(connections.begin() + i);
            i--;
            continue;
        }
        // Assign band 0.
        EV << NOW << " LteReassignment::prepareSchedule Scheduling node " << nodeId << " on band 0." << std::endl;
        SchedulingResult result = schedule(currentConnection, Band(0));
        memory->put(nodeId, Band(0), (i == 0 ? false : true));
        EV << NOW << " LteReassignment::prepareSchedule Scheduled node " << nodeId << " on band 0: " << schedulingResultToString(result) << std::endl;

        if (result == SchedulingResult::INACTIVE) {
            EV << NOW << " LteReassignment::prepareSchedule removing node " << nodeId << " because it is now INACTIVE" << std::endl;
            EV << NOW << " LteReassignment::prepareSchedule BEFORE DELETE SIZE=" << connections.size() << std::endl;
            connections.erase(connections.begin() + i);
            i--;
            EV << NOW << " LteReassignment::prepareSchedule AFTER DELETE SIZE=" << connections.size() << " i=" << i << std::endl;
        }
    }

    for (ActiveSet::iterator iterator = activeConnectionTempSet_.begin(); iterator != activeConnectionTempSet_.end(); iterator++) {
        MacCid connection = *iterator;
        if (std::find(connections.begin(), connections.end(), connection) == connections.end())
            activeConnectionTempSet_.erase(connection);
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
    unsigned long bytes = 4294967295U; // 2^32
//    unsigned long bytes = 50; // 89 is max for 1 RB, take less.
    unsigned int granted = requestGrant(connectionId, bytes, terminate, active, eligible, &bandLimitVec);
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
