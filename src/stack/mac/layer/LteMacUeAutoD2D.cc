//                           SimuLTE

#include "LteMacUeAutoD2D.h"
#include "LteMacQueue.h"
#include "LteSchedulingGrant.h"
#include "LteMacBuffer.h"
#include "UserTxParams.h"
#include "LteSchedulerAutoD2D.h"
#include "LteSchedulerUeAutoD2D.h"
//#include "LteSchedulerUeAutoD2DDl.h"
//#include "LteSchedulerUeAutoD2DUl.h"
#include "LteHarqBufferRx.h"
#include "LteHarqBufferRxD2DMirror.h"
#include "LteDeployer.h"
#include "D2DModeSwitchNotification_m.h"
#include "LteRac_m.h"
#include "LteAmc.h"

Define_Module(LteMacUeAutoD2D);

LteMacUeAutoD2D::LteMacUeAutoD2D() {
    deployer_ = NULL;
    amc_ = NULL;
    ueAutoD2DSchedulerDl_ = NULL;
    ueAutoD2DSchedulerUl_ = NULL;
    numAntennas_ = 0;
    bsrbuf_.clear();
    currentSubFrameType_ = NORMAL_FRAME_TYPE;
    nodeType_ = ENODEB;
    frameIndex_ = 0;
    lastTtiAllocatedRb_ = 0;
    racD2DMulticastRequested_ = false;
    bsrD2DMulticastTriggered_ = false;
    firstTx = false;
    lcgScheduler_ = NULL;
    schedulingGrant_ = NULL;
    currentHarq_ = 0;
    periodCounter_ = 0;
    expirationCounter_ = 0;
    racRequested_ = false;
    bsrTriggered_ = false;

    // TODO setup from NED
    racBackoffTimer_ = 0;
    maxRacTryouts_ = 0;
    currentRacTry_ = 0;
    minRacBackoff_ = 0;
    maxRacBackoff_ = 1;
    raRespTimer_ = 0;
    raRespWinStart_ = 3;
    nodeType_ = UE;

    // KLUDGE: this was unitialized, this is just a guess
    harqProcesses_ = 8;
}

LteMacUeAutoD2D::~LteMacUeAutoD2D() {
}

LteDeployer* LteMacUeAutoD2D::getDeployer() {
    // Get local deployer
    if (deployer_ != NULL)
        return deployer_;

    return check_and_cast<LteDeployer*>(getParentModule()-> // Stack
    getParentModule()-> // Enb
    getSubmodule("deployer")); // Deployer
}

LteDeployer* LteMacUeAutoD2D::getDeployer(MacNodeId nodeId) {
    // Get local deployer
    if (deployer_ != NULL)
        return deployer_;

    return check_and_cast<LteDeployer*>(getParentModule()-> // Stack
    getParentModule()-> // Enb
    getSubmodule("deployer")); // Deployer
}

