/*
 * LteUnassistedD2DDRR.cc
 *
 *  Created on: Aug 21, 2017
 *      Author: Sunil Upardrasta
 */

#include <stack/mac/scheduling_modules/unassistedHeuristic/LteUnassistedD2DDRR.h>
#include <stack/mac/scheduler/LteSchedulerUeUnassistedD2D.h>

void LteUnassistedD2DDRR::prepareSchedule() {
    activeTempList_ = activeList_;
    drrTempMap_ = drrMap_;

    bool terminateFlag = false, activeFlag = true, eligibleFlag = true;
    unsigned int eligible = activeTempList_.size();
    // Loop until the active list is not empty and there is spare room.
    while (!activeTempList_.empty() && eligible > 0)
    {
        // Get the current CID.
        MacCid cid = activeTempList_.current();

        MacNodeId nodeId = MacCidToNodeId(cid);

        // check if node is still a valid node in the simulation - might have been dynamically removed
        if(getBinder()->getOmnetId(nodeId) == 0){
            activeTempList_.erase();          // remove from the active list
            activeConnectionTempSet_.erase(cid);
            EV << "CID " << cid << " of node "<< nodeId << " removed from active connection set - no OmnetId in Binder known.";
            continue;
        }

        // Get the current DRR descriptor.
        DrrDesc& desc = drrTempMap_[cid];

        // Check for connection eligibility. If not, skip it.
        if (!desc.eligible_)
        {
            activeTempList_.move();
            eligible--;
            continue;
        }

        // Update the deficit counter.
        desc.deficit_ += desc.quantum_;
        // Clear the flags passed to the grant() function.
        terminateFlag = false;
        activeFlag = true;
        eligibleFlag = true;
        // Try to schedule as many PDUs as possible (or fragments thereof).
        unsigned int scheduled = ueScheduler_->scheduleGrant(cid, desc.deficit_, terminateFlag, activeFlag,
            eligibleFlag);

        if (desc.deficit_ - scheduled < 0)
            throw cRuntimeError("LteUnassistedD2DDRR::execSchedule CID:%d unexpected deficit value of %d", cid, desc.deficit_);

        // Update the deficit counter.
        desc.deficit_ -= scheduled;

        // Update the number of eligible connections.
        if (!eligibleFlag || !activeFlag)
        {
            eligible--;              // decrement the number of eligible conns
            desc.eligible_ = false;  // this queue is not eligible for service
        }

        // Remove the queue if it has become inactiveList_.
        if (!activeFlag)
        {
            activeTempList_.erase();          // remove from the active list
            activeConnectionTempSet_.erase(cid);
            desc.deficit_ = 0;       // reset the deficit to zero
            desc.active_ = false;   // set this descriptor as inactive

            // If scheduling is going to stop and the current queue has not
            // been served entirely, then the RR pointer should NOT move to
            // the next element of the active list. Instead, the deficit
            // is decremented by a quantum, which will be added at
            // the beginning of the next round. Note that this step is only
            // performed if the deficit counter is greater than a quantum so
            // as not to give the queue more bandwidth than its fair share.
        }
        else if (terminateFlag && desc.deficit_ >= desc.quantum_)
        {
            desc.deficit_ -= desc.quantum_;

            // Otherwise, move the round-robin pointer to the next element.
        }
        else
        {
            activeTempList_.move();
        }

        // Terminate scheduling, if the grant function specified so.
        if (terminateFlag)
            break;
    }
}

void LteUnassistedD2DDRR::commitSchedule() {
    activeList_ = activeTempList_;
    activeConnectionSet_ = activeConnectionTempSet_;
    drrMap_ = drrTempMap_;
}

void LteUnassistedD2DDRR::updateSchedulingInfo()
{
    // Get connections.
    LteMacBufferMap* conn;

    if (direction_ == DL)
    {
        conn = ueScheduler_->ueMac_->getMacBuffers();
    }
    else if (direction_ == UL)
    {
        conn = ueScheduler_->ueMac_->getBsrVirtualBuffers();
    }

    //    // Iterators to cycle through the maps of connection descriptors.

    LteMacBufferMap::iterator it = conn->begin(), et = conn->end();

    // Select the minimum rate and MAC SDU size.
    double minSize = 0;
    double minRate = 0;
    for (it = conn->begin(); it != et; ++it)
    {
//            ConnectionParameters& pars = jt->second.parameters_;
        MacCid cid = it->first;
        MacNodeId nodeId = MacCidToNodeId(cid);
        bool eligible = true;
        const UserTxParams& info = ueScheduler_->ueMac_->getAmc()->computeTxParams(nodeId, direction_);
        unsigned int codeword = info.getLayers().size();
        if (ueScheduler_->allocatedCws(nodeId) == codeword)
            eligible = false;

        for (unsigned int i = 0; i < codeword; i++)
        {
            if (info.readCqiVector()[i] == 0)
                eligible = false;
        }
        if (minRate == 0 /* || pars.minReservedRate_ < minRate*/)
//                TODO add connections parameters and fix this value
            minRate = 500;
        if (minSize == 0 /*|| pars.maxBurst_ < minSize */)
            minSize = 160; /*pars.maxBurst_;*/

        // Compute the quanta. If descriptors do not exist they are created.
        // The values of the other fields, e.g. active status, are not changed.

        drrMap_[cid].quantum_ = (unsigned int) (ceil(( /*pars.minReservedRate_*/ 500 / minRate) * minSize));
        drrMap_[cid].eligible_ = eligible;
    }
}

void LteUnassistedD2DDRR::notifyActiveConnection(MacCid cid) {
    EV << NOW << "LteUnassistedD2DDRR::notify CID notified " << cid << endl;
    activeConnectionSet_.insert(cid);
}

void LteUnassistedD2DDRR::removeActiveConnection(MacCid cid) {
    EV << NOW << "LteUnassistedD2DDRR::remove CID removed " << cid << endl;
    activeConnectionSet_.erase(cid);
}
