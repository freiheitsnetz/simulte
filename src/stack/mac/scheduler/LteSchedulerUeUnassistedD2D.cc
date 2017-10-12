//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "stack/mac/scheduler/LteSchedulerUeUnassistedD2D.h"

#include "../scheduling_modules/unassistedHeuristic/LteUnassistedD2DSchedulingAgent.h"
#include "stack/mac/layer/LteMacEnb.h"
#include "stack/mac/layer/LteMacUeRealisticD2D.h"
#include "stack/mac/scheduler/LteScheduler.h"
#include "stack/mac/allocator/LteAllocationModule.h"
#include "stack/mac/packet/LteSchedulingGrant.h"
#include "stack/mac/scheduler/LcgScheduler.h"
#include "stack/mac/buffer/LteMacBuffer.h"
#include "stack/mac/buffer/harq/LteHarqBufferRx.h"
#include "stack/mac/buffer/harq_d2d/LteHarqBufferRxD2DMirror.h"
#include "stack/mac/scheduling_modules/LteDrr.h"
#include "stack/mac/scheduling_modules/LteMaxCi.h"
#include "stack/mac/scheduling_modules/LtePf.h"
#include "stack/mac/scheduling_modules/LteMaxCiMultiband.h"
#include "stack/mac/scheduling_modules/LteMaxCiOptMB.h"
#include "stack/mac/scheduling_modules/LteMaxCiComp.h"
#include "stack/mac/scheduling_modules/unassistedHeuristic/LteUnassistedD2DPF.h"
#include "stack/mac/scheduling_modules/unassistedHeuristic/LteUnassistedD2DDRR.h"
#include "stack/mac/scheduling_modules/unassistedHeuristic/LteUnassistedD2DMaxCI.h"

LteSchedulerUeUnassistedD2D::LteSchedulerUeUnassistedD2D(LteMacUeRealisticD2D * mac) {
    ueMac_ = mac;
    lteUnassistedD2DSchedulingAgent_ = 0;
    // Initialization of an Unassisted D2D UE's own resources has not been implemented
    allocator_ = 0;
    //    vbuf_ = 0;
    harqTxBuffers_ = 0;
    harqRxBuffers_ = 0;
    resourceBlocks_ = 0;
}

LteSchedulerUeUnassistedD2D::~LteSchedulerUeUnassistedD2D() {
    delete tSample_;
    delete allocator_;
    delete lteUnassistedD2DSchedulingAgent_;
}

void LteSchedulerUeUnassistedD2D::initialize(Direction dir,
        LteMacUeRealisticD2D* mac) {

    direction_ = dir;
    ueMac_ = mac;

    vbuf_ = ueMac_->getMacBuffers();
    bsrbuf_ = ueMac_->getBsrVirtualBuffers();

    harqTxBuffers_ = ueMac_->getHarqTxBuffers();
    harqRxBuffers_ = ueMac_->getHarqRxBuffers();

    // Create Allocator
    allocator_ = new LteAllocationModule(ueMac_, direction_);

    // Create LteScheduler
    SchedDiscipline discipline = ueMac_->getSchedDiscipline();

    lteUnassistedD2DSchedulingAgent_ = getScheduler(discipline,this);

//        lteUnassistedD2DSchedulingAgent_ = new LteUnassistedD2DSchedulingAgent(this); // To DO for multiple scheduling algorithms

    if (direction_ == DL) {
        grantTypeMap_[CONVERSATIONAL] = aToGrantType(
                ueMac_->par("grantTypeConversationalDl"));
        grantTypeMap_[STREAMING] = aToGrantType(
                ueMac_->par("grantTypeStreamingDl"));
        grantTypeMap_[INTERACTIVE] = aToGrantType(
                ueMac_->par("grantTypeInteractiveDl"));
        grantTypeMap_[BACKGROUND] = aToGrantType(
                ueMac_->par("grantTypeBackgroundDl"));

        grantSizeMap_[CONVERSATIONAL] = ueMac_->par(
                "grantSizeConversationalDl");
        grantSizeMap_[STREAMING] = ueMac_->par("grantSizeStreamingDl");
        grantSizeMap_[INTERACTIVE] = ueMac_->par("grantSizeInteractiveDl");
        grantSizeMap_[BACKGROUND] = ueMac_->par("grantSizeBackgroundDl");
    } else if (direction_ == UL) {
        grantTypeMap_[CONVERSATIONAL] = aToGrantType(
                ueMac_->par("grantTypeConversationalUl"));
        grantTypeMap_[STREAMING] = aToGrantType(
                ueMac_->par("grantTypeStreamingUl"));
        grantTypeMap_[INTERACTIVE] = aToGrantType(
                ueMac_->par("grantTypeInteractiveUl"));
        grantTypeMap_[BACKGROUND] = aToGrantType(
                ueMac_->par("grantTypeBackgroundUl"));

        grantSizeMap_[CONVERSATIONAL] = ueMac_->par(
                "grantSizeConversationalUl");
        grantSizeMap_[STREAMING] = ueMac_->par("grantSizeStreamingUl");
        grantSizeMap_[INTERACTIVE] = ueMac_->par("grantSizeInteractiveUl");
        grantSizeMap_[BACKGROUND] = ueMac_->par("grantSizeBackgroundUl");
    } else {
        throw cRuntimeError("Unknown direction %d", direction_);
    }

    // Initialize statistics
    cellBlocksUtilizationDl_ = ueMac_->registerSignal(
            "cellBlocksUtilizationDl");
    cellBlocksUtilizationUl_ = ueMac_->registerSignal(
            "cellBlocksUtilizationUl");
    lteAvgServedBlocksDl_ = ueMac_->registerSignal("avgServedBlocksDl");
    lteAvgServedBlocksUl_ = ueMac_->registerSignal("avgServedBlocksUl");
    depletedPowerDl_ = ueMac_->registerSignal("depletedPowerDl");
    depletedPowerUl_ = ueMac_->registerSignal("depletedPowerUl");

    prf_0 = ueMac_->registerSignal("prf_0");
    prf_1a = ueMac_->registerSignal("prf_1a");
    prf_2a = ueMac_->registerSignal("prf_2a");
    prf_3a = ueMac_->registerSignal("prf_3a");
    prf_1b = ueMac_->registerSignal("prf_1b");
    prf_2b = ueMac_->registerSignal("prf_2b");
    prf_3b = ueMac_->registerSignal("prf_3b");
    prf_1c = ueMac_->registerSignal("prf_1c");
    prf_2c = ueMac_->registerSignal("prf_2c");
    prf_3c = ueMac_->registerSignal("prf_3c");
    prf_4 = ueMac_->registerSignal("prf_4");
    prf_5 = ueMac_->registerSignal("prf_5");
    prf_6a = ueMac_->registerSignal("prf_6a");
    prf_7a = ueMac_->registerSignal("prf_7a");
    prf_8a = ueMac_->registerSignal("prf_8a");
    prf_6b = ueMac_->registerSignal("prf_6b");
    prf_7b = ueMac_->registerSignal("prf_7b");
    prf_8b = ueMac_->registerSignal("prf_8b");
    prf_6c = ueMac_->registerSignal("prf_6c");
    prf_7c = ueMac_->registerSignal("prf_7c");
    prf_8c = ueMac_->registerSignal("prf_8c");
    prf_9 = ueMac_->registerSignal("prf_9");

    rb_0 = ueMac_->registerSignal("rb_0");
    rb_1a = ueMac_->registerSignal("rb_1a");
    rb_2a = ueMac_->registerSignal("rb_2a");
    rb_3a = ueMac_->registerSignal("rb_3a");
    rb_1b = ueMac_->registerSignal("rb_1b");
    rb_2b = ueMac_->registerSignal("rb_2b");
    rb_3b = ueMac_->registerSignal("rb_3b");
    rb_1c = ueMac_->registerSignal("rb_1c");
    rb_2c = ueMac_->registerSignal("rb_2c");
    rb_3c = ueMac_->registerSignal("rb_3c");
    rb_4 = ueMac_->registerSignal("rb_4");
    rb_5 = ueMac_->registerSignal("rb_5");
    rb_6a = ueMac_->registerSignal("rb_6a");
    rb_7a = ueMac_->registerSignal("rb_7a");
    rb_8a = ueMac_->registerSignal("rb_8a");
    rb_6b = ueMac_->registerSignal("rb_6b");
    rb_7b = ueMac_->registerSignal("rb_7b");
    rb_8b = ueMac_->registerSignal("rb_8b");
    rb_6c = ueMac_->registerSignal("rb_6c");
    rb_7c = ueMac_->registerSignal("rb_7c");
    rb_8c = ueMac_->registerSignal("rb_8c");
    rb_9 = ueMac_->registerSignal("rb_9");

    tSample_ = new TaggedSample();

    tSample_->module_ = check_and_cast<cComponent*>(ueMac_);

}

