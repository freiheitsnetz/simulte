/*
 * LteUnassistedHeuristic.cc
 *
 *  Created on: Aug 21, 2017
 *      Author: Sunil Upardrasta
 */

#include <stack/mac/scheduling_modules/unassistedHeuristic/LteUnassistedHeuristic.h>
#include <stack/mac/scheduler/LteSchedulerEnb.h>
#include <algorithm>
#include "stack/mac/buffer/LteMacBuffer.h"
#include <stack/mac/scheduler/LteSchedulerUeAutoD2D.h>
#include <stack/mac/layer/LteMacUeD2D.h>

LteUnassistedHeuristic::LteUnassistedHeuristic() {
    mOracle = Oracle::get();
    EV_STATICCONTEXT;
    EV << "LteUnassistedHeuristic::constructor" << std::endl;
    EV << "\t" << (mOracle == nullptr ? "Couldn't find oracle." : "Found oracle.") << std::endl;
    if (mOracle == nullptr)
        throw cRuntimeError("LteUnassistedHeuristic couldn't find the oracle.");
}

LteUnassistedHeuristic::~LteUnassistedHeuristic() {}

void LteUnassistedHeuristic::prepareSchedule() {
    EV_STATICCONTEXT;
    EV << NOW << " LteUnassistedHeuristic::prepareSchedule for " << (direction_ == DL ? "DOWNLINK" : "UPLINK") << " for " << activeConnectionSet_.size() << " connections." << std::endl;

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
      EV << NOW << " LteUnassistedHeuristic::prepareSchedule stopping because oracle is not ready yet." << std::endl;
    }

    // We now have all <band, id> combinations sorted by expected datarate.
    EV << NOW << " LteUnassistedHeuristic::prepareSchedule bands sorted according to their expected throughputs: " << std::endl;
    EV << sorter->toString(std::string(NOW.str() + " LteUnassistedHeuristic::prepareSchedule ")) << std::endl;

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

        EV << NOW << " LteUnassistedHeuristic::phase1 has assigned " << assignedBands.size() << " bands." << std::endl;

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

MaxDatarateSorter* LteUnassistedHeuristic::sortBandsByDatarate(SchedulingMemory* memory) {
    int numBands;
    try {
        numBands = mOracle->getNumberOfBands();
    } catch (const cRuntimeError& e) {
        EV << NOW << " LteUnassistedHeuristic::sortBandsByDatarate Oracle not yet ready. Skipping scheduling round." << std::endl;
        return nullptr;
    }

    MaxDatarateSorter* sorter = new MaxDatarateSorter(numBands);
    EV << NOW << " LteUnassistedHeuristic::sortBandsByDatarate for " << activeConnectionTempSet_.size() << " connections." << std::endl;
    for (ActiveSet::iterator iterator = activeConnectionTempSet_.begin(); iterator != activeConnectionTempSet_.end (); ++iterator) {
        MacCid currentConnection = *iterator;
        MacNodeId nodeId = MacCidToNodeId(currentConnection);
        EV << NOW << " LteUnassistedHeuristic::sortBandsByDatarate Checking node " << nodeId << "... ";

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
                EV << NOW << " LteUnassistedHeuristic::sortBandsByDatarate From node " << nodeId << " to eNodeB " << mOracle->getEnodeBId() << " (Uplink)" << std::endl;
                break;
            }
            // Downlink: eNB->node
            case Direction::DL: {
                destinationId = nodeId;
                nodeId = mOracle->getEnodeBId();
                SINRs = mOracle->getSINR(nodeId, destinationId , NOW, maxTransmitPower);
                EV << NOW << " LteUnassistedHeuristic::sortBandsByDatarate From eNodeB " << mOracle->getEnodeBId() << " to node " << destinationId << " (Downlink)" << std::endl;
                break;
            }
            // Direct: node->node
            case Direction::D2D: {
                destinationId = determineD2DEndpoint(nodeId);
                SINRs = mOracle->getSINR(nodeId, destinationId, NOW, maxTransmitPower);
                EV << NOW << " LteUnassistedHeuristic::sortBandsByDatarate From node " << nodeId << " to node " << destinationId << " (Direct Link)" << std::endl;
                break;
            }
            default: {
                // This can't happen, 'dir' is specifically set above. Just for the sake of completeness.
                throw cRuntimeError(std::string("LteUnassistedHeuristic::sortBandsByDatarate sees undefined direction: " + std::to_string(dir)).c_str());
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
            EV << NOW << " LteUnassistedHeuristic::sortBandsByDatarate sorter->put(" << currentBand << ", IdRatePair(connectionId=" << currentConnection << ", nodeId=" << nodeId << ", "
               << destinationId << ", txPw=" << maxTransmitPower << ", throughput=" << estimatedThroughput << ", dir=" << dirToA(dir) << "))" << std::endl;
        }
    }

    return sorter;
}

