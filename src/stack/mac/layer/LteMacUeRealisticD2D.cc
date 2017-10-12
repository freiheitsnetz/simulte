//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "stack/mac/layer/LteMacUeRealisticD2D.h"

#include "../scheduling_modules/unassistedHeuristic/LteUnassistedD2DSchedulingAgent.h"
#include "stack/mac/buffer/harq/LteHarqBufferRx.h"
#include "stack/mac/buffer/harq_d2d/LteHarqBufferRxD2D.h"
#include "stack/mac/buffer/LteMacQueue.h"
#include "stack/mac/packet/LteSchedulingGrant.h"
#include "stack/mac/scheduler/LteSchedulerUeUl.h"
#include "stack/mac/layer/LteMacEnbRealistic.h"
#include "stack/mac/buffer/harq_d2d/LteHarqBufferRxD2DMirror.h"
#include "stack/d2dModeSelection/D2DModeSwitchNotification_m.h"
#include "stack/mac/packet/LteRac_m.h"
#include "stack/mac/scheduler/LteSchedulerUeUnassistedD2D.h"
#include "stack/mac/amc/AmcPilotD2D.h"

Define_Module(LteMacUeRealisticD2D);



LteMacUeRealisticD2D::LteMacUeRealisticD2D() :
    LteMacUeRealistic()
{
    racD2DMulticastRequested_ = false;
    bsrD2DMulticastTriggered_ = false;
    lteSchedulerUeUnassistedD2DSLTx_ = NULL;
    lteSchedulerUeUnassistedD2DSLRx_ = NULL;
}

LteMacUeRealisticD2D::~LteMacUeRealisticD2D()
{
    if (par("unassistedD2D")) {
        delete lteSchedulerUeUnassistedD2DSLRx_;
        delete lteSchedulerUeUnassistedD2DSLTx_;
    } else {
        delete lcgScheduler_;
    }
}

void LteMacUeRealisticD2D::initialize(int stage)
{
    LteMacUeRealistic::initialize(stage);
    if (stage == 0)
    {
        // check the RLC module type: if it is not "RealisticD2D", abort simulation
        std::string pdcpType = getParentModule()->par("LtePdcpRrcType").stdstringValue();
        cModule* rlc = getParentModule()->getSubmodule("rlc");
        std::string rlcUmType = rlc->par("LteRlcUmType").stdstringValue();
        bool rlcD2dCapable = rlc->par("d2dCapable").boolValue();
        if (rlcUmType.compare("LteRlcUmRealistic") != 0 || !rlcD2dCapable)
            throw cRuntimeError("LteMacUeRealisticD2D::initialize - %s module found, must be LteRlcUmRealisticD2D. Aborting", rlcUmType.c_str());
        if (pdcpType.compare("LtePdcpRrcUeD2D") != 0)
            throw cRuntimeError("LteMacUeRealisticD2D::initialize - %s module found, must be LtePdcpRrcUeD2D. Aborting", pdcpType.c_str());

        // Enhancement for Unassisted D2D
        if (par("unassistedD2D")) {
            deployer_ = getDeployer();
            numAntennas_ = deployer_->getNumRus() + 1;
            lteSchedulerUeUnassistedD2DSLTx_ = new LteSchedulerUeUnassistedD2D(this); // Should handle SL-UL for Tx D2D
            lteSchedulerUeUnassistedD2DSLTx_->initialize(UL, this);

            lteSchedulerUeUnassistedD2DSLRx_ = new LteSchedulerUeUnassistedD2D(this); // Should handle SL- DL for Rx D2D
            lteSchedulerUeUnassistedD2DSLRx_->initialize(DL, this);

        } else {
            lcgScheduler_ = new LteSchedulerUeUl(this); // Handles Tx only
        }
    }
    if (stage == 1)
    {
        // get parameters
        usePreconfiguredTxParams_ = par("usePreconfiguredTxParams");
        preconfiguredTxParams_ = getPreconfiguredTxParams();
        if (par("unassistedD2D")) {

            /* Create and initialize AMC module */
            amc_ = new LteAmc(this, binder_, deployer_, numAntennas_);
            (amc_->getPilot())->setUnassistedD2DMode(par("unassistedD2D")); // To handle Tx Params in unassissted D2D mode
            amc_->setConnectedUEsMap();

            Cqi d2dCqi = par("d2dCqi");
            if (usePreconfiguredTxParams_)
                check_and_cast<AmcPilotD2D*>(amc_->getPilot())->setPreconfiguredTxParams(d2dCqi);
           }

        // get the reference to the eNB
        enb_ = check_and_cast<LteMacEnbRealisticD2D*>(getSimulation()->getModule(binder_->getOmnetId(getMacCellId()))->getSubmodule("nic")->getSubmodule("mac"));
    }
}

//Function for create only a BSR for the eNB
LteMacPdu* LteMacUeRealisticD2D::makeBsr(int size){

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
    EV << "LteMacUeRealisticD2D::makeBsr() - BSR with size " << size << "created" << endl;
    return macPkt;
}