//SL-Rx Scheduling of Tx D2D done by Rx D2D UE
LteMacScheduleList* LteSchedulerUeUnassistedD2D::scheduleTxD2Ds() {
    // 1) Environment Setup
    EV << "LteSchedulerUeUnassistedD2D::scheduleTxD2Ds performed by Node: "
              << ueMac_->getMacNodeId() << endl;
    // clean up old scheduling decisions
    scheduleList_.clear();
    //Cleaning required ??
    allocatedCws_.clear();
    //Init and reset is not required
    allocator_->initAndReset(resourceBlocks_,
            ((ueMac_->getENodeB())->getDeployer())->getNumBands());
    //reset AMC structures
    ueMac_->getAmc()->cleanAmcStructures(direction_,
            lteUnassistedD2DSchedulingAgent_->readActiveSet());
    EV
              << "___________________________start RTX __________________________________"
              << endl;
    if (!(rtxschedule())) {
        EV
                  << "____________________________ end RTX __________________________________"
                  << endl;
        EV
                  << "___________________________start SCHED ________________________________"
                  << endl;
        lteUnassistedD2DSchedulingAgent_->updateSchedulingInfo();
        lteUnassistedD2DSchedulingAgent_->scheduleTxD2Ds();
        EV
                  << "____________________________ end SCHED ________________________________"
                  << endl;
    }

    // record assigned resource blocks statistics
    resourceBlockStatistics();
    return &scheduleList_;
}

unsigned int LteSchedulerUeUnassistedD2D::schedulePerAcidRtx(MacNodeId nodeId,
        Codeword cw, unsigned char acid, // needs to be modified for Direct HARQ
        std::vector<BandLimit>* bandLim, Remote antenna, bool limitBl) {
    try
    {
        std::string bands_msg = "BAND_LIMIT_SPECIFIED";
        if (bandLim == NULL)
        {
            bands_msg = "NO_BAND_SPECIFIED";
            // Create a vector of band limit using all bands
            // FIXME: bandlim is never deleted
            bandLim = new std::vector<BandLimit>();

            unsigned int numBands = ueMac_->getDeployer()->getNumBands();
            // for each band of the band vector provided
            for (unsigned int i = 0; i < numBands; i++)
            {
                BandLimit elem;
                // copy the band
                elem.band_ = Band(i);
                EV << "Putting band " << i << endl;
                // mark as unlimited
                for (Codeword i = 0; i < MAX_CODEWORDS; ++i)
                {
                    elem.limit_.push_back(-1);
                }
                bandLim->push_back(elem);
            }
        }

        EV << NOW << "LteSchedulerUeUnassistedD2D::schedulePerAcidRtx - Node[" << ueMac_->getMacNodeId() << ", User[" << nodeId << ", Codeword[ " << cw << "], ACID[" << (unsigned int)acid << "] " << endl;

        // Get the current active HARQ process
//        unsigned char currentAcid = harqStatus_.at(nodeId) ;

        unsigned char currentAcid = (harqStatus_.at(nodeId) + 2) % (harqRxBuffers_->at(nodeId)->getProcesses());
        EV << "\t the acid that should be considered is " << currentAcid << endl;

        // acid e currentAcid sono identici per forza, dato che sono calcolati nello stesso modo
//        if(acid != currentAcid)
//        {        // If requested HARQ process is not current for TTI
//            EV << NOW << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtx User is on ACID " << (unsigned int)currentAcid << " while requested one is " << (unsigned int)acid << ". No RTX scheduled. " << endl;
//            return 0;
//        }

        LteHarqProcessRx* currentProcess = harqRxBuffers_->at(nodeId)->getProcess(currentAcid);

        if (currentProcess->getUnitStatus(cw) != RXHARQ_PDU_CORRUPTED)
        {
            // exit if the current active HARQ process is not ready for retransmission
            EV << NOW << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtx User is on ACID " << (unsigned int)currentAcid << " HARQ process is IDLE. No RTX scheduled ." << endl;
            delete(bandLim);
            return 0;
        }

        Codeword allocatedCw = 0;
//        Codeword allocatedCw = MAX_CODEWORDS;
        // search for already allocated codeword
        // create "mirror" scList ID for other codeword than current
        std::pair<unsigned int, Codeword> scListMirrorId = std::pair<unsigned int, Codeword>(idToMacCid(nodeId,SHORT_BSR), MAX_CODEWORDS - cw - 1);
        if (scheduleList_.find(scListMirrorId) != scheduleList_.end())
        {
            allocatedCw = MAX_CODEWORDS - cw - 1;
        }
        // get current process buffered PDU byte length
        unsigned int bytes = currentProcess->getByteLength(cw);
        // bytes to serve
        unsigned int toServe = bytes;
        // blocks to allocate for each band
        std::vector<unsigned int> assignedBlocks;
        // bytes which blocks from the preceding vector are supposed to satisfy
        std::vector<unsigned int> assignedBytes;

        // end loop signal [same as bytes>0, but more secure]
        bool finish = false;
        // for each band
        unsigned int size = bandLim->size();
        for (unsigned int i = 0; (i < size) && (!finish); ++i)
        {
            // save the band and the relative limit
            Band b = bandLim->at(i).band_;
            int limit = bandLim->at(i).limit_.at(cw);

            // TODO add support to multi CW
//            unsigned int bandAvailableBytes = // if a codeword has been already scheduled for retransmission, limit available blocks to what's been  allocated on that codeword
//                    ((allocatedCw == MAX_CODEWORDS) ? availableBytes(nodeId,antenna, b, cw) : ueMac_->getAmc()->blocks2bytes(nodeId, b, cw, allocator_->getBlocks(antenna,b,nodeId) , direction_));    // available space
            unsigned int bandAvailableBytes = availableBytes(nodeId, antenna, b, cw, direction_);

            // use the provided limit as cap for available bytes, if it is not set to unlimited
            if (limit >= 0)
                bandAvailableBytes = limit < (int) bandAvailableBytes ? limit : bandAvailableBytes;

            EV << NOW << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtx BAND " << b << endl;
            EV << NOW << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtx total bytes:" << bytes << " still to serve: " << toServe << " bytes" << endl;
            EV << NOW << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtx Available: " << bandAvailableBytes << " bytes" << endl;

            unsigned int servedBytes = 0;
            // there's no room on current band for serving the entire request
            if (bandAvailableBytes < toServe)
            {
                // record the amount of served bytes
                servedBytes = bandAvailableBytes;
                // the request can be fully satisfied
            }
            else
            {
                // record the amount of served bytes
                servedBytes = toServe;
                // signal end loop - all data have been serviced
                finish = true;
            }
            unsigned int servedBlocks = ueMac_->getAmc()->computeReqRbs(nodeId, b, cw, servedBytes, direction_);
            // update the bytes counter
            toServe -= servedBytes;
            // update the structures
            assignedBlocks.push_back(servedBlocks);
            assignedBytes.push_back(servedBytes);
        }

        if (toServe > 0)
        {
            // process couldn't be served - no sufficient space on available bands
            EV << NOW << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtx Unavailable space for serving node " << nodeId << " ,HARQ Process " << (unsigned int)currentAcid << " on codeword " << cw << endl;
            delete(bandLim);
            return 0;
        }
        else
        {
            // record the allocation
            unsigned int size = assignedBlocks.size();
            unsigned int cwAllocatedBlocks =0;

            // create scList id for current node/codeword
            std::pair<unsigned int,Codeword> scListId = std::pair<unsigned int,Codeword>(idToMacCid(nodeId, SHORT_BSR), cw);

            for(unsigned int i = 0; i < size; ++i)
            {
                // For each LB for which blocks have been allocated
                Band b = bandLim->at(i).band_;

                cwAllocatedBlocks +=assignedBlocks.at(i);
                EV << "\t Cw->" << allocatedCw << "/" << MAX_CODEWORDS << endl;
                //! handle multi-codeword allocation
                if (allocatedCw!=MAX_CODEWORDS)
                {
                    EV << NOW << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtx - adding " << assignedBlocks.at(i) << " to band " << i << endl;
                    allocator_->addBlocks(antenna,b,nodeId,assignedBlocks.at(i),assignedBytes.at(i));
                }
                //! TODO check if ok bandLim->at.limit_.at(cw) = assignedBytes.at(i);
            }

            // signal a retransmission
            // schedule list contains number of granted blocks

            scheduleList_[scListId] = cwAllocatedBlocks;
            // mark codeword as used
            if (allocatedCws_.find(nodeId)!=allocatedCws_.end())
            {
                allocatedCws_.at(nodeId)++;
            }
            else
            {
                allocatedCws_[nodeId]=1;
            }

            EV << NOW << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtx HARQ Process " << (unsigned int)currentAcid << " : " << bytes << " bytes served! " << endl;

            delete(bandLim);
            return bytes;
        }
    }
    catch(std::exception& e)
    {
        throw cRuntimeError("Exception in LteSchedulerUeUnassistedD2D::schedulePerAcidRtx(): %s", e.what());
    }
    delete(bandLim);
    return 0;
}

