/*
 * LteMaxDatarate.cc
 *
 *  Created on: Jul 26, 2017
 *      Author: kunterbunt
 */

#include <stack/mac/scheduling_modules/maxDatarate/LteMaxDatarate.h>
#include <stack/mac/scheduler/LteSchedulerEnb.h>
#include <algorithm>

LteMaxDatarate::LteMaxDatarate() {
    mOracle = Oracle::get();
    EV_STATICCONTEXT;
    EV << "LteMaxDatarate::constructor" << std::endl;
    EV << "\t" << (mOracle == nullptr ? "Couldn't find oracle." : "Found oracle.") << std::endl;
    if (mOracle == nullptr)
        throw cRuntimeError("LteMaxDatarate couldn't find the oracle.");
}

LteMaxDatarate::~LteMaxDatarate() {}

void LteMaxDatarate::prepareSchedule() {
    EV_STATICCONTEXT;
    EV << NOW << " LteMaxDatarate::prepareSchedule for " << (direction_ == DL ? "DOWNLINK" : "UPLINK") << " for " << activeConnectionSet_.size() << " connections." << std::endl;

    // Copy currently active connections to a working copy.
    activeConnectionTempSet_ = activeConnectionSet_;

    // We need to remember for each node in which direction it wants to transmit and which bands are assigned to it.
    // For that, this pointer is passed around the various functions, and updated by each one accordingly.
    memory = new SchedulingMemory();

    // For all connections, we want all resource blocks sorted by their estimated maximum datarate.
    // The sorter holds each connection's info: from, to, direction, datarate and keeps entries
    // sorted by datarate in descending order.
    MaxDatarateSorter* sorter = sortBandsByDatarate(memory);

    // 'nullptr' is returned if the oracle is not ready yet.
    // In this case we can't do scheduling yet.
    if (sorter == nullptr) {
      return;
      EV << NOW << " LteMaxDatarate::prepareSchedule stopping because oracle is not ready yet." << std::endl;
    }

    // We now have all <band, id> combinations sorted by expected datarate.
    EV << NOW << " LteMaxDatarate::prepareSchedule bands sorted according to their expected throughputs: " << std::endl;
    EV << sorter->toString(std::string(NOW.str() + " LteMaxDatarate::prepareSchedule ")) << std::endl;

    // Now initiate phase 1 of the algorithm:
    // Go through all bands
    //   assign the band to the best candidate throughput-wise
    //   check for consecutive bands if the same candidate is also the best for this band,
    //   even if it transmits at halved power.
    // Initiate PHASE 1 of the algorithm:
    //  first assign bands to cellular users sorted by their expected datarate
    //  each time check if a consecutive band should also be assigned to the best
    //  candidate in the previous band, if it has the best datarate even at halved transmit power.
    std::vector<Band> assignedBands;
    assignedBands = phase1_cellular(sorter, memory);

    if (direction_ == UL) {
        // Now assign any remaining bands to D2D users.
        phase1_d2d(sorter, memory, assignedBands);

        EV << NOW << " LteMaxDatarate::phase1 has assigned " << assignedBands.size() << " bands." << std::endl;

        // Phase 2 of the algorithm checks for D2D pairs that went without any assigned bands.
        // If there are any, they are reassigned that band that yields the highest datarate for them.
        phase2(sorter, memory);

        // Notify the omniscient entity of this scheduling round.
        // If it is configured to record the decisions then it'll do so. Otherwise this doesn't do anything.
        mOracle->recordSchedulingRound(*memory);
    }

    // Scheduling is done. Delete the pointers, new ones will be instantiated in the next scheduling round.
    delete sorter;
    delete memory;
}

