//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include <assert.h>
#include "stack/phy/layer/LtePhyUeD2D.h"
#include "stack/mac/layer/LteMacUeRealisticD2D.h"
#include "stack/phy/packet/LteFeedbackPkt.h"
#include "stack/d2dModeSelection/D2DModeSelectionBase.h"

Define_Module(LtePhyUeD2D);

LtePhyUeD2D::LtePhyUeD2D()
{
    handoverStarter_ = NULL;
    handoverTrigger_ = NULL;
}

LtePhyUeD2D::~LtePhyUeD2D()
{
}

void LtePhyUeD2D::initialize(int stage)
{
    LtePhyUe::initialize(stage);
    if (stage == 0)
    {
        d2dTxPower_ = par("d2dTxPower");
        d2dMulticastEnableCaptureEffect_ = par("d2dMulticastCaptureEffect");
        d2dDecodingTimer_ = NULL;
        enableD2DCqiReporting_ = par("enableD2DCqiReporting");
    }
    else if (stage == 1)
    {
        initializeFeedbackComputation(par("feedbackComputation").xmlValue());
    }
}

void LtePhyUeD2D::handleSelfMessage(cMessage *msg)
{
    if (msg->isName("d2dDecodingTimer"))
    {
        // select one frame from the buffer. Implements the capture effect
        LteAirFrame* frame = extractAirFrame();
        UserControlInfo* lteInfo = check_and_cast<UserControlInfo*>(frame->removeControlInfo());

        // decode the selected frame
        decodeAirFrame(frame, lteInfo);

        // clear buffer
        while (!d2dReceivedFrames_.empty())
        {
            LteAirFrame* frame = d2dReceivedFrames_.back();
            d2dReceivedFrames_.pop_back();
            delete frame;
        }

        delete msg;
        d2dDecodingTimer_ = NULL;
    }
    else
        LtePhyUe::handleSelfMessage(msg);
}