// To be used by Rx D2D UE
bool LteSchedulerUeUnassistedD2D::rtxschedule() {
    // Check LteSchedulerEnbUl::rtxschedule()
    // try to handle RAC requests first and abort rtx scheduling if no OFDMA space is left after
    if (racschedule())
        return true;
    try {
        EV << NOW
                  << " LteSchedulerUeUnassistedD2D::rtxschedule --------------------::[ START RTX-SCHEDULE ]::--------------------"
                  << endl;
        EV << NOW << " LteSchedulerUeUnassistedD2D::rtxschedule for Rx mode of D2D UE: "
                  << ueMac_->getMacNodeId() << endl;

        HarqRxBuffers::iterator it = harqRxBuffers_->begin(), et =
                harqRxBuffers_->end();
        for (; it != et; ++it) {
            // get current nodeId
            MacNodeId nodeId = it->first;

            if (nodeId == 0) {
                //Rx D2D UE has left the simulation - erase queue and continue
                harqRxBuffers_->erase(nodeId);
                continue;
            }
            OmnetId id = getBinder()->getOmnetId(nodeId);
            if (id == 0) {
                harqRxBuffers_->erase(nodeId);
                continue;
            }

            // get current Harq Process for nodeId
            unsigned char currentAcid = harqStatus_.at(nodeId);

            // check whether the UE has a H-ARQ process waiting for retransmission. If not, skip UE.
            bool skip = true;
            unsigned char acid = (currentAcid + 2)
                    % (it->second->getProcesses());
            LteHarqProcessRx* currentProcess = it->second->getProcess(acid);
            std::vector<RxUnitStatus> procStatus =
                    currentProcess->getProcessStatus();
            std::vector<RxUnitStatus>::iterator pit = procStatus.begin();
            for (; pit != procStatus.end(); ++pit) {
                if (pit->second == RXHARQ_PDU_CORRUPTED) {
                    skip = false;
                    break;
                }
            }
            if (skip)
                continue;

            EV << NOW << "LteSchedulerUeUnassistedD2D::rtxschedule UE: " << nodeId
                      << "Acid: " << (unsigned int) currentAcid << endl;

            // Get user transmission parameters
            const UserTxParams& txParams = ueMac_->getAmc()->computeTxParams(
                    nodeId, direction_);    // get the user info

            unsigned int codewords = txParams.getLayers().size(); // get the number of available codewords
            unsigned int allocatedBytes = 0;

            // TODO handle the codewords join case (sizeof(cw0+cw1) < currentTbs && currentLayers ==1)

            for (Codeword cw = 0; (cw < MAX_CODEWORDS) && (codewords > 0);
                    ++cw) {
                unsigned int rtxBytes = 0;
                // FIXME PERFORMANCE: check for rtx status before calling rtxAcid

                // perform a retransmission on available codewords for the selected acid
                rtxBytes = LteSchedulerUeUnassistedD2D::schedulePerAcidRtx(nodeId, cw,
                        currentAcid);
                if (rtxBytes > 0) {
                    --codewords;
                    allocatedBytes += rtxBytes;
                }
            }
            EV << NOW << "LteSchedulerUeUnassistedD2D::rtxschedule user " << nodeId
                      << " allocated bytes : " << allocatedBytes << endl;
        }
        if (ueMac_->isD2DCapable()) {
            // --- START Schedule D2D retransmissions --- //
            Direction dir = D2D;
            HarqRxBuffersMirror* harqRxBuffersD2DMirror;
            if (strcmp(ueMac_->getClassName(), "LteMacUeRealisticD2D") == 0)
                harqRxBuffersD2DMirror = check_and_cast<LteMacUeRealisticD2D*>(
                        ueMac_)->getRxHarqBufferMirror();
            else
                throw cRuntimeError(
                        "LteSchedulerUeUnassistedD2D::rtxschedule - unrecognized MAC type %s",
                        ueMac_->getClassName());
            HarqRxBuffersMirror::iterator it_d2d =
                    harqRxBuffersD2DMirror->begin(), et_d2d =
                    harqRxBuffersD2DMirror->end();
            for (; it_d2d != et_d2d; ++it_d2d) {

                // get current nodeIDs
                MacNodeId senderId = it_d2d->second->peerId_; // Transmitter
                MacNodeId destId = it_d2d->first;             // Receiver

                // get current Harq Process for nodeId
                unsigned char currentAcid = harqStatus_.at(senderId);

                // check whether the UE has a H-ARQ process waiting for retransmission. If not, skip UE.
                bool skip = true;
                unsigned char acid = (currentAcid + 2)
                        % (it_d2d->second->getProcesses());
                LteHarqProcessRxD2DMirror* currentProcess =
                        it_d2d->second->getProcess(acid);
                std::vector<RxUnitStatus> procStatus =
                        currentProcess->getProcessStatus();
                std::vector<RxUnitStatus>::iterator pit = procStatus.begin();
                for (; pit != procStatus.end(); ++pit) {
                    if (pit->second == RXHARQ_PDU_CORRUPTED) {
                        skip = false;
                        break;
                    }
                }
                if (skip)
                    continue;

                EV << NOW << " LteSchedulerUeUnassistedD2D::rtxschedule - D2D UE: "
                          << senderId << " Acid: " << (unsigned int) currentAcid
                          << endl;

                // Get user transmission parameters
                const UserTxParams& txParams =
                        ueMac_->getAmc()->computeTxParams(senderId, dir); // get the user info

                unsigned int codewords = txParams.getLayers().size(); // get the number of available codewords
                unsigned int allocatedBytes = 0;

                // TODO handle the codewords join case (size of(cw0+cw1) < currentTbs && currentLayers ==1)

                for (Codeword cw = 0; (cw < MAX_CODEWORDS) && (codewords > 0);
                        ++cw) {
                    unsigned int rtxBytes = 0;
                    // FIXME PERFORMANCE: check for rtx status before calling rtxAcid

                    // perform a retransmission on available codewords for the selected acid
                    rtxBytes = LteSchedulerUeUnassistedD2D::schedulePerAcidRtxD2D(
                            destId, senderId, cw, currentAcid);
                    if (rtxBytes > 0) {
                        --codewords;
                        allocatedBytes += rtxBytes;
                    }
                }
                EV << NOW << " LteSchedulerUeUnassistedD2D::rtxschedule - D2D UE: "
                          << senderId << " allocated bytes : " << allocatedBytes
                          << endl;

            }
            // --- END Schedule D2D retransmissions --- //
        }
        int availableBlocks = allocator_->computeTotalRbs();
        EV << NOW << " LteSchedulerUeUnassistedD2D::rtxschedule residual OFDM Space: "
                  << availableBlocks << endl;
        EV << NOW
                  << " LteSchedulerUeUnassistedD2D::rtxschedule --------------------::[  END RTX-SCHEDULE  ]::--------------------"
                  << endl;
        return (availableBlocks == 0);
    } catch (std::exception& e) {
        throw cRuntimeError(
                "Exception in LteSchedulerUeUnassistedD2D::rtxschedule(): %s",
                e.what());
    }
    return 0;
}