MaxDatarateSorter* LteMaxDatarate::sortBandsByDatarate(SchedulingMemory* memory) {
    int numBands;
    try {
        numBands = mOracle->getNumberOfBands();
    } catch (const cRuntimeError& e) {
        EV << NOW << " LteMaxDatarate::sortBandsByDatarate Oracle not yet ready. Skipping scheduling round." << std::endl;
        return nullptr;
    }

    MaxDatarateSorter* sorter = new MaxDatarateSorter(numBands);
    EV << NOW << " LteMaxDatarate::sortBandsByDatarate for " << activeConnectionTempSet_.size() << " connections." << std::endl;
    for (ActiveSet::iterator iterator = activeConnectionTempSet_.begin(); iterator != activeConnectionTempSet_.end (); ++iterator) {
        MacCid currentConnection = *iterator;
        MacNodeId nodeId = MacCidToNodeId(currentConnection);
        EV << NOW << " LteMaxDatarate::sortBandsByDatarate Checking node " << nodeId << "... ";

        // Make sure the current node has not been dynamically removed.
        if (getBinder()->getOmnetId(nodeId) == 0){
            activeConnectionTempSet_.erase(currentConnection);
            EV << " has been dynamically removed. Skipping." << std::endl;
            continue;
        }

        // Determine direction. Uplink and downlink resources are scheduled separately,
        // and LteScheduler's 'direction_' contains the current scheduling direction.
        Direction dir;
        // Uplink may be reused for D2D, so we have to check if the Buffer Status Report (BSR) tells us that this is a D2D link.
        if (direction_ == UL)
            dir = (MacCidToLcid(currentConnection) == D2D_SHORT_BSR) ? D2D : UL;
        else
            dir = DL;

        EV << "wants to transmit in " << dirToA(dir) << " direction." << std::endl;

        // Save it to memory.
        memory->put(nodeId, dir);

        // Determine device's maximum transmission power.
        double maxTransmitPower = mOracle->getTransmissionPower(nodeId, dir);
        // We need the SINR values for each band.
        std::vector<double> SINRs;
        MacNodeId destinationId;
        // SINR computation depends on the direction.
        switch (dir) {
            // Uplink: node->eNB
            case Direction::UL: {
                destinationId = mOracle->getEnodeBId();
                SINRs = mOracle->getSINR(nodeId, destinationId, NOW, maxTransmitPower);
                EV << NOW << " LteMaxDatarate::sortBandsByDatarate From node " << nodeId << " to eNodeB " << mOracle->getEnodeBId() << " (Uplink)" << std::endl;
                break;
            }
            // Downlink: eNB->node
            case Direction::DL: {
                destinationId = nodeId;
                nodeId = mOracle->getEnodeBId();
                SINRs = mOracle->getSINR(nodeId, destinationId , NOW, maxTransmitPower);
                EV << NOW << " LteMaxDatarate::sortBandsByDatarate From eNodeB " << mOracle->getEnodeBId() << " to node " << destinationId << " (Downlink)" << std::endl;
                break;
            }
            // Direct: node->node
            case Direction::D2D: {
                destinationId = determineD2DEndpoint(nodeId);
                SINRs = mOracle->getSINR(nodeId, destinationId, NOW, maxTransmitPower);
                EV << NOW << " LteMaxDatarate::sortBandsByDatarate From node " << nodeId << " to node " << destinationId << " (Direct Link)" << std::endl;
                break;
            }
            default: {
                // This can't happen, 'dir' is specifically set above. Just for the sake of completeness.
                throw cRuntimeError(std::string("LteMaxDatarate::sortBandsByDatarate sees undefined direction: " + std::to_string(dir)).c_str());
            }
        }

        // Go through all bands ...
        for (size_t i = 0; i < SINRs.size(); i++) {
            Band currentBand = Band(i);
            // Estimate maximum throughput.
            double associatedSinr = SINRs.at(currentBand);
            double estimatedThroughput = mOracle->getChannelCapacity(associatedSinr);
            // And put the result into the container.
            sorter->put(currentBand, IdRatePair(currentConnection, nodeId, destinationId, maxTransmitPower, estimatedThroughput, dir));
            EV << NOW << " LteMaxDatarate::sortBandsByDatarate sorter->put(" << currentBand << ", IdRatePair(connectionId=" << currentConnection << ", nodeId=" << nodeId << ", "
               << destinationId << ", txPw=" << maxTransmitPower << ", throughput=" << estimatedThroughput << ", dir=" << dirToA(dir) << "))" << std::endl;
        }
    }

    return sorter;
}