// TODO: ***reorganize*** method
void LtePhyUeD2D::handleAirFrame(cMessage* msg)
{
    UserControlInfo* lteInfo = check_and_cast<UserControlInfo*>(msg->removeControlInfo());

    if (useBattery_)
    {
        //TODO BatteryAccess::drawCurrent(rxAmount_, 0);
    }
    connectedNodeId_ = masterId_;
    LteAirFrame* frame = check_and_cast<LteAirFrame*>(msg);
    EV << "LtePhyUeD2D: received new LteAirFrame with ID " << frame->getId() << " from channel" << endl;
    //Update coordinates of this user
    if (lteInfo->getFrameType() == HANDOVERPKT)
    {
        EV<<"LtePhyUeD2D: received new LteAirFrame with frameType HANDOVERPKT"<<endl;
        // check if handover is already in process
        if (handoverTrigger_ != NULL && handoverTrigger_->isScheduled())
        {
            delete lteInfo;
            delete frame;
            return;
        }

        handoverHandler(frame, lteInfo);
        return;
    }

    // Check if the frame is for us ( MacNodeId matches or - if this is a multicast communication - enrolled in multicast group)
    if (lteInfo->getDestId() != nodeId_ && !(binder_->isInMulticastGroup(nodeId_, lteInfo->getMulticastGroupId())))
    {
        EV << "ERROR: Frame is not for us. Delete it." << endl;
        EV << "Packet Type: " << phyFrameTypeToA((LtePhyFrameType)lteInfo->getFrameType()) << endl;
        EV << "Frame MacNodeId: " << lteInfo->getDestId() << endl;
        EV << "Local MacNodeId: " << nodeId_ << endl;
        delete lteInfo;
        delete frame;
        return;
    }


    if (binder_->isInMulticastGroup(nodeId_,lteInfo->getMulticastGroupId()))
    {
        EV<<"LtePhyUeD2D: received new LteAirFrame with frameType MULTICAST"<<endl;
        // HACK: if this is a multicast connection, change the destId of the airframe so that upper layers can handle it
        lteInfo->setDestId(nodeId_);
    }

    if (par("unassistedD2D"))
       {
               EV << "Received control pkt " << endl;
               MacNodeId senderMacNodeId = lteInfo->getSourceId();
               try
               {
                   binder_->getOmnetId(senderMacNodeId);
               }
               catch (std::out_of_range& e)
               {
                   std::cerr << "Sender (" << senderMacNodeId << ") does not exist anymore!" << std::endl;
                   delete frame;
                   return;    // FIXME ? make sure that nodes that left the simulation do not send
               }
               //handle feedback pkt
               if (lteInfo->getFrameType() == FEEDBACKPKT)
               {
                   handleFeedbackPkt(lteInfo, frame); // Calculate the CQI Feedback
                   delete frame;
                   return;
               }
               // In Transmitting mode recording UL CQI
               if ((lteInfo->getUserTxParams()) != NULL) // Emit the CQI Feedback for UL for the Tx D2D
               {
                   int cw = lteInfo->getCw();
                   if (lteInfo->getUserTxParams()->readCqiVector().size() == 1)
                       cw = 0;
                   double cqi = lteInfo->getUserTxParams()->readCqiVector()[cw];
                   tSample_->sample_ = cqi;
                   tSample_->id_ = nodeId_;
                   tSample_->module_ = getMacByMacNodeId(nodeId_);
                   if (lteInfo->getDirection() == UL || lteInfo->getDirection() == D2D)
                   {
                       emit(averageCqiUl_, tSample_);
                       emit(averageCqiUlvect_,cqi);
                   }
               }
       }

    // send H-ARQ feedback up
    if (lteInfo->getFrameType() == HARQPKT || lteInfo->getFrameType() == GRANTPKT || lteInfo->getFrameType() == RACPKT || lteInfo->getFrameType() == D2DMODESWITCHPKT)
    {
        EV << "LtePhyUeD2D: received new LteAirFrame with frameType HARQ/Grant/RAC/D2DModeSwitch:: " << lteInfo->getFrameType() << " from channel" << endl;
        EV << "Received control pkt " << endl;
        handleControlMsg(frame, lteInfo);
        return;
    }

    // this is a DATA packet

    // if the packet is a D2D multicast one, store it and decode it at the end of the TTI
    if (d2dMulticastEnableCaptureEffect_ && binder_->isInMulticastGroup(nodeId_,lteInfo->getMulticastGroupId()))
    {
        EV<<"LtePhyUeD2D: received new LteAirFrame with frameType DATA PKT"<<endl;
        // if not already started, auto-send a message to signal the presence of data to be decoded
        if (d2dDecodingTimer_ == NULL)
        {
            d2dDecodingTimer_ = new cMessage("d2dDecodingTimer");
            d2dDecodingTimer_->setSchedulingPriority(10);          // last thing to be performed in this TTI
            scheduleAt(NOW, d2dDecodingTimer_);
        }

        // store frame, together with related control info
        frame->setControlInfo(lteInfo);
        storeAirFrame(frame);            // implements the capture effect

        return;                          // exit the function, decoding will be done later
    }

    if ((lteInfo->getUserTxParams()) != NULL)
    {
        int cw = lteInfo->getCw();
        if (lteInfo->getUserTxParams()->readCqiVector().size() == 1)
            cw = 0;
        double cqi = lteInfo->getUserTxParams()->readCqiVector()[cw];
        tSample_->sample_ = cqi;
        tSample_->id_ = nodeId_;
        tSample_->module_ = getMacByMacNodeId(nodeId_);

        if (lteInfo->getDirection() == DL)
        {
            emit(averageCqiDl_, tSample_);
            emit(averageCqiDlvect_,cqi);
        }
        else
        {
            emit(averageCqiD2D_, tSample_);
            emit(averageCqiD2Dvect_,cqi);
        }
    }
    // apply decider to received packet
    bool result = true;
    RemoteSet r = lteInfo->getUserTxParams()->readAntennaSet();
    if (r.size() > 1)
    {
        // DAS
        for (RemoteSet::iterator it = r.begin(); it != r.end(); it++)
        {
            EV << "LtePhyUeD2D: Receiving Packet from antenna " << (*it) << "\n";

            /*
             * On UE set the sender position
             * and tx power to the sender das antenna
             */

//            cc->updateHostPosition(myHostRef,das_->getAntennaCoord(*it));
            // Set position of sender
//            Move m;
//            m.setStart(das_->getAntennaCoord(*it));
            RemoteUnitPhyData data;
            data.txPower=lteInfo->getTxPower();
            data.m=getRadioPosition();
            frame->addRemoteUnitPhyDataVector(data);
        }
        // apply analog models For DAS
        result=channelModel_->errorDas(frame,lteInfo);
    }
    else
    {
        //RELAY and NORMAL
        result = channelModel_->error(frame,lteInfo);
    }

            // update statistics
    if (result)
        numAirFrameReceived_++;
    else
        numAirFrameNotReceived_++;

    EV << "Handled LteAirframe with ID " << frame->getId() << " with result "
       << ( result ? "RECEIVED" : "NOT RECEIVED" ) << endl;

    cPacket* pkt = frame->decapsulate();

    // here frame has to be destroyed since it is no more useful
    delete frame;

    // attach the decider result to the packet as control info
    lteInfo->setDeciderResult(result);
    pkt->setControlInfo(lteInfo);

    // send decapsulated message along with result control info to upperGateOut_
    send(pkt, upperGateOut_);

    if (getEnvir()->isGUI())
        updateDisplayString();
}