void LteMacUeRealisticD2D::macPduMake()
{
    int64 size = 0;

    macPduList_.clear();

    bool bsrAlreadyMade = false;
    // TX UE is in D2D-mode and is sending BSR
    // TX UE is in D2D-mode but it received an UL grant (for BSR)
    // TX UE is in D2D-mode and is sending data
    if ((bsrTriggered_ || bsrD2DMulticastTriggered_) && schedulingGrant_->getDirection() == UL && scheduleList_->empty())
    {
        // Compute BSR size taking into account only DM flows
        int sizeBsr = 0;
        LteMacBufferMap::const_iterator itbsr;
        for (itbsr = macBuffers_.begin(); itbsr != macBuffers_.end(); itbsr++)
        {
            MacCid cid = itbsr->first;
            Direction connDir = (Direction)connDesc_[cid].getDirection();

            // if the bsr was triggered by D2D (D2D_MULTI), only account for D2D (D2D_MULTI) connections
            if (bsrTriggered_ && connDir != D2D)
                continue;
            if (bsrD2DMulticastTriggered_ && connDir != D2D_MULTI)
                continue;

            sizeBsr += itbsr->second->getQueueOccupancy();

            // take into account the RLC header size
            if (sizeBsr > 0)
            {
                if (connDesc_[cid].getRlcType() == UM)
                    sizeBsr += RLC_HEADER_UM;
                else if (connDesc_[cid].getRlcType() == AM)
                    sizeBsr += RLC_HEADER_AM;
            }
        }

        if (sizeBsr > 0)
        {
            // Call the appropriate function for make a BSR for a D2D communication
            LteMacPdu* macPktBsr = makeBsr(sizeBsr);
            UserControlInfo* info = check_and_cast<UserControlInfo*>(macPktBsr->getControlInfo());
            if (bsrD2DMulticastTriggered_)
            {
                info->setLcid(D2D_MULTI_SHORT_BSR);
                bsrD2DMulticastTriggered_ = false;
            }
            else
                info->setLcid(D2D_SHORT_BSR);

            // Add the created BSR to the PDU List
            if( macPktBsr != NULL )
            {
               macPduList_[ std::pair<MacNodeId, Codeword>( getMacCellId(), 0) ] = macPktBsr;
               bsrAlreadyMade = true;
               EV << "LteMacUeRealisticD2D::macPduMake - BSR D2D created with size " << sizeBsr << "created" << endl;
            }
        }
        else
        {
            bsrD2DMulticastTriggered_ = false;
            bsrTriggered_ = false;
        }
    }

    if(!bsrAlreadyMade)
    {
        // In a D2D communication if BSR was created above this part isn't executed
        // Build a MAC PDU for each scheduled user on each codeword
        LteMacScheduleList::const_iterator it;
        for (it = scheduleList_->begin(); it != scheduleList_->end(); it++)
        {
            LteMacPdu* macPkt;
            cPacket* pkt;

            MacCid destCid = it->first.first;
            Codeword cw = it->first.second;

            // get the direction (UL/D2D/D2D_MULTI) and the corresponding destination ID
            FlowControlInfo* lteInfo = &(connDesc_.at(destCid));
            MacNodeId destId = lteInfo->getDestId();
            Direction dir = (Direction)lteInfo->getDirection();

            std::pair<MacNodeId, Codeword> pktId = std::pair<MacNodeId, Codeword>(destId, cw);
            unsigned int sduPerCid = it->second;

            MacPduList::iterator pit = macPduList_.find(pktId);

            if (sduPerCid == 0 && !bsrTriggered_ && !bsrD2DMulticastTriggered_)
            {
                continue;
            }

            // No packets for this user on this codeword
            if (pit == macPduList_.end())
            {
                // Always goes here because of the macPduList_.clear() at the beginning
                // Build the Control Element of the MAC PDU
                UserControlInfo* uinfo = new UserControlInfo();
                uinfo->setSourceId(getMacNodeId());
                uinfo->setDestId(destId);
                uinfo->setLcid(MacCidToLcid(destCid));
                uinfo->setDirection(dir);
                uinfo->setLcid(MacCidToLcid(SHORT_BSR));
                if (usePreconfiguredTxParams_)
                    uinfo->setUserTxParams(preconfiguredTxParams_->dup());
                else
                    uinfo->setUserTxParams(schedulingGrant_->getUserTxParams()->dup());
                // Create a PDU
                macPkt = new LteMacPdu("LteMacPdu");
                macPkt->setHeaderLength(MAC_HEADER);
                macPkt->setControlInfo(uinfo);
                macPkt->setTimestamp(NOW);
                macPduList_[pktId] = macPkt;
            }
            else
            {
                // Never goes here because of the macPduList_.clear() at the beginning
                macPkt = pit->second;
            }

            while (sduPerCid > 0)
            {
                // Add SDU to PDU
                // Find Mac Pkt
                if (mbuf_.find(destCid) == mbuf_.end())
                    throw cRuntimeError("Unable to find mac buffer for cid %d", destCid);

                if (mbuf_[destCid]->empty())
                    throw cRuntimeError("Empty buffer for cid %d, while expected SDUs were %d", destCid, sduPerCid);

                if ((mbuf_[destCid]->getQueueLength()) < (int) sduPerCid)
                    {
                        throw cRuntimeError("Abnormal queue length detected while building MAC PDU for cid %d "
                                "Queue real SDU length is %d  while scheduled SDUs are %d",
                                destCid, mbuf_[destCid]->getQueueLength(), sduPerCid);
                    }
                pkt = mbuf_[destCid]->popFront();

                // multicast support
                // this trick gets the group ID from the MAC SDU and sets it in the MAC PDU
                int32 groupId = check_and_cast<LteControlInfo*>(pkt->getControlInfo())->getMulticastGroupId();
                if (groupId >= 0) // for unicast, group id is -1
                    check_and_cast<LteControlInfo*>(macPkt->getControlInfo())->setMulticastGroupId(groupId);

                drop(pkt);

                macPkt->pushSdu(pkt);
                sduPerCid--;
            }

            // consider virtual buffers to compute BSR size
            size += macBuffers_[destCid]->getQueueOccupancy();

            if (size > 0)
            {
                // take into account the RLC header size
                if (connDesc_[destCid].getRlcType() == UM)
                    size += RLC_HEADER_UM;
                else if (connDesc_[destCid].getRlcType() == AM)
                    size += RLC_HEADER_AM;
            }
        }
    }

    // Put MAC PDUs in H-ARQ buffers
    MacPduList::iterator pit;
    for (pit = macPduList_.begin(); pit != macPduList_.end(); pit++)
    {
        MacNodeId destId = pit->first.first;
        Codeword cw = pit->first.second;
        // Check if the HarqTx buffer already exists for the destId
        // Get a reference for the destId TXBuffer
        LteHarqBufferTx* txBuf;
        HarqTxBuffers::iterator hit = harqTxBuffers_.find(destId);
        if ( hit != harqTxBuffers_.end() )
        {
            // The tx buffer already exists
            txBuf = hit->second;
        }
        else
        {
            if(!par("unassistedD2D"))
            {
                // The tx buffer does not exist yet for this mac node id, create one
                LteHarqBufferTx* hb;
                // FIXME: hb is never deleted
                UserControlInfo* info = check_and_cast<UserControlInfo*>(pit->second->getControlInfo());
                if (info->getDirection() == UL)
                    hb = new LteHarqBufferTx((unsigned int) ENB_TX_HARQ_PROCESSES, this, (LteMacBase*) getMacByMacNodeId(destId));
                else // D2D or D2D_MULTI
                    hb = new LteHarqBufferTxD2D((unsigned int) ENB_TX_HARQ_PROCESSES, this, (LteMacBase*) getMacByMacNodeId(destId));
                harqTxBuffers_[destId] = hb;
                txBuf = hb;
            }
            else
            {
                // The tx buffer does not exist yet for this mac node id, create one
                LteHarqBufferTx* hb;
                // FIXME: hb is never deleted
                UserControlInfo* info = check_and_cast<UserControlInfo*>(pit->second->getControlInfo());
                if (info->getDirection() == UL)
                   hb = new LteHarqBufferTxD2D((unsigned int) UE_TX_HARQ_PROCESSES, this, (LteMacBase*) getMacByMacNodeId(destId));
                else // D2D or D2D_MULTI
                {
                    // Has to be handled for Unassisted D2D
                    hb = new LteHarqBufferTxD2D((unsigned int) UE_TX_HARQ_PROCESSES, this, (LteMacBase*) getMacByMacNodeId(destId));
                }
                harqTxBuffers_[destId] = hb;
                txBuf = hb;
            }
        }

        // search for an empty unit within current harq process
        UnitList txList = txBuf->getEmptyUnits(currentHarq_);
        EV << "LteMacUeRealisticD2D::macPduMake - [Used Acid=" << (unsigned int)txList.first << "] , [curr=" << (unsigned int)currentHarq_ << "]" << endl;

        //Get a reference of the LteMacPdu from pit pointer (extract Pdu from the MAP)
        LteMacPdu* macPkt = pit->second;

        /* BSR related operations

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
        //        } */

        // Attach BSR to PDU if RAC is won and wasn't already made
        if ((bsrTriggered_ || bsrD2DMulticastTriggered_) && !bsrAlreadyMade )
        {
            MacBsr* bsr = new MacBsr();
            bsr->setTimestamp(simTime().dbl());
            bsr->setSize(size);
            macPkt->pushCe(bsr);
            bsrTriggered_ = false;
            bsrD2DMulticastTriggered_ = false;
            EV << "LteMacUeRealisticD2D::macPduMake - BSR created with size " << size << endl;
        }

        EV << "LteMacUeRealisticD2D: pduMaker created PDU: " << macPkt->info() << endl;

        // TODO: harq test
        // pdu transmission here (if any)
        // txAcid has HARQ_NONE for non-fillable codeword, acid otherwise
        if (txList.second.empty())
        {
            EV << "LteMacUeRealisticD2D() : no available process for this MAC pdu in TxHarqBuffer" << endl;
            delete macPkt;
        }
        else
        {
            //Insert PDU in the Harq Tx Buffer
            //txList.first is the acid
            txBuf->insertPdu(txList.first,cw, macPkt);
        }
    }
}