std::vector<Band> LteMaxDatarate::phase1_cellular(MaxDatarateSorter* sorter, SchedulingMemory* memory) {
    EV << NOW << " LteMaxDatarate::phase1 for cellular users" << endl;
    /** Remember the largest band actually assigned to a cellular user. */
    std::vector<Band> bandsAssigned;
    for (Band band = 0; band < sorter->size(); band++) {
        std::vector<IdRatePair> list = sorter->at_nonD2D(band);
        if (list.size() <= 0) {
            EV << NOW << " LteMaxDatarate::phase1_cellular Skipping band " << band << " because no cellular users are interested in it." << std::endl;
            continue;
        }
        // Assign band to best candidate.
        IdRatePair& bestCandidate = list.at(0);

        EV << NOW << " LteMaxDatarate::phase1_cellular granting " << bestCandidate.from << " -"
                << dirToA(bestCandidate.dir) << "-> " << bestCandidate.to << " on band " << band << std::endl;

        SchedulingResult grantAnswer = schedule(bestCandidate.connectionId, band);

        EV << NOW << " LteMaxDatarate::phase1_cellular grant answer is "
           << (grantAnswer == SchedulingResult::TERMINATE ? "TERMINATE" :
               grantAnswer == SchedulingResult::INACTIVE ? "INACTIVE" :
               grantAnswer == SchedulingResult::INELIGIBLE ? "INELIGIBLE" :
               "OK") << std::endl;

        // Save decision to memory.
        memory->put(bestCandidate.from, band, false);
        bandsAssigned.push_back(band);

        // Exit immediately if the terminate flag is set.
        if (grantAnswer == SchedulingResult::TERMINATE) {
            // The 'terminate' flag is set if a codeword is already allocated.
            // This is a, and I quote: "TODO This is just a BAD patch". Band reassignment is not intended with the
            // current version. So here we just ignore this answer.
        }

        // Set the connection as inactive if indicated by the grant.
        if (grantAnswer == SchedulingResult::INACTIVE) {
            EV << NOW << " LteMaxDatarate::phase1_cellular setting " << bestCandidate.from << " to inactive" << std::endl;
            activeConnectionTempSet_.erase(bestCandidate.from);
            // A connection is set as inactive if the node's queue length is 0.
            // This means nothing needs to be scheduled to this node anymore,
            // so remove it from our container so it's not considered anymore.
            sorter->remove(bestCandidate.from);
            continue;
        }

        // Now check if the same candidate should be assigned consecutive resource blocks.
        for (Band consecutiveBand = band + 1; consecutiveBand < sorter->size(); consecutiveBand++) {
            // Determine throughput for halved transmit power.
            double halvedTxPower = bestCandidate.txPower / 2;
            std::vector<double> consecutiveSINRs = mOracle->getSINR(bestCandidate.from, bestCandidate.to, NOW, halvedTxPower);
            double associatedSinr = consecutiveSINRs.at(consecutiveBand);
            double estimatedThroughput = mOracle->getChannelCapacity(associatedSinr);
            EV << NOW << " LteMaxDatarate::prepareSchedule Determined throughput for consecutive band " << consecutiveBand << " at halved transmit power of "
               << halvedTxPower << "dB: " << estimatedThroughput << std::endl;

            // Is this better than the next best candidate?
            std::vector<IdRatePair> consecutiveList = sorter->at(consecutiveBand);
            IdRatePair& bestConsecutiveCandidate = consecutiveList.at(0);

            if (bestConsecutiveCandidate.rate < estimatedThroughput) {
                EV << NOW << " LteMaxDatarate::phase1_cellular Consecutive throughput at halved txPower is still better than the best candidate: "
                   << bestConsecutiveCandidate.rate << " < " << estimatedThroughput << std::endl;

                EV << NOW << " LteMaxDatarate::phase1_cellular granting " << bestCandidate.from << " -"
                   << dirToA(bestCandidate.dir) << "-> " << bestCandidate.to << " on consecutive band " << band << std::endl;

                // Assign this band also.
                grantAnswer = schedule(bestCandidate.connectionId, consecutiveBand);

                EV << NOW << " LteMaxDatarate::phase1_cellular grant answer is "
                   << (grantAnswer == SchedulingResult::TERMINATE ? "TERMINATE" :
                       grantAnswer == SchedulingResult::INACTIVE ? "INACTIVE" :
                       grantAnswer == SchedulingResult::INELIGIBLE ? "INELIGIBLE" :
                       "OK") << std::endl;

                // Increment outer loop's 'band' so that 'consecutiveBand' is not double-assigned.
                band++;
                // Update txPower so that next consecutive check halves txPower again.
                bestCandidate.txPower = halvedTxPower;

                memory->put(bestCandidate.from, consecutiveBand, false);
                bandsAssigned.push_back(consecutiveBand);

                // Exit immediately if the terminate flag is set.
                if (grantAnswer == SchedulingResult::TERMINATE) {
                    // The 'terminate' flag is set if a codeword is already allocated.
                    // This is a, and I quote: "TODO This is just a BAD patch". Band reassignment is not intended with the
                    // current version. So here we just ignore this answer.
                }

                // Set the connection as inactive if indicated by the grant.
                if (grantAnswer == SchedulingResult::INACTIVE) {
                    EV << NOW << " LteMaxDatarate::phase1_cellular setting " << bestCandidate.from << " to inactive" << std::endl;
                    activeConnectionTempSet_.erase(bestCandidate.from);
                    // A connection is set as inactive if the node's queue length is 0.
                    // This means nothing needs to be scheduled to this node anymore,
                    // so remove it from our container so it's not considered anymore.
                    sorter->remove(bestCandidate.from);
                    continue;
                }

            // Current candidate transmitting at halved power has worse throughput than next candidate.
            } else {
                EV << NOW << "LteMaxDatarate::phase1_cellular Consecutive throughput at halved txPower is not better than the best candidate: "
                   << bestConsecutiveCandidate.rate << " (node " << bestConsecutiveCandidate.from << " -> " << bestConsecutiveCandidate.to << ") >= " << estimatedThroughput << std::endl;
                // Remove current candidate as it shouldn't be assigned any more resource blocks in this scheduling round.
                sorter->remove(bestCandidate.from);
                // Quit checking consecutive bands. Next band will be assigned to the next best candidate.
                break;
            }
        }
    }
    return bandsAssigned;
}

