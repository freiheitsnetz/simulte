/*
K * LteUnassistedD2DMaxCI.cc
 *
 *  Created on: Aug 21, 2017
 *      Author: Sunil Upardrasta
 */

#include <stack/mac/scheduling_modules/unassistedHeuristic/LteUnassistedD2DMaxCI.h>
#include <algorithm>
#include "stack/mac/buffer/LteMacBuffer.h"
#include <stack/mac/scheduler/LteSchedulerUeUnassistedD2D.h>
#include <stack/mac/layer/LteMacUeRealisticD2D.h>

void LteUnassistedD2DMaxCI::prepareSchedule() {
    EV << NOW << " LteUnassistedD2DMaxCI::prepareSchedule "
              << ueScheduler_->ueMac_->getMacNodeId() << endl;

    activeConnectionTempSet_ = activeConnectionSet_;
    int activeSetSize = activeConnectionTempSet_.size();
    ueScheduler_->ueMac_->emit(numActiveConnections_, activeSetSize);


    // Build the score list by cycling through the active connections.
    ScoreList score;
    MacCid cid = 0;
    unsigned int blocks = 0;
    unsigned int byPs = 0;

    for (ActiveSet::iterator it1 = activeConnectionTempSet_.begin();
            it1 != activeConnectionTempSet_.end();) {
        // Current connection.
        cid = *it1;

        ++it1;

        MacNodeId nodeId = MacCidToNodeId(cid);
        OmnetId id = getBinder()->getOmnetId(nodeId);
        if (nodeId == 0 || id == 0) {
            // node has left the simulation - erase corresponding CIDs
            activeConnectionSet_.erase(cid);
            activeConnectionTempSet_.erase(cid);
            continue;
        }

        // if we are allocating the UL subframe, this connection may be either UL or D2D
        Direction dir;
        if (direction_ == UL)
            dir = (MacCidToLcid(cid) == D2D_SHORT_BSR) ? D2D : direction_;
        else
            dir = DL;

        // compute available blocks for the current user
        const UserTxParams& info =
                ueScheduler_->ueMac_->getAmc()->computeTxParams(nodeId, dir);
        const std::set<Band>& bands = info.readBands();
        std::set<Band>::const_iterator it = bands.begin(), et = bands.end();
        unsigned int codeword = info.getLayers().size();
        bool cqiNull = false;
        for (unsigned int i = 0; i < codeword; i++) {
            if (info.readCqiVector()[i] == 0)
                cqiNull = true;
        }
        if (cqiNull)
            continue;
        //no more free cw
        if (ueScheduler_->allocatedCws(nodeId) == codeword)
            continue;

        std::set<Remote>::iterator antennaIt = info.readAntennaSet().begin(),
                antennaEt = info.readAntennaSet().end();

        // compute score based on total available bytes
        unsigned int availableBlocks = 0;
        unsigned int availableBytes = 0;
        // for each antenna
        for (; antennaIt != antennaEt; ++antennaIt) {
            // for each logical band
            for (; it != et; ++it) {
                availableBlocks += ueScheduler_->readAvailableRbs(nodeId,
                        *antennaIt, *it);
                availableBytes +=
                        ueScheduler_->ueMac_->getAmc()->computeBytesOnNRbs(
                                nodeId, *it, availableBlocks, dir);
            }
        }

        blocks = availableBlocks;
        // current user bytes per slot
        byPs = (blocks > 0) ? (availableBytes / blocks) : 0;

        // Create a new score descriptor for the connection, where the score is equal to the bytes per slot i.e. the Short term data rate
        ScoreDesc desc(cid, byPs);
        // insert the cid score
        score.push(desc);

        EV << NOW
                  << " LteUnassistedD2DMaxCI::prepareSchedule computed for cid "
                  << cid << " score of " << desc.score_ << endl;
    }

    // Schedule the connections in score order.
    while (!score.empty()) {
        // Pop the top connection from the list.
        ScoreDesc current = score.top();

        EV << NOW
                  << " LteUnassistedD2DMaxCI::prepareSchedule scheduling connection "
                  << current.x_ << " with score of " << current.score_ << endl;

        // Grant data to that connection.
        bool terminate = false;
        bool active = true;
        bool eligible = true;
        unsigned int granted = ueScheduler_->scheduleGrant(current.x_, 4294967295U, terminate,
                active, eligible);

        EV << NOW << "LteUnassistedD2DMaxCI::prepareSchedule granted "
                  << granted << " bytes to connection " << current.x_ << endl;

        // Exit immediately if the terminate flag is set.
        if (terminate)
            break;

        // Pop the descriptor from the score list if the active or eligible flag are clear.
        if (!active || !eligible) {
            score.pop();
            EV << NOW << "LteUnassistedD2DMaxCI::prepareSchedule  connection "
                      << current.x_ << " was found ineligible" << endl;
        }

        // Set the connection as inactive if indicated by the grant ().
        if (!active) {
            EV << NOW
                      << "LteUnassistedD2DMaxCI::prepareSchedule scheduling connection "
                      << current.x_ << " set to inactive " << endl;

            activeConnectionTempSet_.erase(current.x_);
        }
    }
}

void LteUnassistedD2DMaxCI::commitSchedule() {
    EV_STATICCONTEXT
    ;
    EV << NOW << " LteUnassistedD2DMaxCI::commitSchedule" << std::endl;
    activeConnectionSet_ = activeConnectionTempSet_;
}

void LteUnassistedD2DMaxCI::updateSchedulingInfo()
{
}

void LteUnassistedD2DMaxCI::notifyActiveConnection(MacCid cid) {
    EV << NOW << "LteUnassistedD2DMaxCI::notify CID notified " << cid << endl;
    activeConnectionSet_.insert(cid);
}

void LteUnassistedD2DMaxCI::removeActiveConnection(MacCid cid) {
    EV << NOW << "LteUnassistedD2DMaxCI::remove CID removed " << cid << endl;
    activeConnectionSet_.erase(cid);
}