void LteMacUeRealisticD2D::handleMessage(cMessage* msg)
{
    if (msg->isSelfMessage())
    {
        LteMacUeRealistic::handleMessage(msg);
        return;
    }

    cPacket* pkt = check_and_cast<cPacket *>(msg);
    cGate* incoming = pkt->getArrivalGate();

    if (incoming == down_[IN])
    {
        UserControlInfo *userInfo = check_and_cast<UserControlInfo *>(pkt->getControlInfo());
        MacNodeId src = userInfo->getSourceId();

        if (userInfo->getFrameType() == D2DMODESWITCHPKT)
        {
            EV << "LteMacUeRealisticD2D::handleMessage - Received packet " << pkt->getName() <<
            " from port " << pkt->getArrivalGate()->getName() << endl;

            // message from PHY_to_MAC gate (from lower layer)
            emit(receivedPacketFromLowerLayer, pkt);

            // call handler
            macHandleD2DModeSwitch(pkt);

            return;
        }
        else if ((userInfo->getFrameType() == GRANTPKT) && (par("unassistedD2D"))) // Created for Unassisted D2D grants by Rx D2D for Tx D2D
         {
            // Allow grant
            EV << NOW << "LteMacUeRealisticD2D::handleMessage node " << nodeId_ << " Received Scheduling Grant pkt from node:: "<< src << endl;
            macHandleGrant(pkt); // Does it have to be enhanced to do FD schedulling??
            return;
         }
        else if ((userInfo->getFrameType() == HARQPKT) && (par("unassistedD2D"))) // Created for both Unassisted D2D Rx and TX HARQ
         {
            // H-ARQ feedback, send it to TX buffer of source
            HarqTxBuffers::iterator htit = harqTxBuffers_.find(src);
            EV << NOW << "LteMacUeRealisticD2D::handleMessage() node " << nodeId_ << " Received HARQ Feedback pkt from node:: "<< src << endl;
            if (htit == harqTxBuffers_.end())
            {
               // if a feedback arrives, a tx buffer must exists (unless it is an handover scenario
               // where the harq buffer was deleted but a feedback was in transit)
               // this case must be taken care of

               if (binder_->hasUeHandoverTriggered(nodeId_) || binder_->hasUeHandoverTriggered(src))
                   return;
               throw cRuntimeError("LteMacUeRealisticD2D::handleMessage(): Received feedback for an unexisting H-ARQ tx buffer");
            }
            LteHarqFeedback *hfbpkt = check_and_cast<LteHarqFeedback *>(pkt);
            htit->second->receiveHarqFeedback(hfbpkt);
            return;
         }
        else if((userInfo->getFrameType() == DATAPKT) && (par("unassistedD2D")))
            {
                // data packet: insert in proper rx buffer
                EV << NOW << "LteMacUeRealisticD2D::handleMessage(): node " << nodeId_ << " Received DATA packet from node:: "<< src << endl;

                LteMacPdu *pdu = check_and_cast<LteMacPdu *>(pkt);
                Codeword cw = userInfo->getCw();
                HarqRxBuffers::iterator hrit = harqRxBuffers_.find(src);
                if (hrit != harqRxBuffers_.end())
                {
                    hrit->second->insertPdu(cw,pdu);
                    EV << NOW << "LteMacUeRealisticD2D::handleMessage(): Inserting DATA packet into existing HARQ Buffers" << endl;
                }
                else
                {
                    // FIXME: possible memory leak
                    LteHarqBufferRx *hrb;
                    if (userInfo->getDirection() == DL || userInfo->getDirection() == UL)
                        hrb = new LteHarqBufferRx(UE_RX_HARQ_PROCESSES, this,src);
                    else // D2D
                        hrb = new LteHarqBufferRxD2D(UE_RX_HARQ_PROCESSES, this,src);

                    harqRxBuffers_[src] = hrb;
                    hrb->insertPdu(cw,pdu);
                    EV << NOW << "LteMacUeRealisticD2D::handleMessage(): Inserting DATA packet into new HARQ Buffers" << endl;
                }
                return;
            }
        else if ((userInfo->getFrameType() == RACPKT) && (par("unassistedD2D"))) // Created for both Unassisted D2D Rx and TX HARQ
        {
            EV << NOW << "LteMacUeRealisticD2D::handleMessage():node " << nodeId_ << " Received RAC packet from node:: "<< src << endl;
            LteRac* racPkt = check_and_cast<LteRac*>(pkt);
            UserControlInfo* uinfo = check_and_cast<UserControlInfo*>(racPkt->getControlInfo());

            //To Do: This has to be verified

            if((pkt->getName() == "RacRequest") || (uinfo->getDirection() == 1)) // Checking for SL UL i.e. Created for Rx D2D
            {
                EV << NOW << "LteMacUeRealisticD2D::handleMessage():node " << nodeId_ << " Received RAC packet request from node:: "<< src << endl;
                macHandleRacRequest(pkt);
                return;
            }
            else // Checking for SL DL i.e. Created for Tx D2D
            {
                if ((RacMap.find(src)) != RacMap.end())
                {
                    //response
                    EV << NOW << "LteMacUeRealisticD2D::handleMessage():node " << nodeId_ << " Received RAC response from node:: "<< src << endl;
                    macHandleRac(pkt);
                    return;
                }
                else{
                    EV << "LteMacUeRealisticD2D::macHandleRac - UE " << nodeId_ << " won RAC from unknown UE node:: "<< src << endl;
                    return;
                }

            }
        }
        else if ((userInfo->getFrameType() == HANDOVERPKT) && (par("unassistedD2D")))
        {
            EV << "LteMacUeRealisticD2D::Recieved Handover packet for UE " << nodeId_ << endl;
            return;
        }
    }
    EV << "LteMacUeRealisticD2D::handleMessage - Shifting to LteMacUeRealistic::handleMessage with packet "<< endl;
    LteMacUeRealistic::handleMessage(msg);
}


// Handled in Tx D2D. Stores grant
void LteMacUeRealisticD2D::macHandleGrant(cPacket* pkt)
{
    EV << NOW << " LteMacUeRealisticD2D::macHandleGrant - UE [" << nodeId_ << "] - Grant received " << endl;

    // delete old grant
    LteSchedulingGrant* grant = check_and_cast<LteSchedulingGrant*>(pkt);

    //Codeword cw = grant->getCodeword();

    if (schedulingGrant_!=NULL)
    {
        delete schedulingGrant_;
        schedulingGrant_ = NULL;
    }

    // store received grant
    schedulingGrant_=grant;

    if (grant->getPeriodic())
    {
        periodCounter_=grant->getPeriod();
        expirationCounter_=grant->getExpiration();
    }

    EV << NOW << "Node " << nodeId_ << " received grant of blocks " << grant->getTotalGrantedBlocks()
       << ", bytes " << grant->getGrantedCwBytes(0) <<" Direction: "<<dirToA(grant->getDirection()) << endl;

    // clearing pending RAC requests
    racRequested_=false;
    racD2DMulticastRequested_=false;
}

//if user has backlogged data, this will trigger scheduling request
void LteMacUeRealisticD2D::checkRAC()
{
    EV << NOW << " LteMacUeRealisticD2D::checkRAC , Ue  " << nodeId_ << ", racTimer : " << racBackoffTimer_ << " maxRacTryOuts : " << maxRacTryouts_
       << ", raRespTimer:" << raRespTimer_ << endl;

    if (racBackoffTimer_>0)
    {
        racBackoffTimer_--;
        return;
    }

    if(raRespTimer_>0)
    {
        // decrease RAC response timer
        raRespTimer_--;
        EV << NOW << " LteMacUeRealisticD2D::checkRAC - waiting for previous RAC requests to complete (timer=" << raRespTimer_ << ")" << endl;
        return;
    }

    // Avoids double requests whithin same TTI window
    if (racRequested_)
    {
        EV << NOW << " LteMacUeRealisticD2D::checkRAC - double RAC request" << endl;
        racRequested_=false;
        return;
    }
    if (racD2DMulticastRequested_)
    {
        EV << NOW << " LteMacUeRealisticD2D::checkRAC - double RAC request" << endl;
        racD2DMulticastRequested_=false;
        return;
    }

    bool trigger=false;
    bool triggerD2DMulticast=false;

    LteMacBufferMap::const_iterator it;

    for (it = macBuffers_.begin(); it!=macBuffers_.end();++it)
    {
        if (!(it->second->isEmpty()))
        {
            MacCid cid = it->first;
            if (connDesc_.at(cid).getDirection() == D2D_MULTI)
                triggerD2DMulticast = true;
            else
                trigger = true;
            break;
        }
    }

    if (!trigger && !triggerD2DMulticast)
        EV << NOW << "Ue " << nodeId_ << ",RAC aborted, no data in queues " << endl;

    if ((racRequested_=trigger) || (racD2DMulticastRequested_=triggerD2DMulticast)) // Assisted mode send the packet to ENodeB
    {
        LteRac* racReq = new LteRac("RacRequest");
        UserControlInfo* uinfo = new UserControlInfo();
        uinfo->setSourceId(getMacNodeId());
        uinfo->setDestId(getMacCellId());
        uinfo->setDirection(UL);
        uinfo->setFrameType(RACPKT);
        racReq->setControlInfo(uinfo);

        sendLowerPackets(racReq);

        EV << NOW << " Ue  " << nodeId_ << " cell " << cellId_ << " ,RAC request sent to eNB through PHY " << endl;

        // wait at least  "raRespWinStart_" TTIs before another RAC request
        raRespTimer_ = raRespWinStart_;
    }
}