void LteMaxDatarate::phase1_d2d(MaxDatarateSorter* sorter, SchedulingMemory* memory, std::vector<Band>& alreadyAssignedBands) {
    EV << NOW << " LteMaxDatarate::phase1 for D2D users" << endl;
    for (Band band = 0; band < sorter->size(); band++) {
        if (alreadyAssignedBands.size() > 0 && std::find(alreadyAssignedBands.begin(), alreadyAssignedBands.end(), band) != alreadyAssignedBands.end()) {
            // Band already assigned to a cellular user.
            EV << NOW << " LteMaxDatarate::phase1_d2d skipping band " << band << " because it's already assigned to a cellular user." << std::endl;
            continue;
        }
        std::vector<IdRatePair> list = sorter->at(band, Direction::D2D);
        if (list.size() <= 0) {
            EV << NOW << " LteMaxDatarate::phase1_d2d Skipping band " << band << " because no D2D users are interested in it." << std::endl;
            continue;
        }
        // Assign band to best candidate.
        IdRatePair& bestCandidate = list.at(0);

        EV << NOW << " LteMaxDatarate::phase1_d2d granting " << bestCandidate.from << " -"
           << dirToA(bestCandidate.dir) << "-> " << bestCandidate.to << " on band " << band << std::endl;

        SchedulingResult grantAnswer = schedule(bestCandidate.connectionId, band);

        EV << NOW << " LteMaxDatarate::phase1_d2d grant answer is "
           << (grantAnswer == SchedulingResult::TERMINATE ? "TERMINATE" :
               grantAnswer == SchedulingResult::INACTIVE ? "INACTIVE" :
               grantAnswer == SchedulingResult::INELIGIBLE ? "INELIGIBLE" :
               "OK") << std::endl;

        // Save decision to memory.
        memory->put(bestCandidate.from, band, false);
        alreadyAssignedBands.push_back(band);

        // Exit immediately if the terminate flag is set.
        if (grantAnswer == SchedulingResult::TERMINATE) {
            // The 'terminate' flag is set if a codeword is already allocated.
            // This is a, and I quote: "TODO This is just a BAD patch". Band reassignment is not intended with the
            // current version. So here we just ignore this answer.
        }

        // Set the connection as inactive if indicated by the grant.
        if (grantAnswer == SchedulingResult::INACTIVE) {
            EV << NOW << " LteMaxDatarate::phase1_d2d setting " << bestCandidate.from << " to inactive" << std::endl;
            activeConnectionTempSet_.erase(bestCandidate.from);
            // A connection is set as inactive if the node's queue length is 0.
            // This means nothing needs to be scheduled to this node anymore,
            // so remove it from our container so it's not considered anymore.
            sorter->remove(bestCandidate.from);
            continue;
        }

        // Now check if the same candidate should be assigned consecutive resource blocks.
        for (Band consecutiveBand = band + 1; consecutiveBand < sorter->size(); consecutiveBand++) {
            if (alreadyAssignedBands.size() > 0 && std::find(alreadyAssignedBands.begin(), alreadyAssignedBands.end(), consecutiveBand) != alreadyAssignedBands.end()) {
                // Band already assigned to a cellular user.
                EV << NOW << " LteMaxDatarate::phase1_d2d skipping consecutive band " << consecutiveBand << " because it's already assigned to a cellular user." << std::endl;
                continue;
            }
            // Determine throughput for halved transmit power.
            double halvedTxPower = bestCandidate.txPower / 2;
            std::vector<double> consecutiveSINRs = mOracle->getSINR(bestCandidate.from, bestCandidate.to, NOW, halvedTxPower);
            double associatedSinr = consecutiveSINRs.at(consecutiveBand);
            double estimatedThroughput = mOracle->getChannelCapacity(associatedSinr);
            EV << NOW << " LteMaxDatarate::phase1_d2d Determined throughput for consecutive band " << consecutiveBand << " at halved transmit power of "
               << halvedTxPower << ": " << estimatedThroughput << std::endl;

            // Is this better than the next best candidate?
            std::vector<IdRatePair> consecutiveList = sorter->at(consecutiveBand, Direction::D2D);
            IdRatePair& bestConsecutiveCandidate = consecutiveList.at(0);

            if (bestConsecutiveCandidate.rate < estimatedThroughput) {
                EV << NOW << " LteMaxDatarate::phase1_d2d Consecutive throughput at halved txPower is still better than the best candidate: "
                   << bestConsecutiveCandidate.rate << " < " << estimatedThroughput << std::endl;

                EV << NOW << " LteMaxDatarate::phase1_d2d granting " << bestCandidate.from << " -"
                   << dirToA(bestCandidate.dir) << "-> " << bestCandidate.to << " on consecutive band " << band << std::endl;
                // Assign this band also.
                grantAnswer = schedule(bestCandidate.connectionId, consecutiveBand);

                EV << NOW << " LteMaxDatarate::phase1_d2d grant answer is "
                   << (grantAnswer == SchedulingResult::TERMINATE ? "TERMINATE" :
                       grantAnswer == SchedulingResult::INACTIVE ? "INACTIVE" :
                       grantAnswer == SchedulingResult::INELIGIBLE ? "INELIGIBLE" :
                       "OK") << std::endl;

                // Increment outer loop's 'band' so that 'consecutiveBand' is not double-assigned.
                band++;
                // Update txPower so that next consecutive check halves txPower again.
                bestCandidate.txPower = halvedTxPower;

                memory->put(bestCandidate.from, consecutiveBand, false);
                alreadyAssignedBands.push_back(consecutiveBand);

                // Exit immediately if the terminate flag is set.
                if (grantAnswer == SchedulingResult::TERMINATE) {
                    // The 'terminate' flag is set if a codeword is already allocated.
                    // This is a, and I quote: "TODO This is just a BAD patch". Band reassignment is not intended with the
                    // current version. So here we just ignore this answer.
                }

                // Set the connection as inactive if indicated by the grant.
                if (grantAnswer == SchedulingResult::INACTIVE) {
                    EV << NOW << " LteMaxDatarate::phase1_d2d setting " << bestCandidate.from << " to inactive" << std::endl;
                    activeConnectionTempSet_.erase(bestCandidate.from);
                    // A connection is set as inactive if the node's queue length is 0.
                    // This means nothing needs to be scheduled to this node anymore,
                    // so remove it from our container so it's not considered anymore.
                    sorter->remove(bestCandidate.from);
                    continue;
                }

            // Current candidate transmitting at halved power has worse throughput than next candidate.
            } else {
                EV << NOW << " LteMaxDatarate::phase1_d2d Consecutive throughput at halved txPower is not better than the best candidate: "
                   << bestConsecutiveCandidate.rate << " >= " << estimatedThroughput << std::endl;
                // Remove current candidate as it shouldn't be assigned any more resource blocks in this scheduling round.
                sorter->remove(bestCandidate.from);
                // Quit checking consecutive bands. Next band will be assigned to the next best candidate.
                break;
            }
        }
    }
}