std::vector<Band> LteUnassistedHeuristic::phase1_cellular(MaxDatarateSorter* sorter, SchedulingMemory* memory) {
    EV << NOW << " LteUnassistedHeuristic::phase1 for cellular users" << endl;
    /** Remember the largest band actually assigned to a cellular user. */
    std::vector<Band> bandsAssigned;
    for (Band band = 0; band < sorter->size(); band++) {
        std::vector<IdRatePair> list = sorter->at_nonD2D(band);
        if (list.size() <= 0) {
            EV << NOW << " LteUnassistedHeuristic::phase1_cellular Skipping band " << band << " because no cellular users are interested in it." << std::endl;
            continue;
        }
        // Assign band to best candidate.
        IdRatePair& bestCandidate = list.at(0);

        EV << NOW << " LteUnassistedHeuristic::phase1_cellular granting " << bestCandidate.from << " -"
                << dirToA(bestCandidate.dir) << "-> " << bestCandidate.to << " on band " << band << std::endl;

        SchedulingResult grantAnswer = schedule(bestCandidate.connectionId, band);

        EV << NOW << " LteUnassistedHeuristic::phase1_cellular grant answer is "
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
            EV << NOW << " LteUnassistedHeuristic::phase1_cellular setting " << bestCandidate.from << " to inactive" << std::endl;
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
            EV << NOW << " LteUnassistedHeuristic::prepareSchedule Determined throughput for consecutive band " << consecutiveBand << " at halved transmit power of "
               << halvedTxPower << "dB: " << estimatedThroughput << std::endl;

            // Is this better than the next best candidate?
            std::vector<IdRatePair> consecutiveList = sorter->at(consecutiveBand);
            IdRatePair& bestConsecutiveCandidate = consecutiveList.at(0);

            if (bestConsecutiveCandidate.rate < estimatedThroughput) {
                EV << NOW << " LteUnassistedHeuristic::phase1_cellular Consecutive throughput at halved txPower is still better than the best candidate: "
                   << bestConsecutiveCandidate.rate << " < " << estimatedThroughput << std::endl;

                EV << NOW << " LteUnassistedHeuristic::phase1_cellular granting " << bestCandidate.from << " -"
                   << dirToA(bestCandidate.dir) << "-> " << bestCandidate.to << " on consecutive band " << band << std::endl;

                // Assign this band also.
                grantAnswer = schedule(bestCandidate.connectionId, consecutiveBand);

                EV << NOW << " LteUnassistedHeuristic::phase1_cellular grant answer is "
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
                    EV << NOW << " LteUnassistedHeuristic::phase1_cellular setting " << bestCandidate.from << " to inactive" << std::endl;
                    activeConnectionTempSet_.erase(bestCandidate.from);
                    // A connection is set as inactive if the node's queue length is 0.
                    // This means nothing needs to be scheduled to this node anymore,
                    // so remove it from our container so it's not considered anymore.
                    sorter->remove(bestCandidate.from);
                    continue;
                }

            // Current candidate transmitting at halved power has worse throughput than next candidate.
            } else {
                EV << NOW << "LteUnassistedHeuristic::phase1_cellular Consecutive throughput at halved txPower is not better than the best candidate: "
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

void LteUnassistedHeuristic::phase1_d2d(MaxDatarateSorter* sorter, SchedulingMemory* memory, std::vector<Band>& alreadyAssignedBands) {
    EV << NOW << " LteUnassistedHeuristic::phase1 for D2D users" << endl;
    for (Band band = 0; band < sorter->size(); band++) {
        if (alreadyAssignedBands.size() > 0 && std::find(alreadyAssignedBands.begin(), alreadyAssignedBands.end(), band) != alreadyAssignedBands.end()) {
            // Band already assigned to a cellular user.
            EV << NOW << " LteUnassistedHeuristic::phase1_d2d skipping band " << band << " because it's already assigned to a cellular user." << std::endl;
            continue;
        }
        std::vector<IdRatePair> list = sorter->at(band, Direction::D2D);
        if (list.size() <= 0) {
            EV << NOW << " LteUnassistedHeuristic::phase1_d2d Skipping band " << band << " because no D2D users are interested in it." << std::endl;
            continue;
        }
        // Assign band to best candidate.
        IdRatePair& bestCandidate = list.at(0);

        EV << NOW << " LteUnassistedHeuristic::phase1_d2d granting " << bestCandidate.from << " -"
           << dirToA(bestCandidate.dir) << "-> " << bestCandidate.to << " on band " << band << std::endl;

        SchedulingResult grantAnswer = schedule(bestCandidate.connectionId, band);

        EV << NOW << " LteUnassistedHeuristic::phase1_d2d grant answer is "
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
            EV << NOW << " LteUnassistedHeuristic::phase1_d2d setting " << bestCandidate.from << " to inactive" << std::endl;
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
                EV << NOW << " LteUnassistedHeuristic::phase1_d2d skipping consecutive band " << consecutiveBand << " because it's already assigned to a cellular user." << std::endl;
                continue;
            }
            // Determine throughput for halved transmit power.
            double halvedTxPower = bestCandidate.txPower / 2;
            std::vector<double> consecutiveSINRs = mOracle->getSINR(bestCandidate.from, bestCandidate.to, NOW, halvedTxPower);
            double associatedSinr = consecutiveSINRs.at(consecutiveBand);
            double estimatedThroughput = mOracle->getChannelCapacity(associatedSinr);
            EV << NOW << " LteUnassistedHeuristic::phase1_d2d Determined throughput for consecutive band " << consecutiveBand << " at halved transmit power of "
               << halvedTxPower << ": " << estimatedThroughput << std::endl;

            // Is this better than the next best candidate?
            std::vector<IdRatePair> consecutiveList = sorter->at(consecutiveBand, Direction::D2D);
            IdRatePair& bestConsecutiveCandidate = consecutiveList.at(0);

            if (bestConsecutiveCandidate.rate < estimatedThroughput) {
                EV << NOW << " LteUnassistedHeuristic::phase1_d2d Consecutive throughput at halved txPower is still better than the best candidate: "
                   << bestConsecutiveCandidate.rate << " < " << estimatedThroughput << std::endl;

                EV << NOW << " LteUnassistedHeuristic::phase1_d2d granting " << bestCandidate.from << " -"
                   << dirToA(bestCandidate.dir) << "-> " << bestCandidate.to << " on consecutive band " << band << std::endl;
                // Assign this band also.
                grantAnswer = schedule(bestCandidate.connectionId, consecutiveBand);

                EV << NOW << " LteUnassistedHeuristic::phase1_d2d grant answer is "
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
                    EV << NOW << " LteUnassistedHeuristic::phase1_d2d setting " << bestCandidate.from << " to inactive" << std::endl;
                    activeConnectionTempSet_.erase(bestCandidate.from);
                    // A connection is set as inactive if the node's queue length is 0.
                    // This means nothing needs to be scheduled to this node anymore,
                    // so remove it from our container so it's not considered anymore.
                    sorter->remove(bestCandidate.from);
                    continue;
                }

            // Current candidate transmitting at halved power has worse throughput than next candidate.
            } else {
                EV << NOW << " LteUnassistedHeuristic::phase1_d2d Consecutive throughput at halved txPower is not better than the best candidate: "
                   << bestConsecutiveCandidate.rate << " >= " << estimatedThroughput << std::endl;
                // Remove current candidate as it shouldn't be assigned any more resource blocks in this scheduling round.
                sorter->remove(bestCandidate.from);
                // Quit checking consecutive bands. Next band will be assigned to the next best candidate.
                break;
            }
        }
    }
}