void LteMacUeAutoD2D::initialize(int stage) {
    LteMacBase::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        // TODO: read NED parameters, when will be present
        deployer_ = getDeployer();
        /* Get num RB Dl */
        numRbDl_ = deployer_->getNumRbDl();
        /* Get num RB Ul */
        numRbUl_ = deployer_->getNumRbUl();

        /* Get number of antennas */
        numAntennas_ = getNumAntennas();

        /* Create and initialize MAC Downlink scheduler */
        if (ueAutoD2DSchedulerDl_ == NULL) {
            ueAutoD2DSchedulerDl_ = new LteSchedulerUeAutoD2DDl();
            ueAutoD2DSchedulerDl_->initialize(DL, this);
        }

        /* Create and initialize MAC Uplink scheduler */
        if (ueAutoD2DSchedulerUl_ == NULL) {
            ueAutoD2DSchedulerUl_ = new LteSchedulerUeAutoD2DUl();
            ueAutoD2DSchedulerUl_->initialize(UL, this);
        }
        //Initialize the current sub frame type with the first subframe of the MBSFN pattern
        currentSubFrameType_ = NORMAL_FRAME_TYPE;

        tSample_ = new TaggedSample();
        activatedFrames_ = registerSignal("activatedFrames");
        sleepFrames_ = registerSignal("sleepFrames");
        wastedFrames_ = registerSignal("wastedFrames");
        cqiDlMuMimo0_ = registerSignal("cqiDlMuMimo0");
        cqiDlMuMimo1_ = registerSignal("cqiDlMuMimo1");
        cqiDlMuMimo2_ = registerSignal("cqiDlMuMimo2");
        cqiDlMuMimo3_ = registerSignal("cqiDlMuMimo3");
        cqiDlMuMimo4_ = registerSignal("cqiDlMuMimo4");

        cqiDlTxDiv0_ = registerSignal("cqiDlTxDiv0");
        cqiDlTxDiv1_ = registerSignal("cqiDlTxDiv1");
        cqiDlTxDiv2_ = registerSignal("cqiDlTxDiv2");
        cqiDlTxDiv3_ = registerSignal("cqiDlTxDiv3");
        cqiDlTxDiv4_ = registerSignal("cqiDlTxDiv4");

        cqiDlSpmux0_ = registerSignal("cqiDlSpmux0");
        cqiDlSpmux1_ = registerSignal("cqiDlSpmux1");
        cqiDlSpmux2_ = registerSignal("cqiDlSpmux2");
        cqiDlSpmux3_ = registerSignal("cqiDlSpmux3");
        cqiDlSpmux4_ = registerSignal("cqiDlSpmux4");

        cqiDlSiso0_ = registerSignal("cqiDlSiso0");
        cqiDlSiso1_ = registerSignal("cqiDlSiso1");
        cqiDlSiso2_ = registerSignal("cqiDlSiso2");
        cqiDlSiso3_ = registerSignal("cqiDlSiso3");
        cqiDlSiso4_ = registerSignal("cqiDlSiso4");

        tSample_->module_ = check_and_cast<cComponent*>(this);
        tSample_->id_ = nodeId_;

        eNodeBCount = par("eNodeBCount");
        WATCH(numAntennas_);
        WATCH_MAP(bsrbuf_);
    } else if (stage == 1) {
        /* Create and initialize AMC module */
        amc_ = new LteAmc(this, binder_, deployer_, numAntennas_);

        /* Insert EnbInfo in the Binder */
        EnbInfo* info = new EnbInfo();
        info->id = nodeId_;            // local mac ID
        info->type = MACRO_ENB;        // eNb Type
        info->init = false;            // flag for phy initialization
        info->eNodeB = this->getParentModule()->getParentModule(); // reference to the eNodeB module
        binder_->addEnbInfo(info);

        // register the pair <id,name> to the binder
        const char* moduleName =
                getParentModule()->getParentModule()->getName();
        binder_->registerName(nodeId_, moduleName);
    }

    if (stage == 0) // Added from LteMacUeRealisticAutoD2D
            {
        // check the RLC module type: if it is not "RealisticD2D", abort simulation
        std::string pdcpType =
                getParentModule()->par("LtePdcpRrcType").stdstringValue();
        cModule* rlc = getParentModule()->getSubmodule("rlc");
        std::string rlcUmType = rlc->par("LteRlcUmType").stdstringValue();
        bool rlcD2dCapable = rlc->par("d2dCapable").boolValue();
        std::string macType =
                getParentModule()->par("LteMacType").stdstringValue();
        if (macType.compare("LteMacUeAutoD2DRealistic") == 0
                && rlcUmType.compare("LteRlcUmRealistic") != 0)
            throw cRuntimeError(
                    "LteMacUeAutoD2DRealistic::initialize - %s module found, must be LteRlcUmRealistic. Aborting",
                    rlcUmType.c_str());

        /* Create and initialize MAC Downlink scheduler */
        ueAutoD2DSchedulerDl_ = new LteSchedulerUeAutoD2DDl();
        ueAutoD2DSchedulerDl_->initialize(DL, this);

        /* Create and initialize MAC Uplink scheduler */
        ueAutoD2DSchedulerUl_ = new LteSchedulerUeAutoD2DUl();
        ueAutoD2DSchedulerUl_->initialize(UL, this);

        if (rlcUmType.compare("LteRlcUmRealistic") != 0 || !rlcD2dCapable)
            throw cRuntimeError(
                    "LteMacUeRealisticAutoD2D::initialize - %s module found, must be LteRlcUmRealisticD2D. Aborting",
                    rlcUmType.c_str());
        if (pdcpType.compare("LtePdcpRrcUeD2D") != 0)
            throw cRuntimeError(
                    "LteMacUeRealisticAutoD2D::initialize - %s module found, must be LtePdcpRrcUeD2D. Aborting",
                    pdcpType.c_str());
    }
    if (stage == 1) {
        usePreconfiguredTxParams_ = par("usePreconfiguredTxParams");
        if (usePreconfiguredTxParams_)
            preconfiguredTxParams_ = getPreconfiguredTxParams();
        else
            preconfiguredTxParams_ = NULL;

        // get the reference to the UeAutoD2D
        ueAutoD2D_ =
                check_and_cast<LteMacUeAutoD2D*>(
                        getSimulation()->getModule(
                                binder_->getOmnetId(getMacCellId()))->getSubmodule(
                                "nic")->getSubmodule("mac"));
    }
}

UserTxParams* LteMacUeAutoD2D::getPreconfiguredTxParams() {
    UserTxParams* txParams = new UserTxParams();

    // default parameters for D2D
    txParams->isSet() = true;
    txParams->writeTxMode(TRANSMIT_DIVERSITY);
    Rank ri = 1;                                          // rank for TxD is one
    txParams->writeRank(ri);
    txParams->writePmi(intuniform(1, pow(ri, (double) 2))); // taken from LteFeedbackComputationRealistic::computeFeedback

    Cqi cqi = par("d2dCqi");
    if (cqi < 0 || cqi > 15)
        throw cRuntimeError(
                "LteMacUeAutoD2D::getPreconfiguredTxParams - CQI %s is not a valid value. Aborting",
                cqi);
    txParams->writeCqi(std::vector<Cqi>(1, cqi));

    BandSet b;
    for (Band i = 0; i < getDeployer(nodeId_)->getNumBands(); ++i)
        b.insert(i); // have to check the implemenattion

    RemoteSet antennas;
    antennas.insert(MACRO);
    txParams->writeAntennas(antennas);

    return txParams;
}

void LteMacUeAutoD2D::handleMessage(cMessage* msg) {
    if (msg->isSelfMessage()) {
        handleMessage(msg);
        return;
    }

    cPacket* pkt = check_and_cast<cPacket *>(msg);
    cGate* incoming = pkt->getArrivalGate();

    if (incoming == down_[IN]) {
        UserControlInfo *userInfo = check_and_cast<UserControlInfo *>(
                pkt->getControlInfo());
        if (userInfo->getFrameType() == D2DMODESWITCHPKT) {
            EV << "LteMacUeAutoD2D::handleMessage - Received packet "
                      << pkt->getName() << " from port "
                      << pkt->getArrivalGate()->getName() << endl;

            // message from PHY_to_MAC gate (from lower layer)
            emit(receivedPacketFromLowerLayer, pkt);

            // handle D2D Mode Switch packet
            macHandleD2DModeSwitch(pkt);

            return;
        }
    }

    handleMessage(msg);
}