void LtePhyUeD2D::handleFeedbackPkt(UserControlInfo* lteinfo,
    LteAirFrame *frame)
{
    EV << "Handled Feedback Packet with ID " << frame->getId() << endl;
    LteFeedbackPkt* pkt = check_and_cast<LteFeedbackPkt*>(frame->decapsulate());
    // here frame has to be destroyed since it is no more useful
    pkt->setControlInfo(lteinfo);
    // if feedback was generated by dummy phy we can send up to mac else nodeb should generate the "real" feddback
    if (lteinfo->feedbackReq.request)
    {
        requestFeedback(lteinfo, frame, pkt);
        //DEBUG
        LteFeedbackDoubleVector::iterator it;
        LteFeedbackVector::iterator jt;
        LteFeedbackDoubleVector vec = pkt->getLteFeedbackDoubleVectorDl();
        for (it = vec.begin(); it != vec.end(); ++it)
        {
            for (jt = it->begin(); jt != it->end(); ++jt)
            {
                MacNodeId id = lteinfo->getSourceId();
                EV << endl << "Node:" << id << endl;
                TxMode t = jt->getTxMode();
                EV << "TXMODE: " << txModeToA(t) << endl;
                if (jt->hasBandCqi())
                {
                    std::vector<CqiVector> vec = jt->getBandCqi();
                    std::vector<CqiVector>::iterator kt;
                    CqiVector::iterator ht;
                    int i;
                    for (kt = vec.begin(); kt != vec.end(); ++kt)
                    {
                        for (i = 0, ht = kt->begin(); ht != kt->end();
                            ++ht, i++)
                        EV << "Banda " << i << " Cqi " << *ht << endl;
                    }
                }
                else if (jt->hasWbCqi())
                {
                    CqiVector v = jt->getWbCqi();
                    CqiVector::iterator ht = v.begin();
                    for (; ht != v.end(); ++ht)
                    EV << "wb cqi " << *ht << endl;
                }
                if (jt->hasRankIndicator())
                {
                    EV << "Rank " << jt->getRankIndicator() << endl;
                }
            }
        }
    }
    // send decapsulated message along with result control info to upperGateOut_
    send(pkt, upperGateOut_);
}


