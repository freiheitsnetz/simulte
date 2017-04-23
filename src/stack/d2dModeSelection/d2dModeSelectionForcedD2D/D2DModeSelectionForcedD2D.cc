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

#include <d2dModeSelectionForcedD2D/D2DModeSelectionForcedD2D.h>

Define_Module(D2DModeSelectionForcedD2D);

void D2DModeSelectionForcedD2D::initialize(int stage) {
    D2DModeSelectionBase::initialize(stage);
    EV << NOW << " D2DModeSelectionForcedD2D::initialize" << std::endl;
    mOracle = OmniscientEntity::get();
    EV << NOW << "\t" << (mOracle == nullptr ? "Couldn't find oracle." : "Found oracle.") << std::endl;
    if (mOracle == nullptr)
        throw cRuntimeError("D2DModeSelectionForcedD2D couldn't find the oracle.");
    // Make <from, <to, mode>> map visible to OmniscientEntity.
    mOracle->setModeSelectionMap(peeringModeMap_);
}

void D2DModeSelectionForcedD2D::doModeSelection() {
    EV << NOW << " D2DModeSelectionForcedD2D::doModeSelection for " << peeringModeMap_->size() << " nodes." << endl;

    // The switch list will contain entries of devices whose mode switch.
    // Clear it to start.
    switchList_.clear();
    // Go through all devices.
    std::map<MacNodeId, std::map<MacNodeId, LteD2DMode>>::iterator peeringMapIterator = peeringModeMap_->begin();
    for (; peeringMapIterator != peeringModeMap_->end(); peeringMapIterator++) {
        // Grab source node.
        MacNodeId srcId = peeringMapIterator->first;

        EV << NOW << "\tChecking for starting node " << srcId << std::endl;
        // Make sure node is in this cell.
        if (binder_->getNextHop(srcId) != mac_->getMacCellId()) {
            EV << NOW << "\t\tskipping. It has left the cell." << std::endl;
            continue;
        }

        // Grab handle to <dest, mode> map.
        std::map<MacNodeId, LteD2DMode>::iterator destModeMapIterator = peeringMapIterator->second.begin();
        for (; destModeMapIterator != peeringMapIterator->second.end(); destModeMapIterator++) {
            // Grab destination.
            MacNodeId dstId = destModeMapIterator->first;

            EV << NOW << "\t\t-> " << dstId << " ";

            // Make sure node is in this cell.
            if (binder_->getNextHop(dstId) != mac_->getMacCellId()) {
                EV << "has left the cell." << std::endl;
                continue;
            }

            // Skip nodes that are performing handover.
            if (binder_->hasUeHandoverTriggered(dstId) || binder_->hasUeHandoverTriggered(srcId)) {
                EV << "is performing handover." << std::endl;
                continue;
            }

            EV << std::endl;

            LteD2DMode oldMode = destModeMapIterator->second;
            // New mode is always DM.
            LteD2DMode newMode = DM; // DM = Direct Mode
            EV << NOW << " D2DModeSelectionForcedD2D::doModeSelection Node " << srcId << " will communicate with node " << dstId << " directly." << std::endl;

            if (newMode != oldMode) {
                // Mark this flow as switching modes.
                FlowId p(srcId, dstId);
                FlowModeInfo info;
                info.flow = p;
                info.oldMode = oldMode;
                info.newMode = newMode;
                switchList_.push_back(info);
                // Update peering map.
                destModeMapIterator->second = newMode;
                EV << NOW << "\tFlow: " << srcId << " --> " << dstId << " switches to " << d2dModeToA(newMode) << " mode." << endl;
            }
        }
    }
}