void LteMaxDatarate::phase2(MaxDatarateSorter* sorter, SchedulingMemory* memory) {
    EV << NOW << " LteMaxDatarate::phase2" << endl;
    // Determine D2D pairs that went unassigned.
    std::size_t numberOfReassignedBands = 0;
    for (ActiveSet::iterator iterator = activeConnectionTempSet_.begin(); iterator != activeConnectionTempSet_.end (); ++iterator) {
        MacCid currentConnection = *iterator;
        MacNodeId nodeId = MacCidToNodeId(currentConnection);
        const Direction& dir = memory->getDirection(nodeId);
        if (dir == Direction::D2D) {
            if (memory->getNumberAssignedBands(nodeId) == 0) {
                // Found an unassigned pair.
                EV << NOW << " LteMaxDatarate::phase2 Node " << nodeId << " has not been assigned a band yet." << endl;
                // Reassign that band that has the best datarate for this node.
                Band bestBand;
                try {
                    bestBand = sorter->getBestBand(nodeId);
                } catch (const std::exception& e) {
                    // An exception is thrown if no bands are available.
                    continue;
                }
                EV  << NOW << " LteMaxDatarate::phase2 Reassigning band " << bestBand << " to node " << nodeId << endl;
                SchedulingResult grantAnswer = schedule(currentConnection, bestBand);
                memory->put(nodeId, bestBand, true);
                // Mark the band as reassigned so it won't be double-reassigned.
                sorter->markBand(bestBand, true);

                EV << NOW << " LteMaxDatarate::phase2 grant answer is "
                   << (grantAnswer == SchedulingResult::TERMINATE ? "TERMINATE" :
                       grantAnswer == SchedulingResult::INACTIVE ? "INACTIVE" :
                       grantAnswer == SchedulingResult::INELIGIBLE ? "INELIGIBLE" :
                       "OK") << std::endl;

                // Exit immediately if the terminate flag is set.
                if (grantAnswer == SchedulingResult::TERMINATE) {
                    // The 'terminate' flag is set if a codeword is already allocated.
                    // This is a, and I quote: "TODO This is just a BAD patch". Band reassignment is not intended with the
                    // current version. So here we just ignore this answer.
                }

                // Set the connection as inactive if indicated by the grant.
                if (grantAnswer == SchedulingResult::INACTIVE) {
                    EV << NOW << " LteMaxDatarate::phase2 setting " << nodeId << " to inactive" << std::endl;
                    activeConnectionTempSet_.erase(nodeId);
                    // A connection is set as inactive if the node's queue length is 0.
                    // This means nothing needs to be scheduled to this node anymore,
                    // so remove it from our container so it's not considered anymore.
                    sorter->remove(nodeId);
                    continue;
                }
                numberOfReassignedBands++;
            }
        }
    }
    EV << NOW << " LteMaxDatarate::phase2 has reassigned " << numberOfReassignedBands << " bands." << std::endl;
}