void LtePhyUeD2D::requestFeedback(UserControlInfo* lteinfo, LteAirFrame* frame,
    LteFeedbackPkt* pkt)
{
    EV << NOW << " LtePhyUeD2D::requestFeedback " << endl;
    //get UE Position
    Coord sendersPos = lteinfo->getCoord();
    deployer_->setUePosition(lteinfo->getSourceId(), sendersPos);

    //Apply analog model (pathloss)
    //Get snr for UL direction
    std::vector<double> snr = channelModel_->getSINR(frame, lteinfo);
    FeedbackRequest req = lteinfo->feedbackReq;
    //Feedback computation
    fb_.clear();
    //get number of RU
    int nRus = deployer_->getNumRus();
    TxMode txmode = req.txMode;
    FeedbackType type = req.type;
    RbAllocationType rbtype = req.rbAllocationType;
    std::map<Remote, int> antennaCws = deployer_->getAntennaCws();
    unsigned int numPreferredBand = deployer_->getNumPreferredBands();
    for (Direction dir = UL; dir != UNKNOWN_DIRECTION;
        dir = ((dir == UL )? DL : UNKNOWN_DIRECTION))
    {
        //for each RU is called the computation feedback function
        if (req.genType == IDEAL)
        {
            fb_ = lteFeedbackComputation_->computeFeedback(type, rbtype, txmode,
                antennaCws, numPreferredBand, IDEAL, nRus, snr,
                lteinfo->getSourceId());
        }
        else if (req.genType == REAL)
        {
            RemoteSet::iterator it;
            fb_.resize(das_->getReportingSet().size());
            for (it = das_->getReportingSet().begin();
                it != das_->getReportingSet().end(); ++it)
            {
                fb_[(*it)].resize((int) txmode);
                fb_[(*it)][(int) txmode] =
                lteFeedbackComputation_->computeFeedback(*it, txmode,
                    type, rbtype, antennaCws[*it], numPreferredBand,
                    REAL, nRus, snr, lteinfo->getSourceId());
            }
        }
        // the reports are computed only for the antenna in the reporting set
        else if (req.genType == DAS_AWARE)
        {
            RemoteSet::iterator it;
            fb_.resize(das_->getReportingSet().size());
            for (it = das_->getReportingSet().begin();
                it != das_->getReportingSet().end(); ++it)
            {
                fb_[(*it)] = lteFeedbackComputation_->computeFeedback(*it, type,
                    rbtype, txmode, antennaCws[*it], numPreferredBand,
                    DAS_AWARE, nRus, snr, lteinfo->getSourceId());
            }
        }
        if (dir == UL)
        {
            pkt->setLteFeedbackDoubleVectorUl(fb_);


            if ((enableD2DCqiReporting_)&& (par("unassistedD2D")))
            {
                // compute D2D feedback for all possible peering UEs
                std::vector<UeInfo*>* ueList = binder_->getUeList();
                std::vector<UeInfo*>::iterator it = ueList->begin();
                for (; it != ueList->end(); ++it)
                {
                    MacNodeId peerId = (*it)->id;
//                    if (peerId != lteinfo->getSourceId() && binder_->checkD2DCapability(lteinfo->getSourceId(), peerId) && binder_->getNextHop(peerId) == nodeId_)
                    if (peerId != lteinfo->getSourceId() && binder_->checkD2DCapability(lteinfo->getSourceId(), peerId))
                    {
                        // the source UE might communicate with this peer using D2D, so compute feedback
                        // retrieve the position of the peer
                        Coord peerCoord = (*it)->phy->getCoord();
                        // get SINR for this link
                        snr = channelModel_->getSINR_D2D(frame, lteinfo, peerId, peerCoord, nodeId_);
                        // compute the feedback for this link
                        fb_ = lteFeedbackComputation_->computeFeedback(type, rbtype, txmode,
                                antennaCws, numPreferredBand, IDEAL, nRus, snr,
                                lteinfo->getSourceId());
                        pkt->setLteFeedbackDoubleVectorD2D(peerId, fb_);
                     }
                 }
//                //Prepare  parameters for next loop iteration - in order to compute SNR in DL
//                lteinfo->setTxPower(txPower_);
//                lteinfo->setDirection(DL);
//                dir = DL;
//                //Get snr for DL direction
//                snr = channelModel_->getSINR(frame, lteinfo);
            }

        }
        else if ((dir == DL) || (dir == D2D))
        {
            pkt->setLteFeedbackDoubleVectorDl(fb_);

            if ((enableD2DCqiReporting_)&& (par("unassistedD2D")))
            {
                // compute D2D feedback for all possible peering UEs
                std::vector<UeInfo*>* ueList = binder_->getUeList();
                std::vector<UeInfo*>::iterator it = ueList->begin();
                for (; it != ueList->end(); ++it)
                {
                    MacNodeId peerId = (*it)->id;
                    if (peerId != lteinfo->getSourceId() && binder_->checkD2DCapability(lteinfo->getSourceId(), peerId) && binder_->getNextHop(peerId) == nodeId_)
                    {
                         // the source UE might communicate with this peer using D2D, so compute feedback

                         // retrieve the position of the peer
                         Coord peerCoord = (*it)->phy->getCoord();

                         // get SINR for this link
                         snr = channelModel_->getSINR_D2D(frame, lteinfo, peerId, peerCoord, nodeId_);

                         // compute the feedback for this link
                         fb_ = lteFeedbackComputation_->computeFeedback(type, rbtype, txmode,
                                 antennaCws, numPreferredBand, IDEAL, nRus, snr,
                                 lteinfo->getSourceId());

                         pkt->setLteFeedbackDoubleVectorD2D(peerId, fb_);
                    }
                }
            }
//            else
//            {
//                dir = UL;
//                lteinfo->setDirection(UL);
//                // Get snr for DL direction
//                snr = channelModel_->getSINR(frame, lteinfo);
//
//            }
            dir = UNKNOWN_DIRECTION;
        }
    }
    EV << "LtePhyUeD2D::requestFeedback : Pisa Feedback Generated for nodeId: "
       << nodeId_ << " with generator type "
       << fbGeneratorTypeToA(req.genType) << " Fb size: " << fb_.size() << endl;
}
void LtePhyUeD2D::triggerHandover()
{
    // stop active D2D flows (go back to Infrastructure mode)

    // trigger D2D mode switch
    D2DModeSelectionBase *d2dModeSelection = check_and_cast<D2DModeSelectionBase*>(getSimulation()->getModule(binder_->getOmnetId(masterId_))->getSubmodule("nic")->getSubmodule("d2dModeSelection"));
    d2dModeSelection->doModeSwitchAtHandover(nodeId_);

    LtePhyUe::triggerHandover();
}