//To handle RAC packets by Tx D2D UE from Rx D2D UE
void LteMacUeRealisticD2D::macHandleRac(cPacket* pkt)
{
    LteRac* racPkt = check_and_cast<LteRac*>(pkt);
    UserControlInfo *userInfo = check_and_cast<UserControlInfo *>(pkt->getControlInfo());
    MacNodeId src = userInfo->getSourceId();

    if (racPkt->getSuccess())
    {
        if(par("unassistedD2D") )
        {
            if (RacMap.find(src)  != RacMap.end())
            {
                EV << "LteMacUeRealisticD2D::macHandleRac - Ue " << nodeId_ << " won RAC" << endl;
                // is RAC is won, BSR has to be sent
                if (racD2DMulticastRequested_)
                    bsrD2DMulticastTriggered_=true;
                else
                    bsrTriggered_ = true;

                // reset RAC counter
                RacMap[src].currentRacTry_=0;
                //reset RAC backoff timer
                RacMap[src].racBackoffTimer_=0;
                //set the RAC parameter to true
            }
        }
        else
        {
            EV << "LteMacUeRealisticD2D::macHandleRac - Ue " << nodeId_ << " won RAC" << endl;
            // is RAC is won, BSR has to be sent
            if (racD2DMulticastRequested_)
                bsrD2DMulticastTriggered_=true;
            else
                bsrTriggered_ = true;
            // reset RAC counter
            currentRacTry_=0;
            //reset RAC backoff timer
            racBackoffTimer_=0;
        }
    }
    else
    {
        if(par("unassistedD2D") )
        {
            if (RacMap.find(src)  != RacMap.end())
            {  // RAC has failed
                if (++(RacMap[src].currentRacTry_) >= RacMap[src].maxRacTryouts_)
                {
                    EV << NOW << " Ue " << nodeId_ << ", RAC reached max attempts : " << currentRacTry_ << "for Rx D2D UE "<< MacCidToNodeId(src)<< endl;
                    // no more RAC allowed
                    //! TODO flush all buffers here
                    //reset RAC counter
                    RacMap[src].currentRacTry_=0;
                    //reset RAC backoff timer
                    RacMap[src].racBackoffTimer_=0;
                }
                else
                {
                    // recompute backoff timer
                    racBackoffTimer_= uniform(minRacBackoff_,maxRacBackoff_);
                    EV << NOW << " Ue " << nodeId_ << " RAC attempt failed, backoff extracted : " << racBackoffTimer_ << endl;
                }
            }
        }
        else
        {
            // RAC has failed
            if (++currentRacTry_ >= maxRacTryouts_)
            {
                EV << NOW << " Ue " << nodeId_ << ", RAC reached max attempts : " << currentRacTry_ << endl;
                // no more RAC allowed
                //! TODO flush all buffers here
                //reset RAC counter
                currentRacTry_=0;
                //reset RAC backoff timer
                racBackoffTimer_=0;
            }
            else
            {
                // recompute backoff timer
                racBackoffTimer_= uniform(minRacBackoff_,maxRacBackoff_);
                EV << NOW << " Ue " << nodeId_ << " RAC attempt failed, backoff extracted : " << racBackoffTimer_ << endl;
            }

        }

    }
    delete racPkt;
}