LteMaxDatarate::SchedulingResult LteMaxDatarate::schedule(MacCid connectionId, Band band) {
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
        return LteMaxDatarate::SchedulingResult::TERMINATE;
    else if (!active)
        return LteMaxDatarate::SchedulingResult::INACTIVE;
    else if (!eligible)
        return LteMaxDatarate::SchedulingResult::INELIGIBLE;
    else
        return LteMaxDatarate::SchedulingResult::OK;
}

void LteMaxDatarate::commitSchedule() {
    EV_STATICCONTEXT;
    EV << NOW << " LteMaxDatarate::commitSchedule" << std::endl;
    activeConnectionSet_ = activeConnectionTempSet_;
}

MacNodeId LteMaxDatarate::determineD2DEndpoint(MacNodeId srcNode) const {
    EV_STATICCONTEXT;
    EV << NOW << " LteMaxDatarate::determineD2DEndpoint" << std::endl;
    // Grab <from, <to, mode>> map for all 'from' nodes.
    const std::map<MacNodeId, std::map<MacNodeId, LteD2DMode>>* map = mOracle->getModeSelectionMap();
    if (map == nullptr)
        throw cRuntimeError("LteMaxDatarate::determineD2DEndpoint couldn't get a mode selection map from the oracle.");
    // From there, grab <src, <to, mode>> for given 'srcNode'.
    const std::map<MacNodeId, LteD2DMode> destinationModeMap = map->at(srcNode);
    if (destinationModeMap.size() <= 0)
        throw cRuntimeError(std::string("LteMaxDatarate::determineD2DEndpoint <src, <to, mode>> map is empty -> couldn't find an end point from node " + std::to_string(srcNode)).c_str());
    MacNodeId dstNode = 0;
    bool foundIt = false;
    // Go through all possible destinations.
    for (std::map<MacNodeId, LteD2DMode>::const_iterator iterator = destinationModeMap.begin();
            iterator != destinationModeMap.end(); iterator++) {
        EV << NOW << " LteMaxDatarate::prepareSchedule\tChecking candidate node " << (*iterator).first << "... ";
        // Check if it wanted to run in Direct Mode.
        if ((*iterator).second == LteD2DMode::DM) {
            // If yes, then consider this the endpoint.
            // @TODO this seems like a hacky workaround. >1 endpoints are not supported with this approach.
            foundIt = true;
            dstNode = (*iterator).first;
            EV << "found D2D partner." << std::endl;
            break;
        }
        EV << "nope." << std::endl;
    }
    if (!foundIt)
        throw cRuntimeError(std::string("LteMaxDatarate::determineD2DEndpoint couldn't find an end point from node " + std::to_string(srcNode)).c_str());
    EV << NOW << " LteMaxDatarate::determineD2DEndpoint found " << srcNode << " --> " << dstNode << "." << std::endl;
    return dstNode;
}