unsigned int LteSchedulerUeUnassistedD2D::scheduleGrant(MacCid cid,
        unsigned int bytes, bool& terminate, bool& active, bool& eligible,
        std::vector<BandLimit>* bandLim, Remote antenna, bool limitBl) {
    // Get the node ID and logical connection ID
    MacNodeId nodeId = MacCidToNodeId(cid);
    LogicalCid flowId = MacCidToLcid(cid);

//    direction_ = UL; // To handle Unassisted D2D direction of both UL iand DL is out from the UE and so is same
    Direction dir = direction_;
    if (dir == UL)
    {
        // check if this connection is a D2D connection
        if (flowId == D2D_SHORT_BSR)
            dir = D2D;           // if yes, change direction
        if (flowId == D2D_MULTI_SHORT_BSR)
            dir = D2D_MULTI;     // if yes, change direction
    }

    // Get user transmission parameters
    const UserTxParams& txParams = ueMac_->getAmc()->computeTxParams(nodeId, dir);
    //get the number of codewords
    unsigned int numCodewords = txParams.getLayers().size();

    // TEST: check the number of codewords
    numCodewords = 1;

    std::string bands_msg = "BAND_LIMIT_SPECIFIED";

    std::vector<BandLimit> tempBandLim;

    if (bandLim == NULL)
    {
        bands_msg = "NO_BAND_SPECIFIED";
        // Create a vector of band limit using all bands
        bandLim = &tempBandLim;

        txParams.print("grant()");

        unsigned int numBands = ueMac_->getDeployer()->getNumBands();
        // for each band of the band vector provided
        for (unsigned int i = 0; i < numBands; i++)
        {
            BandLimit elem;
            // copy the band
            elem.band_ = Band(i);
            EV << "Putting band " << i << endl;
            // mark as unlimited
            for (unsigned int j = 0; j < numCodewords; j++)
            {
                EV << "- Codeword " << j << endl;
                elem.limit_.push_back(-1);
            }
            bandLim->push_back(elem);
        }
    }
    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant(" << cid << "," << bytes << "," << terminate << "," << active << "," << eligible << "," << bands_msg << "," << dasToA(antenna) << ")" << endl;

    // Perform normal operation for grant

    // Get virtual buffer reference
    //    LteMacBuffer* conn = ((dir == DL) ? vbuf_->at(cid) : bsrbuf_->at(cid)); as direction is immaterial in Unassissted D2D mode
    LteMacBuffer* conn = bsrbuf_->at(cid);

    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant --------------------::[ START GRANT ]::--------------------" << endl;
    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Cell: " << ueMac_->getMacCellId() << endl;
    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant CID: " << cid << "(UE: " << nodeId << ", Flow: " << flowId << ") current Antenna [" << dasToA(antenna) << "]" << endl;

    //! Multiuser MIMO support
    if (ueMac_->muMimo() && (txParams.readTxMode() == MULTI_USER))
    {
        // request AMC for MU_MIMO pairing
        MacNodeId peer = ueMac_->getAmc()->computeMuMimoPairing(nodeId, dir);
        if (peer != nodeId)
        {
            // this user has a valid pairing
            //1) register pairing  - if pairing is already registered false is returned
            if (allocator_->configureMuMimoPeering(nodeId, peer))
            {
                EV << "LteSchedulerUeUnassistedD2D::scheduleGrant MU-MIMO pairing established: main user [" << nodeId << "], paired user [" << peer << "]" << endl;
            }
            else
            {
                EV << "LteSchedulerUeUnassistedD2D::scheduleGrant MU-MIMO pairing already exists between users [" << nodeId << "] and [" << peer << "]" << endl;
            }
        }
        else
        {
            EV << "LteSchedulerUeUnassistedD2D::scheduleGrant no MU-MIMO pairing available for user [" << nodeId << "]" << endl;
        }
    }

                // registering DAS spaces to the allocator
    Plane plane = allocator_->getOFDMPlane(nodeId);
    allocator_->setRemoteAntenna(plane, antenna);

    unsigned int cwAlredyAllocated = 0;
    // search for already allocated codeword

    if (allocatedCws_.find(nodeId) != allocatedCws_.end())
    {
        cwAlredyAllocated = allocatedCws_.at(nodeId);
    }
    // Check OFDM space
    // OFDM space is not zero if this if we are trying to allocate the second cw in SPMUX or
    // if we are tryang to allocate a peer user in mu_mimo plane
    if (allocator_->computeTotalRbs() == 0 && (((txParams.readTxMode() != OL_SPATIAL_MULTIPLEXING &&
        txParams.readTxMode() != CL_SPATIAL_MULTIPLEXING) || cwAlredyAllocated == 0) &&
        (txParams.readTxMode() != MULTI_USER || plane != MU_MIMO_PLANE)))
    {
        terminate = true; // ODFM space ended, issuing terminate flag
        EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Space ended, no schedulation." << endl;
        return 0;
    }

    // TODO This is just a BAD patch
    // check how a codeword may be reused (as in the if above) in case of non-empty OFDM space
    // otherwise check why an UE is stopped being scheduled while its buffer is not empty
    if (cwAlredyAllocated > 0)
    {
        terminate = true;
        return 0;
    }

    // DEBUG OUTPUT
    if (limitBl)
        EV << "LteSchedulerUeUnassistedD2D::scheduleGrant blocks: " << bytes << endl;
        else
        EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Bytes: " << bytes << endl;
    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Bands: {";
    unsigned int size = (*bandLim).size();
    if (size > 0)
    {
        EV << (*bandLim).at(0).band_;
        for(unsigned int i = 1; i < size; i++)
        EV << ", " << (*bandLim).at(i).band_;
    }
    EV << "}\n";

    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant TxMode: " << txModeToA(txParams.readTxMode()) << endl;
    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Available codewords: " << numCodewords << endl;

    bool stop = false;
    unsigned int totalAllocatedBytes = 0; // total allocated data (in bytes)
    unsigned int totalAllocatedBlocks = 0; // total allocated data (in blocks)
    Codeword cw = 0; // current codeword, modified by reference by the checkeligibility function

    // Retrieve the first free codeword checking the eligibility - check eligibility could modify current cw index.
    if (!checkEligibility(nodeId, cw) || cw >= numCodewords)
    {
        eligible = false;

        EV << "LteSchedulerUeUnassistedD2D::scheduleGrant @@@@@ CODEWORD " << cw << " @@@@@" << endl;
        EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Total allocation: " << totalAllocatedBytes << "bytes" << endl;
        EV << "LteSchedulerUeUnassistedD2D::scheduleGrant NOT ELIGIBLE!!!" << endl;
        EV << "LteSchedulerUeUnassistedD2D::scheduleGrant --------------------::[  END GRANT  ]::--------------------" << endl;
        return totalAllocatedBytes; // return the total number of served bytes
    }

    for (; cw < numCodewords; ++cw)
    {
        unsigned int cwAllocatedBytes = 0;
        // used by uplink only, for signaling cw blocks usage to schedule list
        unsigned int cwAllocatedBlocks = 0;
        // per codeword vqueue item counter (UL: BSRs DL: SDUs)
        unsigned int vQueueItemCounter = 0;

        std::list<Request> bookedRequests;

        // band limit structure

        EV << "LteSchedulerUeUnassistedD2D::scheduleGrant @@@@@ CODEWORD " << cw << " @@@@@" << endl;

        unsigned int size = (*bandLim).size();

        bool firstSdu = true;

        for (unsigned int i = 0; i < size; ++i)
        {
            // for sulle bande
            unsigned int bandAllocatedBytes = 0;

            // save the band and the relative limit
            Band b = (*bandLim).at(i).band_;
            int limit = (*bandLim).at(i).limit_.at(cw);

            EV << "LteSchedulerUeUnassistedD2D::scheduleGrant --- BAND " << b << " LIMIT " << limit << "---" << endl;

            // if the limit flag is set to skip, jump off
            if (limit == -2)
            {
                EV << "LteSchedulerUeUnassistedD2D::scheduleGrant skipping logical band according to limit value" << endl;
                continue;
            }

            unsigned int allocatedCws = 0;
            // search for already allocated codeword

            if (allocatedCws_.find(nodeId) != allocatedCws_.end())
            {
                allocatedCws = allocatedCws_.at(nodeId);
            }
            unsigned int bandAvailableBytes = 0;
            unsigned int bandAvailableBlocks = 0;
            // if there is a previous blocks allocation on the first codeword, blocks allocation is already available
            if (allocatedCws != 0)
            {
                // get band allocated blocks
                int b1 = allocator_->getBlocks(antenna, b, nodeId);
                // limit eventually allocated blocks on other codeword to limit for current cw
                bandAvailableBlocks = (limitBl ? (b1 > limit ? limit : b1) : b1);

                //    bandAvailableBlocks=b1;

                bandAvailableBytes = ueMac_->getAmc()->computeBytesOnNRbs(nodeId, b, cw, bandAvailableBlocks, dir);
            }
            // if limit is expressed in blocks, limit value must be passed to availableBytes function
            else
            {
                bandAvailableBytes = availableBytes(nodeId, antenna, b, cw, dir, (limitBl) ? limit : -1); // available space (in bytes)
                bandAvailableBlocks = allocator_->availableBlocks(nodeId, antenna, b);
            }

            // if no allocation can be performed, notify to skip
            // the band on next processing (if any)

            if (bandAvailableBytes == 0)
            {
                EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Band " << b << "will be skipped since it has no space left." << endl;
                (*bandLim).at(i).limit_.at(cw) = -2;
                continue;
            }
                //if bandLimit is expressed in bytes
            if (!limitBl)
            {
                // use the provided limit as cap for available bytes, if it is not set to unlimited
                if (limit >= 0 && limit < (int) bandAvailableBytes)
                {
                    bandAvailableBytes = limit;
                    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Band space limited to " << bandAvailableBytes << " bytes according to limit cap" << endl;
                }
            }
            else
            {
                // if bandLimit is expressed in blocks
                if(limit >= 0 && limit < (int) bandAvailableBlocks)
                {
                    bandAvailableBlocks=limit;
                    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Band space limited to " << bandAvailableBlocks << " blocks according to limit cap" << endl;
                }
            }

            EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Available Bytes: " << bandAvailableBytes << " available blocks " << bandAvailableBlocks << endl;

            // TODO Consider the MAC PDU header size.

            // Process the SDUs of the specified flow
            while (true)
            {
                // Check whether the virtual buffer is empty
                if (conn->isEmpty())
                {
                    active = false; // all user data have been served
                    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant scheduled connection is no more active . Exiting grant " << endl;
                    stop = true;
                    break;
                }

                PacketInfo vpkt = conn->front();
                unsigned int vQueueFrontSize = vpkt.first;
                unsigned int toServe = vQueueFrontSize;

                // for the first SDU, add 2 byte for the MAC PDU Header
                if (firstSdu)
                {
                    toServe += MAC_HEADER;
//                    firstSdu = false;
                }

                //   Check if the flow will overflow its request by adding an entire SDU
                if (((vQueueFrontSize + totalAllocatedBytes + cwAllocatedBytes + bandAllocatedBytes) > bytes)
                    && bytes != 0)
                {
                    if (dir == DL)
                    {
                        // can't schedule partial SDUs
                        EV << "LteSchedulerUeUnassistedD2D::scheduleGrant available grant is  :  " << bytes - (totalAllocatedBytes + cwAllocatedBytes + bandAllocatedBytes) << ", insufficient for SDU size : " << vQueueFrontSize << endl;
                        stop = true;
                        break;
                    }
                    else
                    {
                        // update toServe to : bytes requested by grant minus so-far-allocated bytes
                        toServe = bytes - totalAllocatedBytes-cwAllocatedBytes - bandAllocatedBytes;
                    }
                }

                        // compute here total booked requests

                unsigned int totalBooked = 0;
                unsigned int bookedUsed = 0;

                std::list<Request>::iterator li = bookedRequests.begin(), le = bookedRequests.end();

                for (; li != le; ++li)
                {
                    totalBooked += li->bytes_;
                    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Band " << li->b_ << " can contribute with " << li->bytes_ << " of booked resources " << endl;
                }

                // The current SDU may have a size greater than available space. In this case, book the band.
                // During UL/D2D scheduling, if this is the last band, it means that there is no more space
                // for booking resources, thus we allocate partial BSR
                if (toServe > (bandAvailableBytes + totalBooked) && (direction_ == DL || ((direction_ == UL || direction_ == D2D || direction_ == D2D_MULTI) && b < size-1)))
                {
                    unsigned int blocksAdded = ueMac_->getAmc()->computeReqRbs(nodeId, b, cw, bandAllocatedBytes,        // substitute bandAllocatedBytes
                        dir);

                    if (blocksAdded > bandAvailableBlocks)
                        throw cRuntimeError("band %d GRANT allocation overflow : avail. blocks %d alloc. blocks %d", b,
                            bandAvailableBlocks, blocksAdded);

                    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Booking band available blocks" << (bandAvailableBlocks-blocksAdded) << " [" << bandAvailableBytes << " bytes] for future use, going to next band" << endl;
                    // enable booking  here
                    bookedRequests.push_back(Request(b, bandAvailableBytes, bandAvailableBlocks - blocksAdded));
                    //  skipping this band
                    break;
                }
                else
                {
                    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant servicing " << toServe << " bytes with " << bandAvailableBytes << " own bytes and " << totalBooked << " booked bytes " << endl;

                    // decrease booking value - if totalBooked is greater than 0, we used booked resources for scheduling the pdu
                    if (totalBooked>0)
                    {
                        // reset booked resources iterator.
                        li=bookedRequests.begin();

                        EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Making use of booked resources [" << totalBooked << "] for inter-band data allocation" << endl;
                        // updating booked requests structure
                        while ((li!=le) && (bookedUsed<=toServe))
                        {
                            Band u = li->b_;
                            unsigned int uBytes = ((li->bytes_ > toServe )? toServe : li->bytes_ );

                            EV << "LteSchedulerUeUnassistedD2D::scheduleGrant allocating " << uBytes << " prev. booked bytes on band " << (unsigned short)u << endl;

                            // mark here the usage of booked resources
                            bookedUsed+= uBytes;
                            cwAllocatedBytes+=uBytes;

                            unsigned int uBlocks = ueMac_->getAmc()->computeReqRbs(nodeId,u, cw, uBytes, dir);

                            if (uBlocks <= li->blocks_)
                            {
                                li->blocks_-=uBlocks;
                            }
                            else
                            {
                                li->blocks_=uBlocks=0;
                            }

                            // update limit
                            if (uBlocks>0)
                            {
                                unsigned int j=0;
                                for (;j<(*bandLim).size();++j) if ((*bandLim).at(j).band_==u) break;

                                if((*bandLim).at(j).limit_.at(cw) > 0)
                                {
                                    (*bandLim).at(j).limit_.at(cw) -= uBlocks;

                                    if ((*bandLim).at(j).limit_.at(cw) < 0)
                                    throw cRuntimeError("Limit decreasing error during booked resources allocation on band %d : new limit %d, due to blocks %d ",
                                        u,(*bandLim).at(j).limit_.at(cw),uBlocks);
                                }
                            }

                            if(allocatedCws == 0)
                            {
                                // mark here allocation
                                allocator_->addBlocks(antenna,u,nodeId,uBlocks,uBytes);
                                cwAllocatedBlocks += uBlocks;
                                totalAllocatedBlocks += uBlocks;
                            }

                            // update reserved status

                            if (li->bytes_>toServe)
                            {
                                li->bytes_-=toServe;

                                li++;
                            }
                            else
                            {
                                std::list<Request>::iterator erase = li;
                                // increment pointer.
                                li++;
                                // erase element from list
                                bookedRequests.erase(erase);

                                EV << "LteSchedulerUeUnassistedD2D::scheduleGrant band " << (unsigned short)u << " depleted all its booked resources " << endl;
                            }
                        }
                    }
                    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant band " << (unsigned short)b << " scheduled  " << toServe << " bytes " << " using " << bookedUsed << " resources on other bands and "
                       << (toServe-bookedUsed) << " on its own space which was " << bandAvailableBytes << endl;

                    if (direction_ == UL || direction_ == D2D || direction_ == D2D_MULTI)
                    {
                        // in UL/D2D, serve what it is possible to serve
                        if ((toServe-bookedUsed)>bandAvailableBytes)
                            toServe = bookedUsed + bandAvailableBytes;
                    }

                    if ((toServe-bookedUsed)>bandAvailableBytes)
                    throw cRuntimeError(" Booked resources allocation exited in an abnormal state ");

                    //if (bandAvailableBytes < ((toServe+bookedUsed)))
                    //    throw cRuntimeError (" grant allocation overflow ");

                    // Updating available and allocated data counters
                    bandAvailableBytes -= (toServe-bookedUsed);
                    bandAllocatedBytes += (toServe-bookedUsed);

                    // consume one virtual SDU

                    // check if we are granting less than a full BSR |
                    if ((dir==UL || dir==D2D || dir==D2D_MULTI) && (toServe-MAC_HEADER-RLC_HEADER_UM<vQueueFrontSize))
                    {
                        // update the virtual queue

//                        conn->front().first = (vQueueFrontSize-toServe);

                        // just decrementing the first element is not correct because the queueOccupancy would not be updated
                        // we need to pop the element from the queue and then push it again with a new value
                        unsigned int newSize = conn->front().first - (toServe-MAC_HEADER-RLC_HEADER_UM);
                        PacketInfo info = conn->popFront();

                        // push the element only if the size is > 0
                        if (newSize > 0)
                        {
                            info.first = newSize;
                            conn->pushFront(info);
                        }
                    }
                    else
                    {
                        conn->popFront();
                    }

                    vQueueItemCounter++;

                    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Consumed: " << vQueueItemCounter << " MAC-SDU (" << vQueueFrontSize << " bytes) [Available: " << bandAvailableBytes << " bytes] [Allocated: " << bandAllocatedBytes << " bytes]" << endl;

                    // if there are no more bands available for UL/D2D scheduling
                    if ((direction_ == UL || direction_ == D2D || direction_ == D2D_MULTI) && b == size-1)
                    {
                        stop = true;
                        break;
                    }

                    if(bytes == 0 && vQueueItemCounter>0)
                    {
                        // Allocate only one SDU
                        EV << "LteSchedulerUeUnassistedD2D::scheduleGrant ONLY ONE VQUEUE ITEM TO ALLOCATE" << endl;
                        stop = true;
                        break;
                    }

                    if (firstSdu)
                    {
                        firstSdu = false;
                    }
                }
            }

            cwAllocatedBytes += bandAllocatedBytes;

            if (bandAllocatedBytes > 0)
            {
                unsigned int blocksAdded = ueMac_->getAmc()->computeReqRbs(nodeId, b, cw, bandAllocatedBytes, dir);

                if (blocksAdded > bandAvailableBlocks)
                    throw cRuntimeError("band %d GRANT allocation overflow: avail. blocks %d alloc. blocks %d", b,
                        bandAvailableBlocks, blocksAdded);

                // update available blocks
                bandAvailableBlocks -= blocksAdded;

                if (allocatedCws == 0)
                {
                    // Record the allocation only for the first codeword
                    if (allocator_->addBlocks(antenna, b, nodeId, blocksAdded, bandAllocatedBytes))
                    {
                        totalAllocatedBlocks += blocksAdded;
                    }
                    else
                        throw cRuntimeError("band %d ALLOCATOR allocation overflow - allocation failed in allocator",
                            b);
                }
                cwAllocatedBlocks += blocksAdded;

                if ((*bandLim).at(i).limit_.at(cw) > 0)
                {
                    if (limitBl)
                    {
                        EV << "LteSchedulerUeUnassistedD2D::scheduleGrant BandLimit decreasing limit on band " << b << " limit " << (*bandLim).at(i).limit_.at(cw) << " blocks " << blocksAdded << endl;
                        (*bandLim).at(i).limit_.at(cw) -=blocksAdded;
                    }
                    // signal the amount of allocated bytes in the current band
                    else if (!limitBl)
                    {
                        (*bandLim).at(i).limit_.at(cw) -= bandAllocatedBytes;
                    }

                    if ((*bandLim).at(i).limit_.at(cw) <0)
                    throw cRuntimeError("BandLimit Main Band %d decreasing inconsistency : %d",b,(*bandLim).at(i).limit_.at(cw));
                }
            }

            if (stop)
                break;
        }

        EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Codeword allocation: " << cwAllocatedBytes << "bytes" << endl;

        if (cwAllocatedBytes > 0)
        {
            // mark codeword as used
            if (allocatedCws_.find(nodeId) != allocatedCws_.end())
            {
                allocatedCws_.at(nodeId)++;
                }
            else
            {
                allocatedCws_[nodeId] = 1;
            }

            totalAllocatedBytes += cwAllocatedBytes;

            std::pair<unsigned int, Codeword> scListId;
            scListId.first = cid;
            scListId.second = cw;

            if (scheduleList_.find(scListId) == scheduleList_.end())
                scheduleList_[scListId] = 0;

            // if direction is DL , then schedule list contains number of to-be-trasmitted SDUs ,
            // otherwise it contains number of granted blocks
            scheduleList_[scListId] += ((dir == DL) ? vQueueItemCounter : cwAllocatedBlocks);

            EV << "LteSchedulerUeUnassistedD2D::scheduleGrant CODEWORD IS NOW BUSY: GO TO NEXT CODEWORD." << endl;
            if (allocatedCws_.at(nodeId) == MAX_CODEWORDS)
            {
                eligible = false;
                stop = true;
            }
        }
        else
        {
            EV << "LteSchedulerUeUnassistedD2D::scheduleGrant CODEWORD IS FREE: NO ALLOCATION IS POSSIBLE IN NEXT CODEWORD." << endl;
            eligible = false;
            stop = true;
        }
        if (stop)
            break;
    }

    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant Total allocation: " << totalAllocatedBytes << " bytes, " << totalAllocatedBlocks << " blocks" << endl;
    EV << "LteSchedulerUeUnassistedD2D::scheduleGrant --------------------::[  END GRANT  ]::--------------------" << endl;

    return totalAllocatedBytes;
}

// Allocator of the bands to Tx UE's
bool LteSchedulerUeUnassistedD2D::racschedule() {
    // Check LteSchedulerEnbUl::racschedule()

    EV << NOW
              << " LteSchedulerUeUnassistedD2D::racschedule --------------------::[ START RAC-SCHEDULE ]::--------------------"
              << endl;
    EV << NOW << " LteSchedulerUeUnassistedD2D::racschedule Rx mode of D2D UE : "
              << ueMac_->getMacNodeId() << endl;
    EV << NOW
              << " LteSchedulerUeUnassistedD2D::racschedule Direction in sidelink (probably D2D): "
              << (direction_ == UL ? "UL" : "DL") << endl;

    RacStatus::iterator it = racStatus_.begin(), et = racStatus_.end();

    for (; it != et; ++it) {
        // get current nodeId
        MacNodeId nodeId = it->first;
        EV << NOW
                  << " LteSchedulerUeUnassistedD2D::racschedule handling RAC for Tx D2D UE node "
                  << nodeId << endl;

        // Get number of logical bands
        unsigned int numBands =
                ((ueMac_->getENodeB())->getDeployer())->getNumBands();

        // FIXME default behavior
        //try to allocate one block to selected UE on at least one logical band of MACRO antenna, first codeword

        const unsigned int cw = 0;
        const unsigned int blocks = 1;

        bool allocation = false;

        for (Band b = 0; b < numBands; ++b) {
            if (allocator_->availableBlocks(nodeId, MACRO, b) > 0) {
                unsigned int bytes = ueMac_->getAmc()->computeBytesOnNRbs(
                        nodeId, b, cw, blocks, UL);
                if (bytes > 0) {
                    allocator_->addBlocks(MACRO, b, nodeId, 1, bytes);

                    EV << NOW
                              << "LteSchedulerUeUnassistedD2D::racschedule  Tx D2D UE: "
                              << nodeId << " Handled RAC on band: " << b
                              << endl;

                    allocation = true;
                    break;
                }
            }
        }

        if (allocation) {
            // create scList id for current cid/codeword
            MacCid cid = idToMacCid(nodeId, SHORT_BSR); // build the cid. Since this grant will be used for a BSR,
                                                        // we use the LCID corresponding to the SHORT_BSR
            std::pair<unsigned int, Codeword> scListId = std::pair<unsigned int,
                    Codeword>(cid, cw);
            scheduleList_[scListId] = blocks;
        }
    }

    // clean up all requests
    racStatus_.clear();

    EV << NOW
              << " LteSchedulerUeUnassistedD2D::racschedule --------------------::[  END RAC-SCHEDULE  ]::--------------------"
              << endl;

    int availableBlocks = allocator_->computeTotalRbs();

    return (availableBlocks == 0);

}

bool LteSchedulerUeUnassistedD2D::checkEligibility(MacNodeId id, Codeword& cw) {
    // check if harq buffer have already been created for this node
    if (ueMac_->getHarqTxBuffers()->find(id)
            != ueMac_->getHarqTxBuffers()->end()) {
        LteHarqBufferTx* dlHarq = ueMac_->getHarqTxBuffers()->at(id);
        UnitList freeUnits = dlHarq->firstAvailable();

        if (freeUnits.first != HARQ_NONE) {
            if (freeUnits.second.empty())
                // there is a process currently selected for user <id> , but all of its cws have been already used.
                return false;
            // retrieving the cw index
            cw = freeUnits.second.front();
            // DEBUG check
            if (cw > MAX_CODEWORDS)
                throw cRuntimeError(
                        "LteSchedulerUeUnassistedD2D::checkEligibility(): abnormal codeword id %d",
                        (int) cw);
            return true;
        }
    }
    return true;
}

unsigned int LteSchedulerUeUnassistedD2D::schedulePerAcidRtxD2D(MacNodeId destId,
        MacNodeId senderId, Codeword cw, unsigned char acid,
        std::vector<BandLimit>* bandLim, Remote antenna, bool limitBl) {
    Direction dir = D2D;
    try {
        std::string bands_msg = "BAND_LIMIT_SPECIFIED";
        if (bandLim == NULL) {
            bands_msg = "NO_BAND_SPECIFIED";
            // Create a vector of band limit using all bands
            // FIXME: bandlim is never deleted
            bandLim = new std::vector<BandLimit>();

            unsigned int numBands =
                    ((ueMac_->getENodeB())->getDeployer())->getNumBands();
            // for each band of the band vector provided
            for (unsigned int i = 0; i < numBands; i++) {
                BandLimit elem;
                // copy the band
                elem.band_ = Band(i);
                EV << "Putting band " << i << endl;
                // mark as unlimited
                for (Codeword i = 0; i < MAX_CODEWORDS; ++i) {
                    elem.limit_.push_back(-1);
                }
                bandLim->push_back(elem);
            }
        }

        EV << NOW << "LteSchedulerUeUnassistedD2D::schedulePerAcidRtxD2D - Node["
                  << ueMac_->getMacNodeId() << ", User[" << senderId
                  << ", Codeword[ " << cw << "], ACID[" << (unsigned int) acid
                  << "] " << endl;

        // Get the current active HARQ process
        HarqRxBuffersMirror* harqRxBuffersD2DMirror;
        if (strcmp(ueMac_->getClassName(), "LteMacUeRealisticD2D") == 0)
            harqRxBuffersD2DMirror = check_and_cast<LteMacUeRealisticD2D*>(
                    ueMac_)->getRxHarqBufferMirror();
        else
            throw cRuntimeError(
                    "LteSchedulerUeUnassistedD2D::rtxschedule - unrecognized MAC type %s",
                    ueMac_->getClassName());
        unsigned char currentAcid = (harqStatus_.at(senderId) + 2)
                % (harqRxBuffersD2DMirror->at(destId)->numHarqProcesses_);
        EV << "\t the acid that should be considered is "
                  << (unsigned int) currentAcid << endl;

        LteHarqProcessRxD2DMirror* currentProcess = harqRxBuffersD2DMirror->at(
                destId)->processes_[currentAcid];
        if (currentProcess->status_.at(cw) != RXHARQ_PDU_CORRUPTED) {
            // exit if the current active HARQ process is not ready for retransmission
            EV << NOW
                      << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtxD2D User is on ACID "
                      << (unsigned int) currentAcid
                      << " HARQ process is IDLE. No RTX scheduled ." << endl;

            delete (bandLim);
            return 0;
        }

        Codeword allocatedCw = 0;
        //Codeword allocatedCw = MAX_CODEWORDS;
        //search for already allocated codeword
        //create "mirror" scList ID for other codeword than current
        std::pair<unsigned int, Codeword> scListMirrorId = std::pair<
                unsigned int, Codeword>(idToMacCid(senderId, D2D_SHORT_BSR),
        MAX_CODEWORDS - cw - 1);
        if (scheduleList_.find(scListMirrorId) != scheduleList_.end()) {
            allocatedCw = MAX_CODEWORDS - cw - 1;
        }
        // get current process buffered PDU byte length
        unsigned int bytes = currentProcess->pdu_length_.at(cw);
        // bytes to serve
        unsigned int toServe = bytes;
        // blocks to allocate for each band
        std::vector<unsigned int> assignedBlocks;
        // bytes which blocks from the preceding vector are supposed to satisfy
        std::vector<unsigned int> assignedBytes;

        // end loop signal [same as bytes>0, but more secure]
        bool finish = false;
        // for each band
        unsigned int size = bandLim->size();
        for (unsigned int i = 0; (i < size) && (!finish); ++i) {
            // save the band and the relative limit
            Band b = bandLim->at(i).band_;
            int limit = bandLim->at(i).limit_.at(cw);

            // TODO add support to multi CW
            //unsigned int bandAvailableBytes = // if a codeword has been already scheduled for retransmission, limit available blocks to what's been  allocated on that codeword
            //((allocatedCw == MAX_CODEWORDS) ? availableBytes(nodeId,antenna, b, cw) : ueMac_->getAmc()->blocks2bytes(nodeId, b, cw, allocator_->getBlocks(antenna,b,nodeId) , direction_));    // available space
            unsigned int bandAvailableBytes = availableBytes(senderId, antenna,
                    b, cw, dir);

            // use the provided limit as cap for available bytes, if it is not set to unlimited
            if (limit >= 0)
                bandAvailableBytes =
                        limit < (int) bandAvailableBytes ?
                                limit : bandAvailableBytes;

            EV << NOW << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtxD2D BAND "
                      << b << endl;
            EV << NOW
                      << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtxD2D total bytes:"
                      << bytes << " still to serve: " << toServe << " bytes"
                      << endl;
            EV << NOW
                      << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtxD2D Available: "
                      << bandAvailableBytes << " bytes" << endl;

            unsigned int servedBytes = 0;
            // there's no room on current band for serving the entire request
            if (bandAvailableBytes < toServe) {
                // record the amount of served bytes
                servedBytes = bandAvailableBytes;
                // the request can be fully satisfied
            } else {
                // record the amount of served bytes
                servedBytes = toServe;
                // signal end loop - all data have been serviced
                finish = true;
                EV << NOW
                          << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtxD2D ALL DATA HAVE BEEN SERVICED"
                          << endl;
            }
            unsigned int servedBlocks = ueMac_->getAmc()->computeReqRbs(
                    senderId, b, cw, servedBytes, dir);
            // update the bytes counter
            toServe -= servedBytes;
            // update the structures
            assignedBlocks.push_back(servedBlocks);
            assignedBytes.push_back(servedBytes);
        }

        if (toServe > 0) {
            // process couldn't be served - no sufficient space on available bands
            EV << NOW
                      << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtxD2D Unavailable space for serving node "
                      << senderId << " ,HARQ Process "
                      << (unsigned int) currentAcid << " on codeword " << cw
                      << endl;

            delete (bandLim);
            return 0;
        } else {
            // record the allocation
            unsigned int size = assignedBlocks.size();
            unsigned int cwAllocatedBlocks = 0;

            // create scList id for current cid/codeword
            std::pair<unsigned int, Codeword> scListId = std::pair<unsigned int,
                    Codeword>(idToMacCid(senderId, D2D_SHORT_BSR), cw);

            for (unsigned int i = 0; i < size; ++i) {
                // For each LB for which blocks have been allocated
                Band b = bandLim->at(i).band_;

                cwAllocatedBlocks += assignedBlocks.at(i);
                EV << "\t Cw->" << allocatedCw << "/" << MAX_CODEWORDS << endl;
                //! handle multi-codeword allocation
                if (allocatedCw != MAX_CODEWORDS) {
                    EV << NOW
                              << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtxD2D - adding "
                              << assignedBlocks.at(i) << " to band " << i
                              << endl;
                    allocator_->addBlocks(antenna, b, senderId,
                            assignedBlocks.at(i), assignedBytes.at(i));
                }
                //! TODO check if ok bandLim->at.limit_.at(cw) = assignedBytes.at(i);
            }

            // signal a retransmission
            // schedule list contains number of granted blocks
            scheduleList_[scListId] = cwAllocatedBlocks;
            // mark codeword as used
            if (allocatedCws_.find(senderId) != allocatedCws_.end()) {
                allocatedCws_.at(senderId)++;}
            else {
                allocatedCws_[senderId] = 1;
            }

            EV << NOW
                      << " LteSchedulerUeUnassistedD2D::schedulePerAcidRtxD2D HARQ Process "
                      << (unsigned int) currentAcid << " : " << bytes
                      << " bytes served! " << endl;

            delete (bandLim);
            return bytes;
        }
    } catch (std::exception& e) {
        throw cRuntimeError(
                "Exception in LteSchedulerUeUnassistedD2D::schedulePerAcidRtxD2D(): %s",
                e.what());
    }
    delete (bandLim);
    return 0;
}

void LteSchedulerUeUnassistedD2D::updateHarqDescs() {
    EV << NOW << "LteSchedulerUeUnassistedD2D::updateHarqDescs  cell "
              << ueMac_->getMacCellId() << endl;

    HarqRxBuffers::iterator it;
    HarqStatus::iterator currentStatus;
    if (!harqRxBuffers_ == NULL) {
        for (it = harqRxBuffers_->begin(); it != harqRxBuffers_->end(); ++it) {
            if ((currentStatus = harqStatus_.find(it->first))
                    != harqStatus_.end()) {
                EV << NOW << "LteSchedulerUeUnassistedD2D::updateHarqDescs UE "
                          << it->first << " OLD Current Process is  "
                          << (unsigned int) currentStatus->second << endl;
                // updating current acid id
                currentStatus->second = (currentStatus->second + 1)
                        % (it->second->getProcesses());

                EV << NOW << "LteSchedulerUeUnassistedD2D::updateHarqDescs UE "
                          << it->first << "NEW Current Process is "
                          << (unsigned int) currentStatus->second
                          << "(total harq processes "
                          << it->second->getProcesses() << ")" << endl;
            } else {
                EV << NOW << "LteSchedulerUeUnassistedD2D::updateHarqDescs UE "
                          << it->first << " initialized the H-ARQ status "
                          << endl;
                harqStatus_[it->first] = 0;
            }
        }
    }
}

// Same as LteSchedulerUeUL
//SL-UL-TX Scheduling done by Tx D2D UE
LteMacScheduleList*
LteSchedulerUeUnassistedD2D::scheduleData() {
    // 1) Environment Setup

    // clean up old scheduling decisions
    scheduleList_.clear();

    // get the grant
    const LteSchedulingGrant* grant = ueMac_->getSchedulingGrant();
    Direction dir = grant->getDirection();

    // get the nodeId of the mac owner node
    MacNodeId nodeId = ueMac_->getMacNodeId();

    EV << NOW << " LteSchedulerUeUnassistedD2D::scheduleData - Scheduling node "
              << nodeId << endl;

    // retrieve Transmission parameters
//        const UserTxParams* txPar = grant->getUserTxParams();

//! MCW support in UL
    unsigned int codewords = grant->getCodewords();

    // TODO get the amount of granted data per codeword
    //unsigned int availableBytes = grant->getGrantedBytes();

    unsigned int availableBlocks = grant->getTotalGrantedBlocks();

    // TODO check if HARQ ACK messages should be subtracted from available bytes

    for (Codeword cw = 0; cw < codewords; ++cw) {
        unsigned int availableBytes = grant->getGrantedCwBytes(cw);

        EV << NOW << " LteSchedulerUeUnassistedD2D::scheduleData - Node "
                  << ueMac_->getMacNodeId() << " available data from grant are "
                  << " blocks " << availableBlocks << " [" << availableBytes
                  << " - Bytes]  on codeword " << cw << endl;

        // per codeword LCP scheduler invocation

        // invoke the schedule() method of the attached LCP scheduler in order to schedule
        // the connections provided
        std::map<MacCid, unsigned int>& sdus =
                lteUnassistedD2DSchedulingAgent_->scheduleData(availableBytes, dir);

        // TODO check if this jump is ok
        if (sdus.empty())
            continue;

        std::map<MacCid, unsigned int>::const_iterator it = sdus.begin(), et =
                sdus.end();
        for (; it != et; ++it) {
            // set schedule list entry
            std::pair<MacCid, Codeword> schedulePair(it->first, cw);
            scheduleList_[schedulePair] = it->second;
        }

        MacCid highestBackloggedFlow = 0;
        MacCid highestBackloggedPriority = 0;
        MacCid lowestBackloggedFlow = 0;
        MacCid lowestBackloggedPriority = 0;
        bool backlog = false;

        // get the highest backlogged flow id and priority
        backlog = ueMac_->getHighestBackloggedFlow(highestBackloggedFlow,
                highestBackloggedPriority);

        if (backlog) // at least one backlogged flow exists
        {
            // get the lowest backlogged flow id and priority
            ueMac_->getLowestBackloggedFlow(lowestBackloggedFlow,
                    lowestBackloggedPriority);
        }

        // TODO make use of above values
    }
    return &scheduleList_;
}


void LteSchedulerUeUnassistedD2D::resourceBlockStatistics(bool sleep)
{
    if (sleep)
    {
        tSample_->id_ = ueMac_->getMacCellId();
        if (direction_ == DL)
        {
            tSample_->sample_ = 0;
            ueMac_->emit(cellBlocksUtilizationDl_, tSample_);
            tSample_->sample_ = 0;
            ueMac_->emit(lteAvgServedBlocksDl_, tSample_);
            tSample_->sample_ = ueMac_->getIdleLevel(DL, MBSFN);
            ueMac_->emit(depletedPowerDl_, tSample_);
        }
        return;
    }
    // Get a reference to the begin and the end of the map which stores the blocks allocated
    // by each UE in each Band. In this case, is requested the pair of iterators which refers
    // to the per-Band (first key) per-Ue (second-key) map
    std::vector<std::vector<unsigned int> >::const_iterator planeIt =
        allocator_->getAllocatedBlocksBegin();
    std::vector<std::vector<unsigned int> >::const_iterator planeItEnd =
        allocator_->getAllocatedBlocksEnd();

    double utilization = 0.0;
    double allocatedBlocks = 0;
    unsigned int plane = 0;
    unsigned int antenna = 0;

    double depletedPower = 0;

    // TODO CHECK IDLE / ACTIVE STATUS
    //if ( ... ) {
    // For Each layer (normal/MU-MIMO)
    //    for (; planeIt != planeItEnd; ++planeIt) {
    std::vector<unsigned int>::const_iterator antennaIt = planeIt->begin();
    std::vector<unsigned int>::const_iterator antennaItEnd = planeIt->end();

    // For each antenna (MACRO/RUs)
    for (; antennaIt != antennaItEnd; ++antennaIt)
    {
        if (plane == MAIN_PLANE && antenna == MACRO)
            if (direction_ == DL)
                ueMac_->allocatedRB(*antennaIt);
        // collect the antenna utilization for current Layer
        utilization += (double) (*antennaIt);

        allocatedBlocks += (double) (*antennaIt);
        if (direction_ == DL)
        {
            depletedPower += ((ueMac_->getZeroLevel(direction_,
                ueMac_->getCurrentSubFrameType()))
                + ((double) (*antennaIt) * ueMac_->getPowerUnit(direction_,
                    ueMac_->getCurrentSubFrameType())));
        }
        EV << "LteSchedulerUeUnassistedD2D::resourceBlockStatistics collecting utilization for plane" <<
        plane << "antenna" << dasToA((Remote)antenna) << " allocated blocks "
           << allocatedBlocks << " depletedPower " << depletedPower << endl;
        antenna++;
    }
    plane++;
    //    }
    // antenna here is the number of antennas used; the same applies for plane;
    // Compute average OFDMA utilization between layers and antennas
    utilization /= (((double) (antenna)) * ((double) resourceBlocks_));
    tSample_->id_ = ueMac_->getMacCellId();
    if (direction_ == DL)
    {
        tSample_->sample_ = utilization;
        ueMac_->emit(cellBlocksUtilizationDl_, tSample_);
        tSample_->sample_ = allocatedBlocks;
        ueMac_->emit(lteAvgServedBlocksDl_, tSample_);
        tSample_->sample_ = depletedPower;
        ueMac_->emit(depletedPowerDl_, tSample_);
    }
    else if (direction_ == UL)
    {
        tSample_->sample_ = utilization;
        ueMac_->emit(cellBlocksUtilizationUl_, tSample_);
        tSample_->sample_ = allocatedBlocks;
        ueMac_->emit(lteAvgServedBlocksUl_, tSample_);
        tSample_->sample_ = depletedPower;
        ueMac_->emit(depletedPowerUl_, tSample_);
    }
    else
    {
        throw cRuntimeError("LteSchedulerUeUnassistedD2D::resourceBlockStatistics(): Unrecognized direction %d", direction_);
    }
}

void LteSchedulerUeUnassistedD2D::backlog(MacCid cid)
{
    EV << "LteSchedulerUeUnassistedD2D::backlog - backlogged data for Logical Cid " << cid << endl;
    if(cid == 1){   //HACK
        return;
    }
    lteUnassistedD2DSchedulingAgent_->notifyActiveConnection(cid);
}


unsigned int LteSchedulerUeUnassistedD2D::availableBytes(const MacNodeId id,
    Remote antenna, Band b, Codeword cw, Direction dir, int limit)
{
    EV << "LteSchedulerUeUnassistedD2D::availableBytes MacNodeId " << id << " Antenna " << dasToA(antenna) << " band " << b << " cw " << cw << endl;
    // Retrieving this user available resource blocks
    int blocks = allocator_->availableBlocks(id,antenna,b);
    //Consistency Check
    if (limit>blocks && limit!=-1)
    throw cRuntimeError("LteSchedulerUeUnassistedD2D::availableBytes signaled limit inconsistency with available space band b %d, limit %d, available blocks %d",b,limit,blocks);

    if (limit!=-1)
    blocks=(blocks>limit)?limit:blocks;
    unsigned int bytes = ueMac_->getAmc()->computeBytesOnNRbs(id, b, cw, blocks, dir);
    EV << "LteSchedulerUeUnassistedD2D::availableBytes MacNodeId " << id << " blocks [" << blocks << "], bytes [" << bytes << "]" << endl;

    return bytes;
}

LteUnassistedD2DSchedulingAgent* LteSchedulerUeUnassistedD2D::getScheduler(SchedDiscipline discipline, LteSchedulerUeUnassistedD2D* lteSchedulerUeUnassistedD2D_)
{

    EV << "Creating LteSchedulerUeUnassistedD2D " << schedDisciplineToA(discipline) << endl;
    switch(discipline)
    {
        case DRR:
            return new LteUnassistedD2DDRR(lteSchedulerUeUnassistedD2D_);
        case PF:
            return new LteUnassistedD2DPF(ueMac_->par("pfAlpha").doubleValue(),lteSchedulerUeUnassistedD2D_);
        case MAXCI:
            return new LteUnassistedD2DMaxCI(lteSchedulerUeUnassistedD2D_);
        default:
            throw cRuntimeError("LteSchedulerUeUnassistedD2D not recognized");
        return NULL;
    }
}