void LteMacUeRealisticD2D::handleSelfMessage()
{
    EV << "----- UE MAIN LOOP -----" << endl;

    // extract pdus from all harqrxbuffers and pass them to unmaker
    HarqRxBuffers::iterator hit = harqRxBuffers_.begin();
    HarqRxBuffers::iterator het = harqRxBuffers_.end();
    LteMacPdu *pdu = NULL;
    std::list<LteMacPdu*> pduList;

    for (; hit != het; ++hit)
    {
        pduList=hit->second->extractCorrectPdus();
        while (! pduList.empty())
        {
            pdu=pduList.front();
            pduList.pop_front();
            macPduUnmake(pdu);
        }
    }
    if (par("unassistedD2D"))
        {
            // Get device lists.
            std::vector<EnbInfo*>* enbInfo = getBinder()->getEnbList();
//            std::vector<UeInfo*>* ueInfo = getBinder()->getUeList();

            if (enbInfo->size() == 0)
                throw cRuntimeError("LteMacUeRealisticD2D::handleSelfMessage can't get AMC pointer because I couldn't find an eNodeB!");
            // -> its ID -> the node -> cast to the eNodeB class
            MacNodeId mENodeBId = enbInfo->at(0)->id;
            eNodeBFunctions = (ExposedLteMacEnb*) getMacByMacNodeId(mENodeBId);
            //schedule tx D2Ds list
            //sendGrants

            /*To be performed by Rx D2d UE to schedule TX D2D UEs for Sidelink UPLINK*/
            EV << "==============================================SIDELINK DOWNLINK i.e. Recieving mode ==============================================" << endl;
            //TODO enable sleep mode also for SIDELINK DOWNLINK ???
            //TODO Check Assumption that the UE knows the RB in UL
            (lteSchedulerUeUnassistedD2DSLRx_->resourceBlocks()) = (eNodeBFunctions->getDeployer())->getNumRbUl();

            lteSchedulerUeUnassistedD2DSLRx_->updateHarqDescs();

            LteMacScheduleList* scheduleListSLRx = lteSchedulerUeUnassistedD2DSLRx_->scheduleTxD2Ds();
            // send uplink grants to PHY layer
            sendGrants(scheduleListSLRx);
            EV << "============================================ END SIDELINK DOWNLINK i.e. Recieving mode ============================================" << endl;
        }

    // For each D2D communication, the status of the HARQRxBuffer must be known to the eNB
    // For each HARQ-RX Buffer corresponding to a D2D communication, store "mirror" buffer at the eNB
    HarqRxBuffers::iterator buffIt = harqRxBuffers_.begin();
    for (; buffIt != harqRxBuffers_.end(); ++buffIt)
    {
        MacNodeId senderId = buffIt->first;
        LteHarqBufferRx* buff = buffIt->second;

        // skip the H-ARQ buffer corresponding to DL transmissions
        if (senderId == cellId_)
            continue;

        // TODO skip the H-ARQ buffers corresponding to D2D_MULTI transmissions

        //The constructor "extracts" all the useful information from the harqRxBuffer and put them in a LteHarqBufferRxD2DMirror object
        //That object resides in enB. Because this operation is done after the enb main loop the enb is 1 TTI backward respect to the Receiver
        //This means that enb will check the buffer for retransmission 3 TTI before
        LteHarqBufferRxD2DMirror* mirbuff = new LteHarqBufferRxD2DMirror(buff, (unsigned char)this->par("maxHarqRtx"), senderId);
        enb_->storeRxHarqBufferMirror(nodeId_, mirbuff);
    }

    EV << NOW << "LteMacUeRealisticD2D::handleSelfMessage " << nodeId_ << " - HARQ process " << (unsigned int)currentHarq_ << endl;
    // updating current HARQ process for next TTI

    //unsigned char currentHarq = currentHarq_;

    // no grant available - if user has backlogged data, it will trigger scheduling request
    // no harq counter is updated since no transmission is sent.

    if (schedulingGrant_==NULL)
    {
        EV << NOW << " LteMacUeRealisticD2D::handleSelfMessage " << nodeId_ << " NO configured grant" << endl;

        // if necessary, a RAC request will be sent to obtain a grant
        if (par("unassistedD2D"))
            {
                if (!(macBuffers_.empty()))
                {
                    // Iterate through the buffer and check RAC for all the destination UEs
                    for(LteMacBufferMap::const_iterator iter = macBuffers_.begin(); iter != macBuffers_.end(); ++iter)
                    {
                        LteMacBuffer* vTxQueue= iter->second;
                        MacCid ueRxD2DId = iter->first;
                        EV << "LteMacUeRealisticD2D::handleSelfMessage: checking RAC for the destination Rx D2D UE node: " << MacCidToNodeId(ueRxD2DId)<< endl;
                        checkRAC(ueRxD2DId);
                    }
                }
            }
        else
            {
                checkRAC();
            }
        // TODO ensure all operations done  before return ( i.e. move H-ARQ rx purge before this point)
    }
    else if (schedulingGrant_->getPeriodic())
    {
        // Periodic checks
        if(--expirationCounter_ < 0)
        {
            // Periodic grant is expired
            delete schedulingGrant_;
            schedulingGrant_ = NULL;
            // if necessary, a RAC request will be sent to obtain a grant
            if (par("unassistedD2D"))
                {
                // Iterate through the buffer and check RAC for all the destination UEs
                for(LteMacBufferMap::iterator iter = macBuffers_.begin(); iter != macBuffers_.end(); ++iter)
                    {
                        LteMacBuffer* vTxQueue= iter->second;
                        MacCid ueRxD2DId = iter->first;
                        EV << "LteMacUeRealisticD2D::handleSelfMessage: checking RAC for the destination rxD2DUE node: " <<  MacCidToNodeId(ueRxD2DId)<< endl;
                        checkRAC(ueRxD2DId);
                    }
                }
            else
                {
                    checkRAC();
                }
        }
        else if (--periodCounter_>0)
        {
            return;
        }
        else
        {
            // resetting grant period
            periodCounter_=schedulingGrant_->getPeriod();
            // this is periodic grant TTI - continue with frame sending
        }
    }

    bool requestSdu = false;
    if (schedulingGrant_!=NULL) // if a grant is configured
    {
        if(!firstTx)
        {
            EV << "\t currentHarq_ counter initialized " << endl;
            firstTx=true;
            // the eNb will receive the first pdu in 2 TTI, thus initializing acid to 0
//            currentHarq_ = harqRxBuffers_.begin()->second->getProcesses() - 2;
            currentHarq_ = UE_TX_HARQ_PROCESSES - 2;
        }
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
        //        if ( LteDebug:r:trace("LteMacUeRealisticD2D::newSubFrame") )
        //            fprintf (stderr,"%.9f UE: [%d] Triggering retransmission for acid %d\n",NOW,nodeId_,currentHarq);
        //        // triggering retransmission --- nothing to do here, really!
//        } else {
        // buffer drop should occour here.
//        scheduleList = ueScheduler_->buildSchedList();

        EV << NOW << " LteMacUeRealisticD2D::handleSelfMessage " << nodeId_ << " entered scheduling" << endl;

        bool retx = false;

        HarqTxBuffers::iterator it2;
        LteHarqBufferTx * currHarq;
        for(it2 = harqTxBuffers_.begin(); it2 != harqTxBuffers_.end(); it2++)
        {
            EV << "\t Looking for retx in acid " << (unsigned int)currentHarq_ << endl;
            currHarq = it2->second;

            // check if the current process has unit ready for retx
            bool ready = currHarq->getProcess(currentHarq_)->hasReadyUnits();
            CwList cwListRetx = currHarq->getProcess(currentHarq_)->readyUnitsIds();

            EV << "\t [process=" << (unsigned int)currentHarq_ << "] , [retx=" << ((retx)?"true":"false")
               << "] , [n=" << cwListRetx.size() << "]" << endl;

            // if a retransmission is needed
            if(ready)
            {
                UnitList signal;
                signal.first=currentHarq_;
                signal.second = cwListRetx;
                currHarq->markSelected(signal,schedulingGrant_->getUserTxParams()->getLayers().size());
                retx = true;
            }
        }
        // if no retx is needed, proceed with normal scheduling
        if(!retx)
        {
            if (par("unassistedD2D")) {
                EV << "============================================ END SIDELINK UPLINK i.e. ReTransmitting mode ============================================" << endl;
                // SL-UL-TX scheduling done by Tx D2D UE
                scheduleList_ = lteSchedulerUeUnassistedD2DSLTx_->scheduleData();// Data packets
            } else {
                // SL-UL-TX scheduling done by Tx D2D UE
                scheduleList_ = lcgScheduler_->schedule();
            }
            if ((bsrTriggered_ || bsrD2DMulticastTriggered_) && scheduleList_->empty())
            {
                // no connection scheduled, but we can use this grant to send a BSR to the Rx D2D Ue
                macPduMake();
            }
            else
            {
                requestSdu = macSduRequest(); // return a bool
            }

        }

        // Message that triggers flushing of Tx H-ARQ buffers for all users
        // This way, flushing is performed after the (possible) reception of new MAC PDUs
        cMessage* flushHarqMsg = new cMessage("flushHarqMsg");
        flushHarqMsg->setSchedulingPriority(1);        // after other messages
        scheduleAt(NOW, flushHarqMsg);
    }

    //============================ DEBUG ==========================
    HarqTxBuffers::iterator it;

    EV << "\n htxbuf.size " << harqTxBuffers_.size() << endl;

    int cntOuter = 0;
    int cntInner = 0;
    for(it = harqTxBuffers_.begin(); it != harqTxBuffers_.end(); it++)
    {
        LteHarqBufferTx* currHarq = it->second;
        BufferStatus harqStatus = currHarq->getBufferStatus();
        BufferStatus::iterator jt = harqStatus.begin(), jet= harqStatus.end();

        EV << "\t cicloOuter " << cntOuter << " - bufferStatus.size=" << harqStatus.size() << endl;
        for(; jt != jet; ++jt)
        {
            EV << "\t\t cicloInner " << cntInner << " - jt->size=" << jt->size()
               << " - statusCw(0/1)=" << jt->at(0).second << "/" << jt->at(1).second << endl;
        }
    }
    //======================== END DEBUG ==========================

    unsigned int purged =0;
    // purge from corrupted PDUs all Rx H-HARQ buffers
    for (hit= harqRxBuffers_.begin(); hit != het; ++hit)
    {
        // purge corrupted PDUs only if this buffer is for a DL transmission. Otherwise, if you
        // purge PDUs for D2D communication, also "mirror" buffers will be purged
        if (hit->first == cellId_)
            purged += hit->second->purgeCorruptedPdus();
    }
    EV << NOW << " LteMacUeRealisticD2D::handleSelfMessage Purged " << purged << " PDUS" << endl;

    if (requestSdu == false)
    {
        // update current harq process id
        currentHarq_ = (currentHarq_+1) % harqProcesses_;
    }

    EV << "--- END UE MAIN LOOP ---" << endl;
}


UserTxParams* LteMacUeRealisticD2D::getPreconfiguredTxParams()
{
    UserTxParams* txParams = new UserTxParams();

    // default parameters for D2D
    txParams->isSet() = true;
    txParams->writeTxMode(TRANSMIT_DIVERSITY);
    Rank ri = 1;                                              // rank for TxD is one
    txParams->writeRank(ri);
    txParams->writePmi(intuniform(1, pow(ri, (double) 2)));   // taken from LteFeedbackComputationRealistic::computeFeedback

    Cqi cqi = par("d2dCqi");
    if (cqi < 0 || cqi > 15)
        throw cRuntimeError("LteMacUeRealisticD2D::getPreconfiguredTxParams - CQI %s is not a valid value. Aborting", cqi);
    txParams->writeCqi(std::vector<Cqi>(1,cqi));

    BandSet b;
    for (Band i = 0; i < getDeployer(nodeId_)->getNumBands(); ++i) b.insert(i);

    RemoteSet antennas;
    antennas.insert(MACRO);
    txParams->writeAntennas(antennas);

    return txParams;
}