void LteMacUeAutoD2D::handleSelfMessage() {
    EV << "----- D2D UE MAIN LOOP -----" << endl;

    // extract pdus from all harqrxbuffers and pass them to unmaker
    HarqRxBuffers::iterator hit = harqRxBuffers_.begin();
    HarqRxBuffers::iterator het = harqRxBuffers_.end();
    LteMacPdu *pdu = NULL;
    std::list<LteMacPdu*> pduList;

    for (; hit != het; ++hit) {
        pduList = hit->second->extractCorrectPdus();
        while (!pduList.empty()) {
            pdu = pduList.front();
            pduList.pop_front();
            macPduUnmake(pdu);
        }
    }

    // For each D2D communication, the status of the HARQRxBuffer must be known to the eNB
    // For each HARQ-RX Buffer corresponding to a D2D communication, store "mirror" buffer at the eNB
    HarqRxBuffers::iterator buffIt = harqRxBuffers_.begin();
    for (; buffIt != harqRxBuffers_.end(); ++buffIt) {
        MacNodeId senderId = buffIt->first;
        LteHarqBufferRx* buff = buffIt->second;

        // skip the H-ARQ buffer corresponding to DL transmissions
        if (senderId == cellId_)
            continue;

        // TODO skip the H-ARQ buffers corresponding to D2D_MULTI transmissions

        //The constructor "extracts" all the useful information from the harqRxBuffer and put them in a LteHarqBufferRxD2DMirror object
        //That object resides in enB. Because this operation is done after the enb main loop the enb is 1 TTI backward respect to the Receiver
        //This means that enb will check the buffer for retransmission 3 TTI before
        LteHarqBufferRxD2DMirror* mirbuff = new LteHarqBufferRxD2DMirror(buff,
                (unsigned char) this->par("maxHarqRtx"), senderId);
        ueAutoD2D_->storeRxHarqBufferMirror(nodeId_, mirbuff);
    }

    EV << NOW << "LteMacUeAutoD2D::handleSelfMessage " << nodeId_
              << " - HARQ process " << (unsigned int) currentHarq_ << endl;
    // updating current HARQ process for next TTI

    //unsigned char currentHarq = currentHarq_;

    // no grant available - if user has backlogged data, it will trigger scheduling request
    // no harq counter is updated since no transmission is sent.

    if (schedulingGrant_ == NULL) {
        EV << NOW << " LteMacUeAutoD2D::handleSelfMessage " << nodeId_
                  << " NO configured grant" << endl;

//        if (!bsrTriggered_)
//        {
        // if necessary, a RAC request will be sent to obtain a grant
        checkRAC();
//        }
        // TODO ensure all operations done  before return ( i.e. move H-ARQ rx purge before this point)
    } else if (schedulingGrant_->getPeriodic()) {
        // Periodic checks
        if (--expirationCounter_ < 0) {
            // Periodic grant is expired
            delete schedulingGrant_;
            schedulingGrant_ = NULL;
            // if necessary, a RAC request will be sent to obtain a grant
            checkRAC();
            //return;
        } else if (--periodCounter_ > 0) {
            return;
        } else {
            // resetting grant period
            periodCounter_ = schedulingGrant_->getPeriod();
            // this is periodic grant TTI - continue with frame sending
        }
    }

    if (schedulingGrant_ != NULL) // if a grant is configured
    {
        EV << NOW
                  << " LteMacUeAutoD2D::handleSelfMessage - Grant received, dir["
                  << dirToA(schedulingGrant_->getDirection()) << "]" << endl;
        if (!firstTx) {
            EV << "\t currentHarq_ counter initialized " << endl;
            firstTx = true;
            // the eNb will receive the first pdu in 2 TTI, thus initializing acid to 0
//            currentHarq_ = harqRxBuffers_.begin()->second->getProcesses() - 2;
            currentHarq_ = UE_TX_HARQ_PROCESSES - 2;
        }
        LteMacScheduleList* scheduleList = NULL;
        EV << "\t " << schedulingGrant_ << endl;

//        //! \TEST  Grant Synchronization check
//        if (!(schedulingGrant_->getPeriodic()))
//        {
//            if ( false /* TODO currentHarq!=grant_->getAcid()*/)
//            {
//                EV << NOW << "FATAL! Ue " << nodeId_ << " Current Process is " << (int)currentHarq << " while Stored grant refers to acid " << /*(int)grant_->getAcid() << */  ". Aborting.   " << endl;
//                abort();
//            }
//        }

        // TODO check if current grant is "NEW TRANSMISSION" or "RETRANSMIT" (periodic grants shall always be "newtx"
//        if ( false/*!grant_->isNewTx() && harqQueue_->rtx(currentHarq) */)
//        {
        //        if ( LteDebug:r:trace("LteMacUe::newSubFrame") )
        //            fprintf (stderr,"%.9f UE: [%d] Triggering retransmission for acid %d\n",NOW,nodeId_,currentHarq);
        //        // triggering retransmission --- nothing to do here, really!
//        } else {
        // buffer drop should occour here.
//        scheduleList = ueScheduler_->buildSchedList();

        EV << NOW << " LteMacUeAutoD2D::handleSelfMessage " << nodeId_
                  << " entered scheduling" << endl;

        bool retx = false;

        HarqTxBuffers::iterator it2;
        LteHarqBufferTx * currHarq;
        for (it2 = harqTxBuffers_.begin(); it2 != harqTxBuffers_.end(); it2++) {
            EV << "\t Looking for retx in acid " << (unsigned int) currentHarq_
                      << endl;
            currHarq = it2->second;

            // check if the current process has unit ready for retx
            bool ready = currHarq->getProcess(currentHarq_)->hasReadyUnits();
            CwList cwListRetx =
                    currHarq->getProcess(currentHarq_)->readyUnitsIds();

            EV << "\t [process=" << (unsigned int) currentHarq_ << "] , [retx="
                      << ((ready) ? "true" : "false") << "] , [n="
                      << cwListRetx.size() << "]" << endl;

            // if a retransmission is needed
            if (ready) {
                UnitList signal;
                signal.first = currentHarq_;
                signal.second = cwListRetx;
                currHarq->markSelected(signal,
                        schedulingGrant_->getUserTxParams()->getLayers().size());
                retx = true;
            }
        }
        // if no retx is needed, proceed with normal scheduling
        if (!retx) {
//            scheduleList = lcgScheduler_->schedule();
//            macPduMake(scheduleList);
        }

        // send the selected units to lower layers
        for (it2 = harqTxBuffers_.begin(); it2 != harqTxBuffers_.end(); it2++)
            it2->second->sendSelectedDown();

        // deleting non-periodic grant
        if (!schedulingGrant_->getPeriodic()) {
            delete schedulingGrant_;
            schedulingGrant_ = NULL;
        }
    }

    //============================ DEBUG ==========================
    HarqTxBuffers::iterator it;

    EV << "\n htxbuf.size " << harqTxBuffers_.size() << endl;

    int cntOuter = 0;
    int cntInner = 0;
    for (it = harqTxBuffers_.begin(); it != harqTxBuffers_.end(); it++) {
        LteHarqBufferTx* currHarq = it->second;
        BufferStatus harqStatus = currHarq->getBufferStatus();
        BufferStatus::iterator jt = harqStatus.begin(), jet = harqStatus.end();

        EV << "\t cicloOuter " << cntOuter << " - bufferStatus.size="
                  << harqStatus.size() << endl;
        for (; jt != jet; ++jt) {
            EV << "\t\t cicloInner " << cntInner << " - jt->size=" << jt->size()
                      << " - statusCw(0/1)=" << jt->at(0).second << "/"
                      << jt->at(1).second << endl;
        }
    }
    //======================== END DEBUG ==========================

    unsigned int purged = 0;
    // purge from corrupted PDUs all Rx H-HARQ buffers
    for (hit = harqRxBuffers_.begin(); hit != het; ++hit) {
        // purge corrupted PDUs only if this buffer is for a DL transmission. Otherwise, if you
        // purge PDUs for D2D communication, also "mirror" buffers will be purged
        if (hit->first == cellId_)
            purged += hit->second->purgeCorruptedPdus();
    }
    EV << NOW << " LteMacUe::handleSelfMessage Purged " << purged << " PDUS"
              << endl;

    // update current harq process id
    currentHarq_ = (currentHarq_ + 1) % harqProcesses_;

    EV << "--- END UE MAIN LOOP ---" << endl;
}