void LteUnassistedHeuristic::phase2(MaxDatarateSorter* sorter, SchedulingMemory* memory) {
    EV << NOW << " LteUnassistedHeuristic::phase2" << endl;
    // Determine D2D pairs that went unassigned.
    std::size_t numberOfReassignedBands = 0;
    for (ActiveSet::iterator iterator = activeConnectionTempSet_.begin(); iterator != activeConnectionTempSet_.end (); ++iterator) {
        MacCid currentConnection = *iterator;
        MacNodeId nodeId = MacCidToNodeId(currentConnection);
        const Direction& dir = memory->getDirection(nodeId);
        if (dir == Direction::D2D) {
            if (memory->getNumberAssignedBands(nodeId) == 0) {
                // Found an unassigned pair.
                EV << NOW << " LteUnassistedHeuristic::phase2 Node " << nodeId << " has not been assigned a band yet." << endl;
                // Reassign that band that has the best datarate for this node.
                Band bestBand;
                try {
                    bestBand = sorter->getBestBand(nodeId);
                } catch (const std::exception& e) {
                    // An exception is thrown if no bands are available.
                    continue;
                }
                EV  << NOW << " LteUnassistedHeuristic::phase2 Reassigning band " << bestBand << " to node " << nodeId << endl;
                SchedulingResult grantAnswer = schedule(currentConnection, bestBand);
                memory->put(nodeId, bestBand, true);
                // Mark the band as reassigned so it won't be double-reassigned.
                sorter->markBand(bestBand, true);

                EV << NOW << " LteUnassistedHeuristic::phase2 grant answer is "
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
                    EV << NOW << " LteUnassistedHeuristic::phase2 setting " << nodeId << " to inactive" << std::endl;
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
    EV << NOW << " LteUnassistedHeuristic::phase2 has reassigned " << numberOfReassignedBands << " bands." << std::endl;
}

LteUnassistedHeuristic::SchedulingResult LteUnassistedHeuristic::schedule(MacCid connectionId, Band band) {
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
        return LteUnassistedHeuristic::SchedulingResult::TERMINATE;
    else if (!active)
        return LteUnassistedHeuristic::SchedulingResult::INACTIVE;
    else if (!eligible)
        return LteUnassistedHeuristic::SchedulingResult::INELIGIBLE;
    else
        return LteUnassistedHeuristic::SchedulingResult::OK;
}

void LteUnassistedHeuristic::commitSchedule() {
    EV_STATICCONTEXT;
    EV << NOW << " LteUnassistedHeuristic::commitSchedule" << std::endl;
    activeConnectionSet_ = activeConnectionTempSet_;
}

MacNodeId LteUnassistedHeuristic::determineD2DEndpoint(MacNodeId srcNode) const {
    EV_STATICCONTEXT;
    EV << NOW << " LteUnassistedHeuristic::determineD2DEndpoint" << std::endl;
    // Grab <from, <to, mode>> map for all 'from' nodes.
    const std::map<MacNodeId, std::map<MacNodeId, LteD2DMode>>* map = mOracle->getModeSelectionMap();
    if (map == nullptr)
        throw cRuntimeError("LteUnassistedHeuristic::determineD2DEndpoint couldn't get a mode selection map from the oracle.");
    // From there, grab <src, <to, mode>> for given 'srcNode'.
    const std::map<MacNodeId, LteD2DMode> destinationModeMap = map->at(srcNode);
    if (destinationModeMap.size() <= 0)
        throw cRuntimeError(std::string("LteUnassistedHeuristic::determineD2DEndpoint <src, <to, mode>> map is empty -> couldn't find an end point from node " + std::to_string(srcNode)).c_str());
    MacNodeId dstNode = 0;
    bool foundIt = false;
    // Go through all possible destinations.
    for (std::map<MacNodeId, LteD2DMode>::const_iterator iterator = destinationModeMap.begin();
            iterator != destinationModeMap.end(); iterator++) {
        EV << NOW << " LteUnassistedHeuristic::prepareSchedule\tChecking candidate node " << (*iterator).first << "... ";
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
        throw cRuntimeError(std::string("LteUnassistedHeuristic::determineD2DEndpoint couldn't find an end point from node " + std::to_string(srcNode)).c_str());
    EV << NOW << " LteUnassistedHeuristic::determineD2DEndpoint found " << srcNode << " --> " << dstNode << "." << std::endl;
    return dstNode;
}

ScheduleList& LteUnassistedHeuristic::schedule(unsigned int availableBytes, Direction grantDir)
{
        /* clean up old schedule decisions
         for each cid, this map will store the the amount of sent data (in SDUs)
         */
        scheduleList_.clear();

        /*
         * Clean up scheduling support status map
         */
        statusMap_.clear();

        // amount of time passed since last scheduling operation
        // simtime_t timeInterval = NOW - lastExecutionTime_;

        // these variables will contain the flow identifier and the priority of the lowest
        // priority flow encountered during the scheduling process
        // int lowestBackloggedFlow = -1;
        // int lowestBackloggedPriority = -1;

        // these variables will contain the flow identifier and the priority of the highest
        // priority flow encountered during the scheduling process
        // int highestBackloggedFlow = -1;
        // int highestBackloggedPriority = -1;

        // Considering 3GPP TS 36.321:
        //  - ConnDesc::parameters_.minReservedRate_    --> Prioritized Bit Rate (PBR) in Bytes/s
        //  - ConnDesc::parameters_.maximumBucketSizeurst_         --> Maximum Bucket size (MBS) = PBR*BSD product
        //  - ConnDesc::parameters_.bucket_         --> is the B parameter in Bytes

        // If true, assure a minimum reserved rate to all connection (LCP first
        // phase), if false, provide a best effort service (LCP second phase)
        bool priorityService = true;

        LcgMap& lcgMap = mac_->getLcgMap();

        if (lcgMap.empty())
            return scheduleList_;

        // for all traffic classes
        for (unsigned short i = 0; i < UNKNOWN_TRAFFIC_TYPE; ++i)
        {
            // Prepare the iterators to cycle the entire scheduling set
            std::pair<LcgMap::iterator, LcgMap::iterator> it_pair;
            it_pair = lcgMap.equal_range((LteTrafficClass) i);
            LcgMap::iterator it = it_pair.first, et = it_pair.second;

            EV << NOW << " LteSchedulerUeUl::schedule - Node  " << mac_->getMacNodeId() << ", Starting priority service for traffic class " << i << endl;

            //! FIXME Allocation of the same resource to flows with same priority not implemented - not suitable with relays
            for (; it != et; ++it)
            {
                // processing all connections of same traffic class

                // get the connection virtual buffer
                LteMacBuffer* vQueue = it->second.second;

                // connection id of the processed connection
                MacCid cid = it->second.first;

                // get the Flow descriptor
                FlowControlInfo connDesc = mac_->getConnDesc().at(cid);

                if (connDesc.getDirection() != grantDir)  // if the connection has different direction from the grant direction, skip it
                {
                    EV << NOW << " LteSchedulerUeUl::schedule - Connection " << cid << " is " << dirToA((Direction)connDesc.getDirection()) << " whereas grant is " << dirToA(grantDir) << ". Skip. " << endl;
                    continue;
                }

                // TODO get the QoS parameters

    //            // get a pointer to the appropriate status element: we need a tracing element
    //            // in order to store information about connections and data transmitted. These
    //            // information may be consulted at the end of the LCP algorithm
                StatusElem* elem;

                if (statusMap_.find(cid) == statusMap_.end())
                {
                    // the element does not exist, initialize it
                    elem = &statusMap_[cid];
                    elem->occupancy_ = vQueue->getQueueLength();
                    elem->sentData_ = 0;
                    elem->sentSdus_ = 0;
                    // TODO set bucket from QoS parameters
                    elem->bucket_ = 1000;
                }
                else
                {
                    elem = &statusMap_[cid];
                }

                EV << NOW << " LteSchedulerUeUl::schedule Node " << mac_->getMacNodeId() << " , Parameters:" << endl;
                EV << "\t Logical Channel ID: " << MacCidToLcid(cid) << endl;
                EV << "\t CID: " << cid << endl;
    //                fprintf(stderr, "\tGroup ID: %d\n", desc->parameters_.groupId_);
    //                fprintf(stderr, "\tPriority: %d\n", desc->parameters_.priority_);
    //                fprintf(stderr, "\tMin Reserved Rate: %.0lf bytes/s\n", desc->parameters_.minReservedRate_);
    //                fprintf(stderr, "\tMax Burst: %.0lf bytes\n", desc->parameters_.maxBurst_);

                if (priorityService)
                {
                    // Update bucket value for this connection

                    // get the actual bucket value and the configured max size
                    double bucket = elem->bucket_; // TODO parameters -> bucket ;
                    double maximumBucketSize = 10000.0; // TODO  parameters -> maxBurst;

                    EV << NOW << " LteSchedulerUeUl::schedule Bucket size: " << bucket << " bytes (max size " << maximumBucketSize << " bytes) - BEFORE SERVICE " << endl;

                    // if the connection started before last scheduling event , use the
                    // global time interval
                    if (lastExecutionTime_ > 0)
                    { // TODO desc->parameters_.startTime_) {
    //                    // PBR*(n*TTI) where n is the number of TTI from last update
                        bucket += /* TODO desc->parameters_.minReservedRate_*/ 100.0 * TTI;
                    }
    //                // otherwise, set the bucket value accordingly to the start time
                    else
                    {
                        simtime_t localTimeInterval = NOW - 0/* TODO desc->parameters_.startTime_ */;
                        if (localTimeInterval < 0)
                            localTimeInterval = 0;

                        bucket = /* TODO desc->parameters_.minReservedRate_*/ 100.0 * localTimeInterval.dbl();
                    }

                    // do not overflow the maximum bucket size
                    if (bucket > maximumBucketSize)
                        bucket = maximumBucketSize;

                    // update connection's bucket
    // TODO                desc->parameters_.bucket_ = bucket;

    //                // update the tracing element accordingly
                    elem->bucket_ = 100.0; // TODO desc->parameters_.bucket_;
                    EV << NOW << " LteSchedulerUeUl::schedule Bucket size: " << bucket << " bytes (max size " << maximumBucketSize << " bytes) - AFTER SERVICE " << endl;
                }

                EV << NOW << " LteSchedulerUeUl::schedule - Node " << mac_->getMacNodeId() << ", remaining grant: " << availableBytes << " bytes " << endl;
                EV << NOW << " LteSchedulerUeUl::schedule - Node " << mac_->getMacNodeId() << " buffer Size: " << vQueue->getQueueOccupancy() << " bytes " << endl;

    //
    //            // If priority service: (availableBytes>0) && (desc->buffer_.occupancy() > 0) && (desc->parameters_.bucket_ > 0)
    //            // If best effort service: (availableBytes>0) && (desc->buffer_.occupancy() > 0)
                while ((availableBytes > 0) && (vQueue->getQueueOccupancy() > 0)
                    && (!priorityService || 1 /*TODO (desc->parameters_.bucket_ > 0)*/))
                {
                    // get size of hol sdu
                    unsigned int sduSize = vQueue->front().first;

                    // Check if it is possible to serve the sdu, depending on the constraint
    //                // of the type of service
    //                // Priority service:
    //                //    ( sdu->size() <= availableBytes) && ( sdu->size() <= desc->parameters_.bucket_)
    //                // Best Effort service:
    //                //    ( sdu->size() <= availableBytes) && (!priorityService_)

                    if ((sduSize <= availableBytes) /*&& ( !priorityService || ( sduSize <= 0 TODO desc->parameters_.bucket_) )*/)
                    {
                        // remove SDU from virtual buffer
                        vQueue->popFront();

                        if (priorityService)
                        {
    //    TODO                        desc->parameters_.bucket_ -= sduSize;
    //                        // update the tracing element accordingly
    //    TODO                    elem->bucket_ = 100.0 /* TODO desc->parameters_.bucket_*/;
                        }
                        availableBytes -= sduSize;

    //
                        // update the tracing element
                        elem->occupancy_ = vQueue->getQueueOccupancy();
                        elem->sentData_ += sduSize;
                        elem->sentSdus_++;

    //
                        EV << NOW << " LteSchedulerUeUl::schedule - Node " << mac_->getMacNodeId() << ",  SDU of size " << sduSize << " selected for transmission" << endl;
                        EV << NOW << " LteSchedulerUeUl::schedule - Node " << mac_->getMacNodeId() << ", remaining grant: " << availableBytes << " bytes" << endl;
                        EV << NOW << " LteSchedulerUeUl::schedule - Node " << mac_->getMacNodeId() << " buffer Size: " << vQueue->getQueueOccupancy() << " bytes" << endl;
                    }
                    else
                    {
    //
                        EV << NOW << " LteSchedulerUeUl::schedule - Node " << mac_->getMacNodeId() << ",  SDU of size " << sduSize << " could not be serviced " << endl;
                        break;// sdu can't be serviced
                    }
                }

                        // check if flow is still backlogged
                if (vQueue->getQueueLength() > 0)
                {
                    // TODO the priority is higher when the associated integer is lower ( e.g. priority 2 is
                    // greater than 4 )
    //
    //                if ( desc->parameters_.priority_ >= lowestBackloggedPriority_ ) {
    //
    //                    if(LteDebug::trace("LteSchedulerUeUl::schedule"))
    //                        fprintf(stderr,"%.9f LteSchedulerUeUl::schedule - Node %d, this flow priority: %u (old lowest priority %u) - LOWEST FOR NOW\n", NOW, nodeId_, desc->parameters_.priority_, lowestBackloggedPriority_);
    //
    //                    // store the new lowest backlogged flow and its priority
    //                    lowestBackloggedFlow_ = fid;
    //                    lowestBackloggedPriority_ = desc->parameters_.priority_;
                }
    //
    //
    //                if ( highestBackloggedPriority_ == -1 || desc->parameters_.priority_ <= highestBackloggedPriority_ ) {
    //
    //                    if(LteDebug::trace("LteSchedulerUeUl::schedule"))
    //                        fprintf(stderr,"%.9f LteSchedulerUeUl::schedule - Node %d, this flow priority: %u (old highest priority %u) - HIGHEST FOR NOW\n", NOW, nodeId_, desc->parameters_.priority_, highestBackloggedPriority_);
    //
    //                    // store the new highest backlogged flow and its priority
    //                    highestBackloggedFlow_ = fid;
    //                    highestBackloggedPriority_ = desc->parameters_.priority_;
    //                }
    //
    //            }
    //
                // update the last schedule time
                lastExecutionTime_ = NOW;

                // signal service for current connection
                unsigned int* servicedSdu = NULL;

                if (scheduleList_.find(cid) == scheduleList_.end())
                {
                    // the element does not exist, initialize it
                    servicedSdu = &scheduleList_[cid];
                    *servicedSdu = elem->sentSdus_;
                }
                else
                {
                    // connection already scheduled during this TTI
                    servicedSdu = &scheduleList_.at(cid);
                }

                // If the end of the connections map is reached and we were on priority and on last traffic class
                if (priorityService && (it == et) && ((i + 1) == (unsigned short) UNKNOWN_TRAFFIC_TYPE))
                {
                    // the first phase of the LCP algorithm has completed ...
                    // ... switch to best effort allocation!
                    priorityService = false;
                    //  reset traffic class
                    i = 0;
                    EV << "LteSchedulerUeUl::schedule - Node" << mac_->getMacNodeId() << ", Starting best effort service" << endl;
                }
            } // END of connections cycle
        } // END of Traffic Classes cycle

        return scheduleList_;
    }