void LteMacUeRealisticD2D::macHandleD2DModeSwitch(cPacket* pkt)
{
    EV << NOW << " LteMacUeRealisticD2D::macHandleD2DModeSwitch - Start" << endl;

    // all data in the MAC buffers of the connection to be switched are deleted

    D2DModeSwitchNotification* switchPkt = check_and_cast<D2DModeSwitchNotification*>(pkt);
    bool txSide = switchPkt->getTxSide();
    MacNodeId peerId = switchPkt->getPeerId();
    LteD2DMode newMode = switchPkt->getNewMode();
    LteD2DMode oldMode = switchPkt->getOldMode();
    UserControlInfo* uInfo = check_and_cast<UserControlInfo*>(pkt->removeControlInfo());

    if (txSide)
    {
        Direction newDirection = (newMode == DM) ? D2D : UL;
        Direction oldDirection = (oldMode == DM) ? D2D : UL;

        // find the correct connection involved in the mode switch
        MacCid cid;
        FlowControlInfo* lteInfo = NULL;
        std::map<MacCid, FlowControlInfo>::iterator it = connDesc_.begin();
        for (; it != connDesc_.end(); ++it)
        {
            cid = it->first;
            lteInfo = check_and_cast<FlowControlInfo*>(&(it->second));

            if (lteInfo->getD2dRxPeerId() == peerId && (Direction)lteInfo->getDirection() == oldDirection)
            {
                EV << NOW << " LteMacUeRealisticD2D::macHandleD2DModeSwitch - found old connection with cid " << cid << ", erasing buffered data" << endl;
                if (oldDirection != newDirection)
                {
                    // empty virtual buffer for the selected cid
                    LteMacBufferMap::iterator macBuff_it = macBuffers_.find(cid);
                    if (macBuff_it != macBuffers_.end())
                    {
                        while (!(macBuff_it->second->isEmpty()))
                            macBuff_it->second->popFront();
                        delete macBuff_it->second;
                        macBuffers_.erase(macBuff_it);
                    }

                    // empty real buffer for the selected cid (they should be already empty)
                    LteMacBuffers::iterator mBuf_it = mbuf_.find(cid);
                    if (mBuf_it != mbuf_.end())
                    {
                        while (mBuf_it->second->getQueueLength() > 0)
                        {
                            cPacket* pdu = mBuf_it->second->popFront();
                            delete pdu;
                        }
                        delete mBuf_it->second;
                        mbuf_.erase(mBuf_it);
                    }

                    // interrupt H-ARQ processes for SL
                    unsigned int id = peerId;
                    HarqTxBuffers::iterator hit = harqTxBuffers_.find(id);
                    if (hit != harqTxBuffers_.end())
                    {
                        for (int proc = 0; proc < (unsigned int) UE_TX_HARQ_PROCESSES; proc++)
                        {
                            hit->second->forceDropProcess(proc);
                        }
                    }

                    // interrupt H-ARQ processes for UL
                    id = getMacCellId();
                    hit = harqTxBuffers_.find(id);
                    if (hit != harqTxBuffers_.end())
                    {
                        for (int proc = 0; proc < (unsigned int) UE_TX_HARQ_PROCESSES; proc++)
                        {
                            hit->second->forceDropProcess(proc);
                        }
                    }
                }

                // abort BSR requests
                bsrTriggered_ = false;

                D2DModeSwitchNotification* switchPkt_dup = switchPkt->dup();
                switchPkt_dup->setControlInfo(lteInfo->dup());
                switchPkt_dup->setOldConnection(true);
                sendUpperPackets(switchPkt_dup);

                if (oldDirection != newDirection)
                {
                    // remove entry from lcgMap
                    LcgMap::iterator lt = lcgMap_.begin();
                    for (; lt != lcgMap_.end(); )
                    {
                        if (lt->second.first == cid)
                        {
                            lcgMap_.erase(lt++);
                        }
                        else
                        {
                            ++lt;
                        }
                    }
                }
                EV << NOW << " LteMacUeRealisticD2D::macHandleD2DModeSwitch - send switch signal to the RLC TX entity corresponding to the old mode, cid " << cid << endl;
            }
            else if (lteInfo->getD2dRxPeerId() == peerId && (Direction)lteInfo->getDirection() == newDirection)
            {
                EV << NOW << " LteMacUeRealisticD2D::macHandleD2DModeSwitch - send switch signal to the RLC TX entity corresponding to the new mode, cid " << cid << endl;
                if (oldDirection != newDirection)
                {
                    D2DModeSwitchNotification* switchPkt_dup = switchPkt->dup();
                    switchPkt_dup->setOldConnection(false);
                    switchPkt_dup->setControlInfo(lteInfo->dup());
                    sendUpperPackets(switchPkt_dup);
                }
            }
        }
    }
    else   // rx side
    {
        Direction newDirection = (newMode == DM) ? D2D : DL;
        Direction oldDirection = (oldMode == DM) ? D2D : DL;

        // find the correct connection involved in the mode switch
        MacCid cid;
        FlowControlInfo* lteInfo = NULL;
        std::map<MacCid, FlowControlInfo>::iterator it = connDescIn_.begin();
        for (; it != connDescIn_.end(); ++it)
        {
            cid = it->first;
            lteInfo = check_and_cast<FlowControlInfo*>(&(it->second));
            if (lteInfo->getD2dTxPeerId() == peerId && (Direction)lteInfo->getDirection() == oldDirection)
            {
                EV << NOW << " LteMacUeRealisticD2D::macHandleD2DModeSwitch - found old connection with cid " << cid << ", send signal to the RLC RX entity" << endl;
                if (oldDirection != newDirection)
                {
                    // interrupt H-ARQ processes for SL
                    unsigned int id = peerId;
                    HarqRxBuffers::iterator hit = harqRxBuffers_.find(id);
                    if (hit != harqRxBuffers_.end())
                    {
                        for (unsigned int proc = 0; proc < (unsigned int) UE_RX_HARQ_PROCESSES; proc++)
                        {
                            unsigned int numUnits = hit->second->getProcess(proc)->getNumHarqUnits();
                            for (unsigned int i=0; i < numUnits; i++)
                            {

                                hit->second->getProcess(proc)->purgeCorruptedPdu(i); // delete contained PDU
                                hit->second->getProcess(proc)->resetCodeword(i);     // reset unit
                            }
                        }
                    }
                    // Has to be handled for unassisted D2D
                    enb_->deleteRxHarqBufferMirror(nodeId_);

                    // notify that this UE is switching during this TTI
                    resetHarq_[peerId] = NOW;

                    D2DModeSwitchNotification* switchPkt_dup = switchPkt->dup();
                    switchPkt_dup->setControlInfo(lteInfo->dup());
                    switchPkt_dup->setOldConnection(true);
                    sendUpperPackets(switchPkt_dup);
                }
            }
            else if (lteInfo->getD2dTxPeerId() == peerId && (Direction)lteInfo->getDirection() == newDirection)
            {
                EV << NOW << " LteMacUeRealisticD2D::macHandleD2DModeSwitch - found new connection with cid " << cid << ", send signal to the RLC RX entity" << endl;
                if (oldDirection != newDirection)
                {
                    D2DModeSwitchNotification* switchPkt_dup = switchPkt->dup();
                    switchPkt_dup->setOldConnection(false);
                    switchPkt_dup->setControlInfo(lteInfo->dup());
                    sendUpperPackets(switchPkt_dup);
                }
            }
        }
    }
    delete uInfo;
    delete pkt;
}