void LtePhyUeD2D::doHandover()
{
    // amc calls
    LteAmc *oldAmc = getAmcModule(masterId_);
    LteAmc *newAmc = getAmcModule(candidateMasterId_);
    assert(newAmc != NULL);
    oldAmc->detachUser(nodeId_, D2D);
    newAmc->attachUser(nodeId_, D2D);

    LtePhyUe::doHandover();
}

void LtePhyUeD2D::handleUpperMessage(cMessage* msg)
{
//    if (useBattery_) {
//    TODO     BatteryAccess::drawCurrent(txAmount_, 1);
//    }

    UserControlInfo* lteInfo = check_and_cast<UserControlInfo*>(msg->removeControlInfo());

    // Store the RBs used for transmission. For interference computation
    RbMap rbMap = lteInfo->getGrantedBlocks();
    UsedRBs info;
    info.time_ = NOW;
    info.rbMap_ = rbMap;

    usedRbs_.push_back(info);

    std::vector<UsedRBs>::iterator it = usedRbs_.begin();
    while (it != usedRbs_.end())  // purge old allocations
    {
        if (it->time_ < NOW - 0.002)
            usedRbs_.erase(it++);
        else
            ++it;
    }
    lastActive_ = NOW;

    EV << NOW << " LtePhyUeD2D::handleUpperMessage - message from stack" << endl;
    LteAirFrame* frame = NULL;

    if (lteInfo->getFrameType() == HARQPKT || lteInfo->getFrameType() == GRANTPKT || lteInfo->getFrameType() == RACPKT)
    {
        frame = new LteAirFrame("harqFeedback-grant");
    }
    else
    {
        // create LteAirFrame and encapsulate the received packet
        frame = new LteAirFrame("airframe");
    }

    frame->encapsulate(check_and_cast<cPacket*>(msg));

    // initialize frame fields

    frame->setSchedulingPriority(airFramePriority_);
    frame->setDuration(TTI);
    // set current position
    lteInfo->setCoord(getRadioPosition());

    lteInfo->setTxPower(txPower_);
    frame->setControlInfo(lteInfo);

    EV << "LtePhyUeD2D::handleUpperMessage - " << nodeTypeToA(nodeType_) << " with id " << nodeId_
       << " sending message to the air channel. Dest=" << lteInfo->getDestId() << endl; //UE

    // if this is a multicast/broadcast connection, send the frame to all neighbors in the hearing range
    // otherwise, send unicast to the destination
    if (lteInfo->getDirection() == D2D_MULTI)
        sendBroadcast(frame);
    else
        sendUnicast(frame);
}

