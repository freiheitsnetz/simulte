/*
 * LteUnassistedD2DPF.cc
 *
 *  Created on: Aug 21, 2017
 *      Author: Sunil Upardrasta
 */

#include <stack/mac/scheduling_modules/unassistedHeuristic/LteUnassistedD2DPF.h>
#include <algorithm>
#include "stack/mac/buffer/LteMacBuffer.h"
#include <stack/mac/scheduler/LteSchedulerUeUnassistedD2D.h>
#include <stack/mac/layer/LteMacUeRealisticD2D.h>


void LteUnassistedD2DPF::prepareSchedule() {
    EV << NOW << "LteUnassistedD2DPF::execSchedule ############### eNodeB " << ueScheduler_->ueMac_->getMacNodeId() << " ###############" << endl;
    EV << NOW << "LteUnassistedD2DPF::execSchedule Direction: " << ( ( direction_ == DL ) ? " DL ": " UL ") << endl;

    // Clear structures
    grantedBytes_.clear();

    // Create a working copy of the active set
    activeConnectionTempSet_ = activeConnectionSet_;
    int activeSetSize = activeConnectionTempSet_.size();
    ueScheduler_->ueMac_->emit(numActiveConnections_, activeSetSize);

    // Build the score list by cycling through the active connections.
    ScoreList score;

    ActiveSet::iterator cidIt = activeConnectionTempSet_.begin();
    ActiveSet::iterator cidEt = activeConnectionTempSet_.end();

    for(; cidIt != cidEt; )
    {
        MacCid cid = *cidIt;
        ++cidIt;
        MacNodeId nodeId = MacCidToNodeId(cid);

        // if we are allocating the UL subframe, this connection may be either UL or D2D
        Direction dir;
        if (direction_ == UL)
            dir = (MacCidToLcid(cid) == D2D_SHORT_BSR) ? D2D : direction_;
        else
            dir = DL;

        // check if node is still a valid node in the simulation - might have been dynamically removed
        if(getBinder()->getOmnetId(nodeId) == 0){
            activeConnectionTempSet_.erase(cid);
            EV << "CID " << cid << " of node "<< nodeId << " removed from active connection set - no OmnetId in Binder known.";
            continue;
        }

        // compute available blocks for the current user
        const UserTxParams& info = ueScheduler_->ueMac_->getAmc()->computeTxParams(nodeId,dir);
        const std::set<Band>& bands = info.readBands();
        unsigned int codeword=info.getLayers().size();
        if (ueScheduler_->allocatedCws(nodeId)==codeword)
        continue;
        std::set<Band>::const_iterator it = bands.begin(),et=bands.end();

        std::set<Remote>::iterator antennaIt = info.readAntennaSet().begin(), antennaEt=info.readAntennaSet().end();

        bool cqiNull=false;
        for (unsigned int i=0;i<codeword;i++)
        {
            if (info.readCqiVector()[i]==0)
            cqiNull=true;
        }
        if (cqiNull)
        continue;
        // compute score based on total available bytes
        unsigned int availableBlocks=0;
        unsigned int availableBytes =0;
        // for each antenna
        for (;antennaIt!=antennaEt;++antennaIt)
        {
            // for each logical band
            for (;it!=et;++it)
            {
                availableBlocks += ueScheduler_->readAvailableRbs(nodeId,*antennaIt,*it);
                availableBytes += ueScheduler_->ueMac_->getAmc()->computeBytesOnNRbs(nodeId,*it, availableBlocks, dir);
            }
        }

        double s=.0;

        if (pfRate_.find(cid)==pfRate_.end()) pfRate_[cid]=0;
        //pfRate_[cid] is the long term rate or historical average data rate of the connection
        // availableBytes / availableBlocks is the short term data rate
        if(pfRate_[cid] < scoreEpsilon_) s = 1.0 / scoreEpsilon_;
        else if(availableBlocks > 0) s = ((availableBytes / availableBlocks) / pfRate_[cid]) + uniform(getEnvir()->getRNG(0),-scoreEpsilon_/2.0, scoreEpsilon_/2.0);
        else s = 0.0;

        // Create a new score descriptor for the connection, where the score is equal to the ratio between bytes per slot and long term rate
        ScoreDesc desc(cid,s);
        score.push(desc);

        EV << NOW << "LteUnassistedD2DPF::execSchedule CID " << cid << "- Score = " << s << endl;
    }

    // Schedule the connections in score order.
    while(!score.empty())
    {
        // Pop the top connection from the list.
        ScoreDesc current = score.top();
        MacCid cid = current.x_;// The CID

        EV << NOW << "LteUnassistedD2DPF::execSchedule @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" << endl;
        EV << NOW << "LteUnassistedD2DPF::execSchedule CID: " << cid;
        EV << NOW << "LteUnassistedD2DPF::execSchedule Score: " << current.score_ << endl;

        // Grant data to that connection.
        bool terminate = false;
        bool active = true;
        bool eligible = true;

        unsigned int granted = ueScheduler_->scheduleGrant(cid, 4294967295U, terminate, active, eligible);
        grantedBytes_[cid] += granted;

        EV << NOW << "LteUnassistedD2DPF::execSchedule Granted: " << granted << " bytes" << endl;

        // Exit immediately if the terminate flag is set.
        if(terminate)
        {
            EV << NOW << "LteUnassistedD2DPF::execSchedule TERMINATE " << endl;
            break;
        }

        // Pop the descriptor from the score list if the active or eligible flag are clear.
        if(!active || !eligible)
        {
            score.pop ();

            if(!eligible)
            EV << NOW << "LteUnassistedD2DPF::execSchedule NOT ELIGIBLE " << endl;
        }

        // Set the connection as inactive if indicated by the grant ().
        if(!active)
        {
            EV << NOW << "LteUnassistedD2DPF::execSchedule NOT ACTIVE" << endl;
            activeConnectionTempSet_.erase (current.x_);
        }
    }
}