void LteMacUeRealisticD2D::storeRxHarqBufferMirror(MacNodeId id, LteHarqBufferRxD2DMirror* mirbuff)
{
    // TODO optimize

    if ( harqRxBuffersD2DMirror_.find(id) != harqRxBuffersD2DMirror_.end() )
        delete harqRxBuffersD2DMirror_[id];
    harqRxBuffersD2DMirror_[id] = mirbuff;
}

HarqRxBuffersMirror* LteMacUeRealisticD2D::getRxHarqBufferMirror()
{
    return &harqRxBuffersD2DMirror_;
}

void LteMacUeRealisticD2D::deleteRxHarqBufferMirror(MacNodeId id)
{
    HarqRxBuffersMirror::iterator it = harqRxBuffersD2DMirror_.begin() , et=harqRxBuffersD2DMirror_.end();
    for(; it != et;)
    {
        // get current nodeIDs
        MacNodeId senderId = it->second->peerId_; // Transmitter
        MacNodeId destId = it->first;             // Receiver

        if (senderId == id || destId == id)
        {
            harqRxBuffersD2DMirror_.erase(it++);
        }
        else
        {
            ++it;
        }
    }
}


void LteMacUeRealisticD2D::sendGrants(LteMacScheduleList* scheduleList)
{
    EV << NOW << "LteMacUeRealisticD2D::sendGrants " << endl;
    // Get device lists.
    std::vector<EnbInfo*>* enbInfo = getBinder()->getEnbList();
    std::vector<UeInfo*>* ueInfo = getBinder()->getUeList();
    if (enbInfo->size() == 0)
        throw cRuntimeError("LteMacUeRealisticD2D::handleSelfMessage can't get AMC pointer because I couldn't find an eNodeB!");
    // -> its ID -> the node -> cast to the eNodeB class
    MacNodeId mENodeBId = enbInfo->at(0)->id;
    ExposedLteMacEnb *eNodeBFunctions = (ExposedLteMacEnb*) getMacByMacNodeId(mENodeBId);

    while (!scheduleList->empty())
    {
        LteMacScheduleList::iterator it, ot;
        it = scheduleList->begin();

        Codeword cw = it->first.second;
        Codeword otherCw = MAX_CODEWORDS - cw;

//        MacNodeId nodeId = it->first.first;
        MacCid cid = it->first.first;
        LogicalCid lcid = MacCidToLcid(cid);
        MacNodeId nodeId = MacCidToNodeId(cid);
        unsigned int granted = it->second;
        unsigned int codewords = 0;

        // removing visited element from scheduleList.
        scheduleList->erase(it);

        if (granted > 0)
        {
            // increment number of allocated Cw
            ++codewords;
        }
        else
        {
            // active cw becomes the "other one"
            cw = otherCw;
        }

        std::pair<unsigned int, Codeword> otherPair(nodeId, otherCw);

        if ((ot = (scheduleList->find(otherPair))) != (scheduleList->end()))
        {
            // increment number of allocated Cw
            ++codewords;

            // removing visited element from scheduleList.
            scheduleList->erase(ot);
        }

        if (granted == 0)
            continue; // avoiding transmission of 0 grant (0 grant should not be created)

        EV << NOW << " LteMacUeRealisticD2D::sendGrants Rx D2D UE Node[" << getMacNodeId() << "] - "
           << granted << " blocks to grant for Tx D2D UE user " << nodeId << " on "
           << codewords << " codewords. CW[" << cw << "\\" << otherCw << "]" << endl;

        // get the direction of the grant, depending on which connection has been scheduled by the eNB
        Direction dir = (lcid == D2D_MULTI_SHORT_BSR) ? D2D_MULTI : ((lcid == D2D_SHORT_BSR) ? D2D : UL);

        // TODO Grant is set aperiodic as default
        LteSchedulingGrant* grant = new LteSchedulingGrant("LteGrant");
        grant->setDirection(dir);
        grant->setCodewords(codewords);

        // set total granted blocks
        grant->setTotalGrantedBlocks(granted);

        UserControlInfo* uinfo = new UserControlInfo();
        uinfo->setSourceId(getMacNodeId());
        // Should set the ID of the Tx D2D UE and not eNB
        uinfo->setDestId(nodeId);
        uinfo->setFrameType(GRANTPKT);

        grant->setControlInfo(uinfo);

        const UserTxParams& ui = getAmc()->computeTxParams(nodeId, dir);
        UserTxParams* txPara = new UserTxParams(ui);
        // FIXME: possible memory leak
        grant->setUserTxParams(txPara);

        // acquiring remote antennas set from user info
        const std::set<Remote>& antennas = ui.readAntennaSet();
        std::set<Remote>::const_iterator antenna_it = antennas.begin(),
        antenna_et = antennas.end();
        const unsigned int logicalBands = (eNodeBFunctions->getDeployer())->getNumBands();
        //  HANDLE MULTICW
        for (; cw < codewords; ++cw)
        {
            unsigned int grantedBytes = 0;

            for (Band b = 0; b < logicalBands; ++b)
            {
                unsigned int bandAllocatedBlocks = 0;
               // for (; antenna_it != antenna_et; ++antenna_it) // OLD FOR
               for (antenna_it = antennas.begin(); antenna_it != antenna_et; ++antenna_it)
               {
                    bandAllocatedBlocks += lteSchedulerUeUnassistedD2DSLRx_->readPerUeAllocatedBlocks(nodeId,*antenna_it, b);
               }
               grantedBytes += getAmc()->computeBytesOnNRbs(nodeId, b, cw, bandAllocatedBlocks, dir );
            }

            grant->setGrantedCwBytes(cw, grantedBytes);
            EV << NOW << " LteMacUeRealisticD2D::sendGrants - granting " << grantedBytes << " on cw " << cw << endl;
        }
        RbMap map;

        lteSchedulerUeUnassistedD2DSLRx_->readRbOccupation(nodeId, map);

        grant->setGrantedBlocks(map);
        // send grant to PHY layer
        sendLowerPackets(grant);
    }
}

double LteMacUeRealisticD2D::getZeroLevel(Direction dir, LteSubFrameType type)
{
    std::string a("zeroLevel");
    a.append(SubFrameTypeToA(type));
    a.append(dirToA(dir));
    return par(a.c_str());
}

double LteMacUeRealisticD2D::getPowerUnit(Direction dir, LteSubFrameType type)
{
    std::string a("powerUnit");
    a.append(SubFrameTypeToA(type));
    a.append(dirToA(dir));
    return par(a.c_str());
}

double LteMacUeRealisticD2D::getIdleLevel(Direction dir, LteSubFrameType type)
{
    std::string a("idleLevel");
    a.append(SubFrameTypeToA(type));
    a.append(dirToA(dir));
    return par(a.c_str());
}

void LteMacUeRealisticD2D::allocatedRB(unsigned int rb)
{
    lastTtiAllocatedRb_ = rb;
}

void LteMacUeRealisticD2D::bufferizeBsr(MacBsr* bsr, MacCid cid)
{
    PacketInfo vpkt(bsr->getSize(), bsr->getTimestamp());

    LteMacBufferMap::iterator it = bsrbuf_.find(cid);
    if (it == bsrbuf_.end())
    {
        // Queue not found for this cid: create
        LteMacBuffer* bsrqueue = new LteMacBuffer();

        bsrqueue->pushBack(vpkt);
        bsrbuf_[cid] = bsrqueue;

        EV << "LteBsrBuffers : Added new BSR buffer for node: "
           << MacCidToNodeId(cid) << " for Lcid: " << MacCidToLcid(cid)
           << " Current BSR size: " << bsr->getSize() << "\n";
    }
    else
    {
        // Found
        LteMacBuffer* bsrqueue = it->second;
        bsrqueue->pushBack(vpkt);

        EV << "LteBsrBuffers : Using old buffer for node: " << MacCidToNodeId(
            cid) << " for Lcid: " << MacCidToLcid(cid)
           << " Current BSR size: " << bsr->getSize() << "\n";
    }

        // signal backlog to Uplink scheduler
    lteSchedulerUeUnassistedD2DSLRx_->backlog(cid);
}