void LtePhyUeD2D::storeAirFrame(LteAirFrame* newFrame)
{
    // implements the capture effect
    // store the frame received from the nearest transmitter
    UserControlInfo* newInfo = check_and_cast<UserControlInfo*>(newFrame->getControlInfo());
    Coord myCoord = getCoord();
    double distance = 0.0;
    double rsrpMean = 0.0;
    std::vector<double> rsrpVector;
    bool useRsrp = false;

    if (strcmp(par("d2dMulticastCaptureEffectFactor"),"RSRP") == 0)
    {
        useRsrp = true;

        double sum = 0.0;
        unsigned int allocatedRbs = 0;
        rsrpVector = channelModel_->getRSRP_D2D(newFrame, newInfo, nodeId_, myCoord);

        // get the average RSRP on the RBs allocated for the transmission
        RbMap rbmap = newInfo->getGrantedBlocks();
        RbMap::iterator it;
        std::map<Band, unsigned int>::iterator jt;
        //for each Remote unit used to transmit the packet
        for (it = rbmap.begin(); it != rbmap.end(); ++it)
        {
            //for each logical band used to transmit the packet
            for (jt = it->second.begin(); jt != it->second.end(); ++jt)
            {
                Band band = jt->first;
                if (jt->second == 0) // this Rb is not allocated
                    continue;

                sum += rsrpVector.at(band);
                allocatedRbs++;
            }
        }
        rsrpMean = sum / allocatedRbs;
        EV << NOW << " LtePhyUeD2D::storeAirFrame - Average RSRP from node " << newInfo->getSourceId() << ": " << rsrpMean ;
    }
    else  // distance
    {
        Coord newSenderCoord = newInfo->getCoord();
        distance = myCoord.distance(newSenderCoord);
        EV << NOW << " LtePhyUeD2D::storeAirFrame - Distance from node " << newInfo->getSourceId() << ": " << distance ;
    }

    if (!d2dReceivedFrames_.empty())
    {
        LteAirFrame* prevFrame = d2dReceivedFrames_.front();
        if (!useRsrp && distance < nearestDistance_)
        {
            EV << "[ < nearestDistance: " << nearestDistance_ << "]" << endl;

            // remove the previous frame
            d2dReceivedFrames_.pop_back();
            delete prevFrame;

            nearestDistance_ = distance;
            d2dReceivedFrames_.push_back(newFrame);
        }
        else if (rsrpMean > bestRsrpMean_)
        {
            EV << "[ > bestRsrp: " << bestRsrpMean_ << "]" << endl;

            // remove the previous frame
            d2dReceivedFrames_.pop_back();
            delete prevFrame;

            bestRsrpMean_ = rsrpMean;
            bestRsrpVector_ = rsrpVector;
            d2dReceivedFrames_.push_back(newFrame);
        }
        else
        {
            // this frame will not be decoded
            delete newFrame;
        }
    }
    else
    {
        if (!useRsrp)
        {
            nearestDistance_ = distance;
            d2dReceivedFrames_.push_back(newFrame);
        }
        else
        {
            bestRsrpMean_ = rsrpMean;
            bestRsrpVector_ = rsrpVector;
            d2dReceivedFrames_.push_back(newFrame);
        }
    }
}