void LteMacUeAutoD2D::macPduMake(LteMacScheduleList* scheduleList) {
    EV << NOW << " LteMacUeAutoD2D::macPduMake - Start " << endl;
    int64 size = 0;

    macPduList_.clear();

    bool bsrAlreadyMade = false;

    //TODO add a parameter for discriminates if the UE is actually making or not a D2D communication
    if ((bsrTriggered_ || bsrD2DMulticastTriggered_)
            && schedulingGrant_->getDirection() == UL
            && scheduleList->empty()) {
        // compute the size of the BSR taking into account DM packets only.
        // that info is not available in virtual buffers, thus use real buffers
        int sizeBsr = 0;
        LteMacBuffers::iterator itbuf;
        for (itbuf = mbuf_.begin(); itbuf != mbuf_.end(); itbuf++) {
//            // scan the packets stored in the buffers
//            for (int i=0; i < itbuf->second->getQueueLength(); ++i)
//            {
//                cPacket* pkt = itbuf->second->get(i);
//                FlowControlInfo* pktInfo = check_and_cast<FlowControlInfo*>(pkt->getControlInfo());
//
//                // check whether is a DM or a IM packet
//                if (pktInfo->getDirection() == D2D)
//                    sizeBsr += pkt->getByteLength();
//
//            }
            MacCid cid = itbuf->first;
            Direction connDir = (Direction) connDesc_[cid].getDirection();

            // if the bsr was triggered by D2D (D2D_MULTI), only account for D2D (D2D_MULTI) connections
            if (bsrTriggered_ && connDir != D2D)
                continue;
            if (bsrD2DMulticastTriggered_ && connDir != D2D_MULTI)
                continue;
            sizeBsr += itbuf->second->getQueueOccupancy();
        }

        if (sizeBsr != 0) {
            // Call the appropriate function for make a BSR for a D2D communication
            LogicalCid lcid =
                    (bsrD2DMulticastTriggered_) ?
                            D2D_MULTI_SHORT_BSR : D2D_SHORT_BSR;
            LteMacPdu* macPktBsr = makeBsr(sizeBsr);
            UserControlInfo* info = check_and_cast<UserControlInfo*>(
                    macPktBsr->getControlInfo());
            info->setLcid(lcid);

            // Add the created BSR to the PDU List
            if (macPktBsr != NULL) {
                macPduList_[std::pair<MacNodeId, Codeword>(getMacCellId(), 0)] =
                        macPktBsr;
                bsrAlreadyMade = true;
                EV << "LteMacUeAutoD2D::macPduMake - BSR D2D created with size "
                          << sizeBsr << "created" << endl;

            }
        }
    }

    if (!bsrAlreadyMade) {
        //  Build a MAC pdu for each scheduled user on each codeword
        LteMacScheduleList::const_iterator it;
        for (it = scheduleList->begin(); it != scheduleList->end(); it++) {
            LteMacPdu* macPkt;
            cPacket* pkt;

            MacCid destCid = it->first.first;
            Codeword cw = it->first.second;

            // get the direction (UL/D2D/D2D_MULTI) and the corresponding destination ID
            FlowControlInfo* lteInfo = &(connDesc_.at(destCid));
            MacNodeId destId = lteInfo->getDestId();
            Direction dir = (Direction) lteInfo->getDirection();

            std::pair<MacNodeId, Codeword> pktId =
                    std::pair<MacNodeId, Codeword>(destId, cw);
            unsigned int sduPerCid = it->second;

            MacPduList::iterator pit = macPduList_.find(pktId);

            if (sduPerCid == 0 && !bsrTriggered_
                    && !bsrD2DMulticastTriggered_) {
                continue;
            }

            // No packets for this user on this codeword
            if (pit == macPduList_.end()) {
                UserControlInfo* uinfo = new UserControlInfo();
                uinfo->setSourceId(getMacNodeId());
                uinfo->setDestId(destId);
                uinfo->setDirection(dir);
                uinfo->setLcid(MacCidToLcid(SHORT_BSR));
                if (usePreconfiguredTxParams_)
                    uinfo->setUserTxParams(preconfiguredTxParams_->dup());
                else
                    uinfo->setUserTxParams(
                            schedulingGrant_->getUserTxParams()->dup());
                macPkt = new LteMacPdu("LteMacPdu");
                macPkt->setHeaderLength(MAC_HEADER);
                macPkt->setControlInfo(uinfo);
                macPkt->setTimestamp(NOW);
                macPduList_[pktId] = macPkt;
            } else {
                macPkt = pit->second;
            }

            while (sduPerCid > 0) {
                // Add SDU to PDU
                // Find Mac Pkt

                if (mbuf_.find(destCid) == mbuf_.end())
                    throw cRuntimeError("Unable to find mac buffer for cid %d",
                            destCid);

                if (mbuf_[destCid]->empty())
                    throw cRuntimeError(
                            "Empty buffer for cid %d, while expected SDUs were %d",
                            destCid, sduPerCid);

                pkt = mbuf_[destCid]->popFront();

                // multicast support
                // this trick gets the group ID from the MAC SDU and sets it in the MAC PDU
                int32 groupId = check_and_cast<LteControlInfo*>(
                        pkt->getControlInfo())->getMulticastGroupId();
                if (groupId >= 0) // for unicast, group id is -1
                    check_and_cast<LteControlInfo*>(macPkt->getControlInfo())->setMulticastGroupId(
                            groupId);

                drop(pkt);
                macPkt->pushSdu(pkt);
                sduPerCid--;
            }
            size += mbuf_[destCid]->getQueueOccupancy();
        }
    }

    //  Put MAC pdus in H-ARQ buffers
    MacPduList::iterator pit;
    for (pit = macPduList_.begin(); pit != macPduList_.end(); pit++) {
        MacNodeId destId = pit->first.first;
        Codeword cw = pit->first.second;

        LteHarqBufferTx* txBuf;
        HarqTxBuffers::iterator hit = harqTxBuffers_.find(destId);
        if (hit != harqTxBuffers_.end()) {
            // the tx buffer already exists
            txBuf = hit->second;
        } else {
            // the tx buffer does not exist yet for this mac node id, create one
            // FIXME: hb is never deleted
            LteHarqBufferTx* hb;
            UserControlInfo* info = check_and_cast<UserControlInfo*>(
                    pit->second->getControlInfo());
            if (info->getDirection() == UL)
                hb = new LteHarqBufferTx((unsigned int) ENB_TX_HARQ_PROCESSES,
                        this, (LteMacBase*) getMacByMacNodeId(destId));
            else
                // D2D or D2D_MULTI
                hb = new LteHarqBufferTxD2D(
                        (unsigned int) ENB_TX_HARQ_PROCESSES, this,
                        (LteMacBase*) getMacByMacNodeId(destId));

            harqTxBuffers_[destId] = hb;
            txBuf = hb;
        }
        //
//        UnitList txList = (txBuf->firstAvailable());
//        LteHarqProcessTx * currProc = txBuf->getProcess(currentHarq_);

        // search for an empty unit within current harq process
        UnitList txList = txBuf->getEmptyUnits(currentHarq_);
//        EV << "LteMacUeAutoD2D::macPduMake - [Used Acid=" << (unsigned int)txList.first << "] , [curr=" << (unsigned int)currentHarq_ << "]" << endl;

        LteMacPdu* macPkt = pit->second;

        // BSR related operations

        // according to the TS 36.321 v8.7.0, when there are uplink resources assigned to the UE, a BSR
        // has to be send even if there is no data in the user's queues. In few words, a BSR is always
        // triggered and has to be send when there are enough resources

        // TODO implement differentiated BSR attach
        //
        //            // if there's enough space for a LONG BSR, send it
        //            if( (availableBytes >= LONG_BSR_SIZE) ) {
        //                // Create a PDU if data were not scheduled
        //                if (pdu==0)
        //                    pdu = new LteMacPdu();
        //
        //                if(LteDebug::trace("LteSchedulerUeUl::schedule") || LteDebug::trace("LteSchedulerUeUl::schedule@bsrTracing"))
        //                    fprintf(stderr, "%.9f LteSchedulerUeUl::schedule - Node %d, sending a Long BSR...\n",NOW,nodeId);
        //
        //                // create a full BSR
        //                pdu->ctrlPush(fullBufferStatusReport());
        //
        //                // do not reset BSR flag
        //                mac_->bsrTriggered() = true;
        //
        //                availableBytes -= LONG_BSR_SIZE;
        //
        //            }
        //
        //            // if there's space only for a SHORT BSR and there are scheduled flows, send it
        //            else if( (mac_->bsrTriggered() == true) && (availableBytes >= SHORT_BSR_SIZE) && (highestBackloggedFlow != -1) ) {
        //
        //                // Create a PDU if data were not scheduled
        //                if (pdu==0)
        //                    pdu = new LteMacPdu();
        //
        //                if(LteDebug::trace("LteSchedulerUeUl::schedule") || LteDebug::trace("LteSchedulerUeUl::schedule@bsrTracing"))
        //                    fprintf(stderr, "%.9f LteSchedulerUeUl::schedule - Node %d, sending a Short/Truncated BSR...\n",NOW,nodeId);
        //
        //                // create a short BSR
        //                pdu->ctrlPush(shortBufferStatusReport(highestBackloggedFlow));
        //
        //                // do not reset BSR flag
        //                mac_->bsrTriggered() = true;
        //
        //                availableBytes -= SHORT_BSR_SIZE;
        //
        //            }
        //            // if there's a BSR triggered but there's not enough space, collect the appropriate statistic
        //            else if(availableBytes < SHORT_BSR_SIZE && availableBytes < LONG_BSR_SIZE) {
        //                Stat::put(LTE_BSR_SUPPRESSED_NODE,nodeId,1.0);
        //                Stat::put(LTE_BSR_SUPPRESSED_CELL,mac_->cellId(),1.0);
        //            }
        //            Stat::put (LTE_GRANT_WASTED_BYTES_UL, nodeId, availableBytes);
        //        }
        //
        //        // 4) PDU creation
        //
        //        if (pdu!=0) {
        //
        //            pdu->cellId() = mac_->cellId();
        //            pdu->nodeId() = nodeId;
        //            pdu->direction() = mac::UL;
        //            pdu->error() = false;
        //
        //            if(LteDebug::trace("LteSchedulerUeUl::schedule"))
        //                fprintf(stderr, "%.9f LteSchedulerUeUl::schedule - Node %d, creating uplink PDU.\n", NOW, nodeId);
        //
        //        }

        // Attach BSR to PDU if RAC is won and it wasn't already made
        if ((bsrTriggered_ || bsrD2DMulticastTriggered_) && !bsrAlreadyMade) {
            MacBsr* bsr = new MacBsr();
            bsr->setTimestamp(simTime().dbl());
            bsr->setSize(size);
            macPkt->pushCe(bsr);
            bsrTriggered_ = false;
            bsrD2DMulticastTriggered_ = false;
            EV << "LteMacUeAutoD2D::macPduMake - BSR with size " << size
                      << "created" << endl;
        }

        EV << "LteMacBase: pduMaker created PDU: " << macPkt->info() << endl;

        // TODO: harq test
        // pdu transmission here (if any)
        // txAcid has HARQ_NONE for non-fillable codeword, acid otherwise
        if (txList.second.empty()) {
            EV
                      << "macPduMake() : no available process for this MAC pdu in TxHarqBuffer"
                      << endl;
            delete macPkt;
        } else {
            txBuf->insertPdu(txList.first, cw, macPkt);
        }
    }
}