void LteMacUeRealisticD2D::macPduUnmake(cPacket* pkt)
{
    EV << "LteMacUeRealisticD2D: pduUnmaker Reached" << endl;
    LteMacPdu* macPkt = check_and_cast<LteMacPdu*>(pkt);
    while (macPkt->hasSdu())
    {
        // Extract and send SDU
        cPacket* upPkt = macPkt->popSdu();
        take(upPkt);

        /* TODO: upPkt->info() */
        EV << "LteMacUeRealisticD2D: pduUnmaker extracted SDU" << endl;

        // store descriptor for the incoming connection, if not already stored
        FlowControlInfo* lteInfo = check_and_cast<FlowControlInfo*>(upPkt->getControlInfo());
        MacNodeId senderId = lteInfo->getSourceId();
        LogicalCid lcid = lteInfo->getLcid();
        MacCid cid = idToMacCid(senderId, lcid);
        if (connDescIn_.find(cid) == connDescIn_.end())
        {
            FlowControlInfo toStore(*lteInfo);
            connDescIn_[cid] = toStore;
        }
        sendUpperPackets(upPkt);
    }
    while (macPkt->hasCe())
        {
            // Extract CE
            // TODO: vedere se   per cid o lcid
            MacBsr* bsr = check_and_cast<MacBsr*>(macPkt->popCe());
            UserControlInfo* lteInfo = check_and_cast<UserControlInfo*>(macPkt->getControlInfo());
            LogicalCid lcid = lteInfo->getLcid();  // one of SHORT_BSR or D2D_MULTI_SHORT_BSR

            MacCid cid = idToMacCid(lteInfo->getSourceId(), lcid); // this way, different connections from the same UE (e.g. one UL and one D2D)
                                                                   // obtain different CIDs. With the inverse operation, you can get
                                                                   // the LCID and discover if the connection is UL or D2D
            bufferizeBsr(bsr, cid);
            EV << "LteMacUeRealisticD2D: pduUnmaker bufferized BSR" << endl;
        }

    ASSERT(macPkt->getOwner() == this);
    delete macPkt;
}

//To handle RAC Requests by Rx D2D UE from Tx D2D UE
void LteMacUeRealisticD2D::macHandleRacRequest(cPacket* pkt)
{
    EV << NOW << " LteMacUeRealisticD2D::macHandleRacRequest" << endl;

    LteRac* racPkt = check_and_cast<LteRac*> (pkt);
    UserControlInfo* uinfo = check_and_cast<UserControlInfo*> (
        racPkt->getControlInfo());
    // To signal RAC request to the scheduler (called by Rx D2D Ue)
    EV << NOW << " LteMacUeRealisticD2D::macHandleRacRequest Signal Rac for UE::" << uinfo->getSourceId() <<endl;
    lteSchedulerUeUnassistedD2DSLRx_->signalRac(uinfo->getSourceId());


    // TODO all RACs are marked are successful
    racPkt->setSuccess(true);
    // Should set the ID of the D2D TX UE
    uinfo->setDestId(uinfo->getSourceId());
    uinfo->setSourceId(nodeId_);
    uinfo->setDirection(DL);

    sendLowerPackets(racPkt);
}


//if Tx D2D UE has backlogged data, it will trigger scheduling request to the Rx D2D UE
void LteMacUeRealisticD2D::checkRAC(MacCid ueRxD2DId)
{
    // Find the rx UE in the RACMAP
    // Create a new entry into the Map if the UE is not present
    if ((RacMap.find(MacCidToNodeId(ueRxD2DId)))  == RacMap.end())
    {
        RacVariables tempRacVariables;
        tempRacVariables.racBackoffTimer_ = 0;
        tempRacVariables.maxRacTryouts_ = 0;
        tempRacVariables.currentRacTry_ = 0;
        tempRacVariables.minRacBackoff_ = 0;
        tempRacVariables.maxRacBackoff_ = 1;

        tempRacVariables.raRespTimer_ = 0;
        tempRacVariables.raRespWinStart_ = 3;
        RacMap[MacCidToNodeId(ueRxD2DId)] = tempRacVariables;
    }
    EV << NOW << " LteMacUeRealisticD2D::checkRAC , Tx D2D Ue :: " << nodeId_ << " and Rx D2D Ue::" << MacCidToNodeId(ueRxD2DId) <<", racTimer : " << RacMap[MacCidToNodeId(ueRxD2DId)].racBackoffTimer_ << " maxRacTryOuts : " << RacMap[MacCidToNodeId(ueRxD2DId)].maxRacTryouts_
       << ", raRespTimer:" << RacMap[MacCidToNodeId(ueRxD2DId)].raRespTimer_ << endl;

    if (RacMap[MacCidToNodeId(ueRxD2DId)].racBackoffTimer_>0)
    {
        RacMap[MacCidToNodeId(ueRxD2DId)].racBackoffTimer_--;
        return;
    }

    if(RacMap[MacCidToNodeId(ueRxD2DId)].raRespTimer_>0)
    {
        // decrease RAC response timer
        RacMap[MacCidToNodeId(ueRxD2DId)].raRespTimer_--;
        EV << NOW << " LteMacUeRealisticD2D::checkRAC - waiting for previous RAC requests sent to Rx D2D Ue:: "<< MacCidToNodeId(ueRxD2DId) <<" to complete (timer=" << RacMap[MacCidToNodeId(ueRxD2DId)].raRespTimer_ << ")" << endl;
        return;
    }

    // Avoids double requests within same TTI window
    if (RacMap[MacCidToNodeId(ueRxD2DId)].racRequested_)
    {
        EV << NOW << " LteMacUeRealisticD2D::checkRAC - double RAC request" << endl;
        RacMap[MacCidToNodeId(ueRxD2DId)].racRequested_=false;
        return;
    }
    if (racD2DMulticastRequested_)
    {
        EV << NOW << " LteMacUeRealisticD2D::checkRAC - double RAC request" << endl;
        racD2DMulticastRequested_=false;
        return;
    }

    bool trigger=false;
    bool triggerD2DMulticast=false;

    LteMacBufferMap::const_iterator it;

    for (it = macBuffers_.begin(); it!=macBuffers_.end();++it)
    {
        if (!(it->second->isEmpty()))
        {
            MacCid cid = it->first;
            if (connDesc_.at(cid).getDirection() == D2D_MULTI)
                triggerD2DMulticast = true;
            else
                trigger = true;
            break;
        }
    }

    if (!trigger && !triggerD2DMulticast)
        EV << NOW << "Ue " << nodeId_ << ",RAC aborted, no data in queues " << endl;

    if (RacMap[MacCidToNodeId(ueRxD2DId)].racRequested_=trigger) // Unassisted mode send the packet to Rx D2D Ue
    {
        LteRac* racReq = new LteRac("RacRequest");
        UserControlInfo* uinfo = new UserControlInfo();
        uinfo->setSourceId(getMacNodeId());
        uinfo->setDestId(MacCidToNodeId(ueRxD2DId));
        uinfo->setDirection(UL);
        uinfo->setFrameType(RACPKT);
        racReq->setControlInfo(uinfo);

        sendLowerPackets(racReq);

        EV << NOW << " Ue  " << nodeId_ << " cell " << cellId_ << " ,RAC request sent to Rx D2D UE with ID:: "<< MacCidToNodeId(ueRxD2DId) <<" through PHY " << endl;

        // wait at least  "raRespWinStart_" TTIs before another RAC request
        RacMap[MacCidToNodeId(ueRxD2DId)].raRespTimer_ = RacMap[MacCidToNodeId(ueRxD2DId)].raRespWinStart_;
    }
}

SchedDiscipline LteMacUeRealisticD2D::getSchedDiscipline()
{
    return aToSchedDiscipline(par("schedulingDisciplineUl").stdstringValue());
}