LteAirFrame* LtePhyUeD2D::extractAirFrame()
{
    // implements the capture effect
    // the vector is storing the frame received from the strongest/nearest transmitter

    return d2dReceivedFrames_.front();
}

void LtePhyUeD2D::decodeAirFrame(LteAirFrame* frame, UserControlInfo* lteInfo)
{
    EV << NOW << " LtePhyUeD2D::decodeAirFrame - Start decoding..." << endl;
    if ((lteInfo->getUserTxParams()) != NULL)
    {
        int cw = lteInfo->getCw();
        if (lteInfo->getUserTxParams()->readCqiVector().size() == 1)
            cw = 0;
        double cqi = lteInfo->getUserTxParams()->readCqiVector()[cw];
        tSample_->sample_ = cqi;
        tSample_->id_ = nodeId_;
        tSample_->module_ = getMacByMacNodeId(nodeId_);
        emit(averageCqiD2D_, tSample_);
    }

    // apply decider to received packet
    bool result = true;

    RemoteSet r = lteInfo->getUserTxParams()->readAntennaSet();
    if (r.size() > 1)
    {
        // DAS
        for (RemoteSet::iterator it = r.begin(); it != r.end(); it++)
        {
            EV << "LtePhyUeD2D::decodeAirFrame: Receiving Packet from antenna " << (*it) << "\n";

            /*
             * On UE set the sender position
             * and tx power to the sender das antenna
             */

//            cc->updateHostPosition(myHostRef,das_->getAntennaCoord(*it));
            // Set position of sender
//            Move m;
//            m.setStart(das_->getAntennaCoord(*it));
            RemoteUnitPhyData data;
            data.txPower=lteInfo->getTxPower();
            data.m=getRadioPosition();
            frame->addRemoteUnitPhyDataVector(data);
        }
        // apply analog models For DAS
        result=channelModel_->errorDas(frame,lteInfo);
    }
    else
    {
        //RELAY and NORMAL
        if (lteInfo->getDirection() == D2D_MULTI)
            result = channelModel_->error_D2D(frame,lteInfo,bestRsrpVector_);
        else
            result = channelModel_->error(frame,lteInfo);
    }

    // update statistics
    if (result)
        numAirFrameReceived_++;
    else
        numAirFrameNotReceived_++;

    EV << "Handled LteAirframe with ID " << frame->getId() << " with result "
       << ( result ? "RECEIVED" : "NOT RECEIVED" ) << endl;

    cPacket* pkt = frame->decapsulate();

    // attach the decider result to the packet as control info
    lteInfo->setDeciderResult(result);
    pkt->setControlInfo(lteInfo);

    // send decapsulated message along with result control info to upperGateOut_
    send(pkt, upperGateOut_);

    if (getEnvir()->isGUI())
        updateDisplayString();
}