LteMacPdu* LteMacUeAutoD2D::makeBsr(int size) {
    UserControlInfo* uinfo = new UserControlInfo();
    uinfo->setSourceId(getMacNodeId());
    uinfo->setDestId(getMacCellId());
    uinfo->setDirection(UL);
    uinfo->setUserTxParams(schedulingGrant_->getUserTxParams()->dup());
    LteMacPdu* macPkt = new LteMacPdu("LteMacPdu");
    macPkt->setHeaderLength(MAC_HEADER);
    macPkt->setControlInfo(uinfo);
    macPkt->setTimestamp(NOW);
    MacBsr* bsr = new MacBsr();
    bsr->setTimestamp(simTime().dbl());
    bsr->setSize(size);
    macPkt->pushCe(bsr);
    bsrTriggered_ = false;
    bsrD2DMulticastTriggered_ = false;
    EV << "LteMacUeAutoD2D::makeBsr() - BSR with size " << size << "created"
              << endl;
    return macPkt;
}

void LteMacUeAutoD2D::macHandleD2DModeSwitch(cPacket* pkt) {
    EV << NOW << " LteMacUeAutoD2D::macHandleD2DModeSwitch - Start" << endl;

    // add here specific behavior for handling mode switch at the MAC layer

    // here, all data in the MAC buffers of the connection to be switched are deleted

    D2DModeSwitchNotification* switchPkt = check_and_cast<
            D2DModeSwitchNotification*>(pkt);
    bool txSide = switchPkt->getTxSide();
    MacNodeId peerId = switchPkt->getPeerId();
    LteD2DMode newMode = switchPkt->getNewMode();
    LteD2DMode oldMode = switchPkt->getOldMode();

    UserControlInfo* uInfo = check_and_cast<UserControlInfo*>(
            pkt->removeControlInfo());

    if (txSide) {
        Direction newDirection = (newMode == DM) ? D2D : UL;
        Direction oldDirection = (oldMode == DM) ? D2D : UL;

        // find the correct connection involved in the mode switch
        MacCid cid;
        FlowControlInfo* lteInfo = NULL;
        std::map<MacCid, FlowControlInfo>::iterator it = connDesc_.begin();
        for (; it != connDesc_.end();) {
            cid = it->first;
            lteInfo = check_and_cast<FlowControlInfo*>(&(it->second));
            if (lteInfo->getD2dRxPeerId() == peerId
                    && (Direction) lteInfo->getDirection() == oldDirection) {
                EV << NOW
                          << " LteMacUeAutoD2D::macHandleD2DModeSwitch - found connection with cid "
                          << cid << ", erasing buffered data" << endl;
                if (oldDirection != newDirection) {
                    // empty virtual buffer for the selected cid
                    LteMacBufferMap::iterator macBuff_it = macBuffers_.find(
                            cid);
                    if (macBuff_it != macBuffers_.end()) {
                        while (!(macBuff_it->second->isEmpty()))
                            macBuff_it->second->popFront();
                        macBuffers_.erase(macBuff_it);
                    }

                    // empty real buffer for the selected cid (they should be already empty)
                    LteMacBuffers::iterator mBuf_it = mbuf_.find(cid);
                    if (mBuf_it != mbuf_.end()) {
                        while (mBuf_it->second->getQueueLength() > 0) {
                            cPacket* pdu = mBuf_it->second->popFront();
                            delete pdu;
                        }
                        delete mBuf_it->second;
                        mbuf_.erase(mBuf_it);
                    }

                    // interrupt H-ARQ processes
                    HarqTxBuffers::iterator hit = harqTxBuffers_.find(peerId);
                    if (hit != harqTxBuffers_.end()) {
                        for (int proc = 0;
                                proc < (unsigned int) UE_TX_HARQ_PROCESSES;
                                proc++) {
                            hit->second->forceDropProcess(proc);
                        }
                    }
                }

                D2DModeSwitchNotification* switchPkt_dup = switchPkt->dup();
                switchPkt_dup->setControlInfo(lteInfo->dup());
                sendUpperPackets(switchPkt_dup);

                if (oldDirection != newDirection) {
                    // remove connection descriptor
                    connDesc_.erase(it++);

                    // remove entry from lcgMap
                    LcgMap::iterator lt = lcgMap_.begin();
                    for (; lt != lcgMap_.end();) {
                        if (lt->second.first == cid) {
                            lcgMap_.erase(lt++);
                        } else {
                            ++lt;
                        }
                    }
                } else {
                    ++it;
                }
                EV << NOW
                          << " LteMacUeAutoD2D::macHandleD2DModeSwitch - send switch signal to the RLC TX entity corresponding to the old mode, cid "
                          << cid << endl;
            } else {
                ++it;
            }
        }
    } else   // rx side
    {
        Direction newDirection = (newMode == DM) ? D2D : DL;
        Direction oldDirection = (oldMode == DM) ? D2D : DL;

        // find the correct connection involved in the mode switch
        MacCid cid;
        FlowControlInfo* lteInfo = NULL;
        std::map<MacCid, FlowControlInfo>::iterator it = connDescIn_.begin();
        for (; it != connDescIn_.end();) {
            cid = it->first;
            lteInfo = check_and_cast<FlowControlInfo*>(&(it->second));
            if (lteInfo->getD2dTxPeerId() == peerId
                    && (Direction) lteInfo->getDirection() == oldDirection) {
                if (oldDirection != newDirection) {
                    // interrupt H-ARQ processes
                    HarqRxBuffers::iterator hit = harqRxBuffers_.find(peerId);
                    if (hit != harqRxBuffers_.end()) {
                        EV << NOW
                                  << " LteMacUeAutoD2D::macHandleD2DModeSwitch - Reset H-ARQ RX buffer"
                                  << endl;
                        for (unsigned int proc = 0;
                                proc < (unsigned int) UE_RX_HARQ_PROCESSES;
                                proc++) {
                            unsigned int numUnits = hit->second->getProcess(
                                    proc)->getNumHarqUnits();
                            for (unsigned int i = 0; i < numUnits; i++) {

                                hit->second->getProcess(proc)->purgeCorruptedPdu(
                                        i); // delete contained PDU
                                hit->second->getProcess(proc)->resetCodeword(i); // reset unit
                            }
                        }
                    }
                    ueAutoD2D_->deleteRxHarqBufferMirror(nodeId_);

                    EV << NOW
                              << " LteMacUeAutoD2D::macHandleD2DModeSwitch - Remove connection descriptor for RX flow"
                              << endl;
                    // remove connection descriptor
                    connDescIn_.erase(it++);
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }

    delete uInfo;
    delete pkt;
}

void LteMacUeAutoD2D::macHandleGrant(cPacket* pkt) {
    // clearing pending RAC requests for multicast connections
    racD2DMulticastRequested_ = false;
    macHandleGrant(pkt);
}

void LteMacUeAutoD2D::checkRAC() {
    EV << NOW << " LteMacUeAutoD2D::checkRAC , Ue  " << nodeId_
              << ", racTimer : " << racBackoffTimer_ << " maxRacTryOuts : "
              << maxRacTryouts_ << ", raRespTimer:" << raRespTimer_ << endl;

    if (racBackoffTimer_ > 0) {
        racBackoffTimer_--;
        return;
    }

    if (raRespTimer_ > 0) {
        // decrease RAC response timer
        raRespTimer_--;
        EV << NOW
                  << " LteMacUeAutoD2D::checkRAC - waiting for previous RAC requests to complete (timer="
                  << raRespTimer_ << ")" << endl;
        return;
    }

    // Avoids double requests whithin same TTI window
    if (racRequested_) {
        EV << NOW << " LteMacUeAutoD2D::checkRAC - double RAC request" << endl;
        racRequested_ = false;
        return;
    }
    if (racD2DMulticastRequested_) {
        EV << NOW << " LteMacUeAutoD2D::checkRAC - double RAC request" << endl;
        racD2DMulticastRequested_ = false;
        return;
    }

    bool trigger = false;
    bool triggerD2DMulticast = false;

    LteMacBufferMap::const_iterator it;

    for (it = macBuffers_.begin(); it != macBuffers_.end(); ++it) {
        if (!(it->second->isEmpty())) {
            MacCid cid = it->first;
            if (connDesc_.at(cid).getDirection() == D2D_MULTI)
                triggerD2DMulticast = true;
            else
                trigger = true;
            break;
        }
    }

    if (!trigger && !triggerD2DMulticast)
        EV << NOW << "Ue " << nodeId_ << ",RAC aborted, no data in queues "
                  << endl;

    if ((racRequested_ = trigger) || (racD2DMulticastRequested_ =
            triggerD2DMulticast)) {
        LteRac* racReq = new LteRac("RacRequest");
        UserControlInfo* uinfo = new UserControlInfo();
        uinfo->setSourceId(getMacNodeId());
        uinfo->setDestId(getMacCellId());
        uinfo->setDirection(UL);
        uinfo->setFrameType(RACPKT);
        racReq->setControlInfo(uinfo);

        sendLowerPackets(racReq);

        EV << NOW << " Ue  " << nodeId_ << " cell " << cellId_
                  << " ,RAC request sent to PHY " << endl;

        // wait at least  "raRespWinStart_" TTIs before another RAC request
        raRespTimer_ = raRespWinStart_;
    }
}

void LteMacUeAutoD2D::macHandleRac(cPacket* pkt) {
    LteRac* racPkt = check_and_cast<LteRac*>(pkt);

    if (racPkt->getSuccess()) {
        EV << "LteMacUeAutoD2D::macHandleRac - Ue " << nodeId_ << " won RAC"
                  << endl;
        // is RAC is won, BSR has to be sent
        if (racD2DMulticastRequested_) {
            bsrD2DMulticastTriggered_ = true;
        } else
            bsrTriggered_ = true;

        // reset RAC counter
        currentRacTry_ = 0;
        //reset RAC backoff timer
        racBackoffTimer_ = 0;
    } else {
        // RAC has failed
        if (++currentRacTry_ >= maxRacTryouts_) {
            EV << NOW << " Ue " << nodeId_ << ", RAC reached max attempts : "
                      << currentRacTry_ << endl;
            // no more RAC allowed
            //! TODO flush all buffers here
            //reset RAC counter
            currentRacTry_ = 0;
            //reset RAC backoff timer
            racBackoffTimer_ = 0;
        } else {
            // recompute backoff timer
            racBackoffTimer_ = uniform(minRacBackoff_, maxRacBackoff_);
            EV << NOW << " Ue " << nodeId_
                      << " RAC attempt failed, backoff extracted : "
                      << racBackoffTimer_ << endl;
        }
    }
    delete racPkt;
}

void LteMacUeAutoD2D::doHandover(MacNodeId targetEnb) {
    ueAutoD2D_ = check_and_cast<LteMacUeAutoD2D*>(getMacByMacNodeId(targetEnb));
    doHandover(targetEnb);
}
void LteMacUeAutoD2D::storeRxHarqBufferMirror(MacNodeId id,
        LteHarqBufferRxD2DMirror* mirbuff) {
    // TODO optimize

    if (harqRxBuffersD2DMirror_.find(id) != harqRxBuffersD2DMirror_.end())
        delete harqRxBuffersD2DMirror_[id];
    harqRxBuffersD2DMirror_[id] = mirbuff;
}

HarqRxBuffersMirror* LteMacUeAutoD2D::getRxHarqBufferMirror() {
    return &harqRxBuffersD2DMirror_;
}

void LteMacUeAutoD2D::deleteRxHarqBufferMirror(MacNodeId id) {
    HarqRxBuffersMirror::iterator it = harqRxBuffersD2DMirror_.begin(), et =
            harqRxBuffersD2DMirror_.end();
    for (; it != et;) {
        // get current nodeIDs
        MacNodeId destId = it->first;             // Receiver

        if (destId == id) {
            harqRxBuffersD2DMirror_.erase(it++);
        } else {
            ++it;
        }
    }

}

void LteMacUeAutoD2D::sendModeSwitchNotification(MacNodeId srcId,
        MacNodeId dstId, LteD2DMode oldMode, LteD2DMode newMode) {
    Enter_Method("sendModeSwitchNotification");

    // send switch notification to both the tx and rx side of the flow
    D2DModeSwitchNotification* switchPktTx = new D2DModeSwitchNotification("D2DModeSwitchNotification");
    switchPktTx->setTxSide(true);
    switchPktTx->setPeerId(dstId);
    switchPktTx->setOldMode(oldMode);
    switchPktTx->setNewMode(newMode);

    D2DModeSwitchNotification* switchPktRx = new D2DModeSwitchNotification("D2DModeSwitchNotification");
    switchPktRx->setTxSide(false);
    switchPktRx->setPeerId(srcId);
    switchPktRx->setOldMode(oldMode);
    switchPktRx->setNewMode(newMode);

    UserControlInfo* uinfoTx = new UserControlInfo();
    uinfoTx->setSourceId(nodeId_);
    uinfoTx->setDestId(srcId);
    uinfoTx->setFrameType(D2DMODESWITCHPKT);
    switchPktTx->setControlInfo(uinfoTx);

    UserControlInfo* uinfoRx = new UserControlInfo();
    uinfoRx->setSourceId(nodeId_);
    uinfoRx->setDestId(dstId);
    uinfoRx->setFrameType(D2DMODESWITCHPKT);
    switchPktRx->setControlInfo(uinfoRx);

    // send pkt to PHY layer
    sendLowerPackets(switchPktTx);
    sendLowerPackets(switchPktRx);
}