void LteUnassistedD2DPF::commitSchedule() {
    unsigned int total = ueScheduler_->resourceBlocks_;

    std::map<MacCid, unsigned int>::iterator it = grantedBytes_.begin();
    std::map<MacCid, unsigned int>::iterator et = grantedBytes_.end();

    for (; it != et; ++it)
    {
        MacCid cid = it->first;
        unsigned int granted = it->second;

        EV << NOW << " LteUnassistedD2DPF::storeSchedule @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" << endl;
        EV << NOW << " LteUnassistedD2DPF::storeSchedule CID: " << cid << endl;
        EV << NOW << " LteUnassistedD2DPF::storeSchedule Direction: " << ((direction_ == DL) ? "DL": "UL" ) << endl;

        // Computing the short term rate
        double shortTermRate;

        if (total > 0)
            shortTermRate = double(granted) / double(total);
        else
            shortTermRate = 0.0;

        EV << NOW << " LteUnassistedD2DPF::storeSchedule Short Term Rate " << shortTermRate << endl;

        // Updating the long term rate
        double& longTermRate = pfRate_[cid];
        longTermRate = (1.0 - pfAlpha_) * longTermRate + pfAlpha_ * shortTermRate;

        EV << NOW << "LteUnassistedD2DPF::storeSchedule Long Term Rate = " << longTermRate;
    }

    activeConnectionSet_ = activeConnectionTempSet_;
}

void LteUnassistedD2DPF::notifyActiveConnection(MacCid cid) {
    EV << NOW << "LteUnassistedD2DPF::notify CID notified " << cid << endl;
    activeConnectionSet_.insert(cid);
}

void LteUnassistedD2DPF::removeActiveConnection(MacCid cid) {
    EV << NOW << "LteUnassistedD2DPF::remove CID removed " << cid << endl;
    activeConnectionSet_.erase(cid);
}

void LteUnassistedD2DPF::updateSchedulingInfo()
{
}