void LtePhyUeD2D::sendFeedback(LteFeedbackDoubleVector fbDl, LteFeedbackDoubleVector fbUl, FeedbackRequest req)
{
    Enter_Method("SendFeedback");
    EV << "LtePhyUeD2D: feedback from Feedback Generator" << endl;

    //Create a feedback packet
    LteFeedbackPkt* fbPkt = new LteFeedbackPkt();
    //Set the feedback
    fbPkt->setLteFeedbackDoubleVectorDl(fbDl);
    fbPkt->setLteFeedbackDoubleVectorDl(fbUl);
    fbPkt->setSourceNodeId(nodeId_);
    UserControlInfo* uinfo = new UserControlInfo();
    uinfo->setSourceId(nodeId_);
    if(par("unassistedD2D"))
        {
            MacNodeId destId_ = (dynamic_cast<LteMacUeRealisticD2D*>(getParentModule()->getSubmodule("mac")))->getLastContactedId();
            uinfo->setDestId(destId_);
        }
    if ((uinfo->getDestId()==0) || !(par("unassistedD2D")))
    {
            uinfo->setDestId(masterId_);
    }
    uinfo->setFrameType(FEEDBACKPKT);
    uinfo->setIsCorruptible(false);
    // create LteAirFrame and encapsulate a feedback packet
    LteAirFrame* frame = new LteAirFrame("feedback_pkt");
    frame->encapsulate(check_and_cast<cPacket*>(fbPkt));
    uinfo->feedbackReq = req;
    uinfo->setDirection(UL);
    simtime_t signalLength = TTI;
    uinfo->setTxPower(txPower_);
    uinfo->setD2dTxPower(d2dTxPower_);
    // initialize frame fields

    frame->setSchedulingPriority(airFramePriority_);
    frame->setDuration(signalLength);

    uinfo->setCoord(getRadioPosition());

    frame->setControlInfo(uinfo);
    //TODO access speed data Update channel index
//    if (coherenceTime(move.getSpeed())<(NOW-lastFeedback_)){
//        deployer_->channelIncrease(nodeId_);
//        deployer_->lambdaIncrease(nodeId_,1);
//    }
    lastFeedback_ = NOW;
    EV << "LtePhy: " << nodeTypeToA(nodeType_) << " with id "
       << nodeId_ << " sending feedback to the air channel" << endl;
    sendUnicast(frame);
}

void LtePhyUeD2D::initializeFeedbackComputation(cXMLElement* xmlConfig)
{
    lteFeedbackComputation_ = 0;

    if (xmlConfig == 0)
    {
        error("No feedback computation configuration file specified.");
        return;
    }

    cXMLElementList fbComputationList = xmlConfig->getElementsByTagName(
        "FeedbackComputation");

    if (fbComputationList.empty())
    {
        error(
            "No feedback computation configuration found in configuration file.");
        return;
    }

    if (fbComputationList.size() > 1)
    {
        error(
            "More than one feedback computation configuration found in configuration file.");
        return;
    }

    cXMLElement* fbComputationData = fbComputationList.front();

    const char* name = fbComputationData->getAttribute("type");

    if (name == 0)
    {
        error(
            "Could not read type of feedback computation from configuration file.");
        return;
    }

    ParameterMap params;
    getParametersFromXML(fbComputationData, params);

    lteFeedbackComputation_ = getFeedbackComputationFromName(name, params);

    EV << "Feedback Computation \"" << name << "\" loaded." << endl;
}

LteFeedbackComputation* LtePhyUeD2D::getFeedbackComputationFromName(
    std::string name, ParameterMap& params)
{
    ParameterMap::iterator it;
    if (name == "REAL")
    {
        //default value
        double targetBler = 0.1;
        double lambdaMinTh = 0.02;
        double lambdaMaxTh = 0.2;
        double lambdaRatioTh = 20;
        it = params.find("targetBler");
        if (it != params.end())
        {
            targetBler = params["targetBler"].doubleValue();
        }
        it = params.find("lambdaMinTh");
        if (it != params.end())
        {
            lambdaMinTh = params["lambdaMinTh"].doubleValue();
        }
        it = params.find("lambdaMaxTh");
        if (it != params.end())
        {
            lambdaMaxTh = params["lambdaMaxTh"].doubleValue();
        }
        it = params.find("lambdaRatioTh");
        if (it != params.end())
        {
            lambdaRatioTh = params["lambdaRatioTh"].doubleValue();
        }
        LteFeedbackComputation* fbcomp = new LteFeedbackComputationRealistic(
            targetBler, deployer_->getLambda(), lambdaMinTh, lambdaMaxTh,
            lambdaRatioTh, deployer_->getNumBands());
        return fbcomp;
    }
    else
        return 0;
}

