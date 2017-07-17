//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "LteRlcUmRealisticAutoD2D.h"
//#include "D2DModeSwitchNotification_m.h"

#include "LteMacSduRequest.h"

Define_Module(LteRlcUmRealisticAutoD2D);

void LteRlcUmRealisticAutoD2D::handleMessage(cMessage *msg) {
    LteRlcUm::handleMessage(msg);
}

//void LteRlcUmRealisticAutoD2D::handleLowerMessage(cPacket *pkt) {
//    if (strcmp(pkt->getName(), "AutoD2DModeSwitchNotification") == 0) {
//        EV << NOW
//                  << " LteRlcUmRealisticAutoD2D::handleLowerMessage - Received packet "
//                  << pkt->getName() << " from lower layer\n";
//
//        // add here specific behavior for handling mode switch at the RLC layer
//        AutoD2DModeSwitchNotification* switchPkt = check_and_cast<
//                AutoD2DModeSwitchNotification*>(pkt);
//        FlowControlInfo* lteInfo = check_and_cast<FlowControlInfo*>(
//                switchPkt->getControlInfo());
//
//        if (switchPkt->getTxSide()) {
//            // get the corresponding Rx buffer & call handler
//            UmTxEntity* txbuf = getTxBuffer(lteInfo);
//            txbuf->rlcHandleAutoD2DModeSwitch(switchPkt->getOldConnection());
//
//            // forward packet to PDCP
//            EV << "LteRlcUmRealisticAutoD2D::handleLowerMessage - Sending packet "
//                      << pkt->getName() << " to port UM_Sap_up$o\n";
//            send(pkt, up_[OUT]);
//        } else  // rx side
//        {
//            // get the corresponding Rx buffer & call handler
//            UmRxEntity* rxbuf = getRxBuffer(lteInfo);
//            rxbuf->rlcHandleAutoD2DModeSwitch(switchPkt->getOldConnection(),
//                    switchPkt->getOldMode());
//
//            delete switchPkt;
//        }
//    } else
//        LteRlcUmRealistic::handleLowerMessage(pkt);
//}

void LteRlcUmRealisticAutoD2D::initialize(int stage) {
    if (stage == 0) {
        // check the MAC module type: if it is not "RealisticAutoD2D", abort simulation
        std::string nodeType = getParentModule()->getParentModule()->par(
                "nodeType").stdstringValue();
        std::string macType = getParentModule()->getParentModule()->par(
                "LteMacType").stdstringValue();
        std::string pdcpType = getParentModule()->getParentModule()->par(
                "LtePdcpRrcType").stdstringValue();

       if (nodeType.compare("UE") == 0) {
            nodeType_ = UE;
            if (macType.compare("LteMacUeRealisticAutoD2D") != 0)
                throw cRuntimeError(
                        "LteRlcUmRealisticAutoD2D::initialize - %s module found, must be LteMacUeRealisticAutoD2D. Aborting",
                        macType.c_str());

            if (pdcpType.compare("LtePdcpRrcUeAutoD2D") != 0)
                throw cRuntimeError(
                        "LteRlcUmRealisticAutoD2D::initialize - %s module found, must be LtePdcpRrcUeAutoD2D. Aborting",
                        pdcpType.c_str());
        }

        up_[IN] = gate("UM_Sap_up$i");
        up_[OUT] = gate("UM_Sap_up$o");
        down_[IN] = gate("UM_Sap_down$i");
        down_[OUT] = gate("UM_Sap_down$o");

        WATCH_MAP(txEntities_);
        WATCH_MAP(rxEntities_);
    }
}

UmTxEntity* LteRlcUmRealisticAutoD2D::getTxBuffer(FlowControlInfo* lteInfo) {
    MacNodeId nodeId = ctrlInfoToUeId(lteInfo);
    LogicalCid lcid = lteInfo->getLcid();

    // Find TXBuffer for this CID
    MacCid cid = idToMacCid(nodeId, lcid);
    UmTxEntities::iterator it = txEntities_.find(cid);
    if (it == txEntities_.end()) {
        // Not found: create
        std::stringstream buf;
        // FIXME HERE

        buf << "UmTxEntity Lcid: " << lcid;
        cModuleType* moduleType = cModuleType::get("lte.stack.rlc.UmTxEntity");
        UmTxEntity* txEnt = check_and_cast<UmTxEntity *>(
                moduleType->createScheduleInit(buf.str().c_str(),
                        getParentModule()));
        txEntities_[cid] = txEnt;    // Add to tx_entities map

        if (lteInfo != NULL) {
            // store control info for this flow
            txEnt->setFlowControlInfo(lteInfo->dup());
        }

        EV << "LteRlcUmRealisticAutoD2D : Added new UmTxEntity: " << txEnt->getId()
                  << " for node: " << nodeId << " for Lcid: " << lcid << "\n";

        return txEnt;
    } else {
        // Found
        EV << "LteRlcUmRealisticAutoD2D : Using old UmTxBuffer: "
                  << it->second->getId() << " for node: " << nodeId
                  << " for Lcid: " << lcid << "\n";

        return it->second;
    }
}

UmRxEntity* LteRlcUmRealisticAutoD2D::getRxBuffer(FlowControlInfo* lteInfo) {
    MacNodeId nodeId;
    if (lteInfo->getDirection() == DL)
        nodeId = lteInfo->getDestId();
    else
        nodeId = lteInfo->getSourceId();
    LogicalCid lcid = lteInfo->getLcid();

    // Find RXBuffer for this CID
    MacCid cid = idToMacCid(nodeId, lcid);

    UmRxEntities::iterator it = rxEntities_.find(cid);
    if (it == rxEntities_.end()) {
        // Not found: create
        std::stringstream buf;
        buf << "UmRxEntity Lcid: " << lcid;
        cModuleType* moduleType = cModuleType::get("lte.stack.rlc.UmRxEntity");
        UmRxEntity* rxEnt = check_and_cast<UmRxEntity *>(
                moduleType->createScheduleInit(buf.str().c_str(),
                        getParentModule()));
        rxEntities_[cid] = rxEnt;    // Add to rx_entities map

        // store control info for this flow
        rxEnt->setFlowControlInfo(lteInfo->dup());

        EV << "LteRlcUmRealisticAutoD2D : Added new UmRxEntity: " << rxEnt->getId()
                  << " for node: " << nodeId << " for Lcid: " << lcid << "\n";

        return rxEnt;
    } else {
        // Found
        EV << "LteRlcUmRealisticAutoD2D : Using old UmRxEntity: "
                  << it->second->getId() << " for node: " << nodeId
                  << " for Lcid: " << lcid << "\n";

        return it->second;
    }
}

void LteRlcUmRealisticAutoD2D::handleUpperMessage(cPacket *pkt) {
    EV << "LteRlcUmRealisticAutoD2D::handleUpperMessage - Received packet "
              << pkt->getName() << " from upper layer, size "
              << pkt->getByteLength() << "\n";

    FlowControlInfo* lteInfo = check_and_cast<FlowControlInfo*>(
            pkt->removeControlInfo());

    UmTxEntity* txbuf = getTxBuffer(lteInfo);

    // Create a new RLC packet
    LteRlcSdu* rlcPkt = new LteRlcSdu("rlcUmPkt");
    rlcPkt->setSnoMainPacket(lteInfo->getSequenceNumber());
    rlcPkt->setLengthMainPacket(pkt->getByteLength());
    rlcPkt->encapsulate(pkt);

    // create a message so as to notify the MAC layer that the queue contains new data
    LteRlcPdu* newDataPkt = new LteRlcPdu("newDataPkt");
    // make a copy of the RLC SDU
    LteRlcSdu* rlcPktDup = rlcPkt->dup();
    // the MAC will only be interested in the size of this packet
    newDataPkt->encapsulate(rlcPktDup);
    newDataPkt->setControlInfo(lteInfo->dup());

    EV << "LteRlcUmRealisticAutoD2D::handleUpperMessage - Sending message "
              << newDataPkt->getName() << " to port UM_Sap_down$o\n";
    send(newDataPkt, down_[OUT]);

    rlcPkt->setControlInfo(lteInfo);

    drop(rlcPkt);

    // Bufferize RLC SDU
    EV << "LteRlcUmRealisticAutoD2D::handleUpperMessage - Enqueue packet "
              << rlcPkt->getName() << " into the Tx Buffer\n";
    txbuf->enque(rlcPkt);
}

void LteRlcUmRealisticAutoD2D::handleLowerMessage(cPacket *pkt) {
    EV << "LteRlcUmRealisticAutoD2D::handleLowerMessage - Received packet "
              << pkt->getName() << " from lower layer\n";

    FlowControlInfo* lteInfo = check_and_cast<FlowControlInfo*>(
            pkt->getControlInfo());

    if (strcmp(pkt->getName(), "LteMacSduRequest") == 0) {
        // get the corresponding Tx buffer
        UmTxEntity* txbuf = getTxBuffer(lteInfo);

        LteMacSduRequest* macSduRequest = check_and_cast<LteMacSduRequest*>(
                pkt);
        unsigned int size = macSduRequest->getSduSize();

        drop(pkt);

        // do segmentation/concatenation and send a pdu to the lower layer
        txbuf->rlcPduMake(size);

        delete macSduRequest;
    } else {
        // Extract informations from fragment
        UmRxEntity* rxbuf = getRxBuffer(lteInfo);
        drop(pkt);

        // Bufferize PDU
        EV << "LteRlcUmRealisticAutoD2D::handleLowerMessage - Enque packet "
                  << pkt->getName() << " into the Rx Buffer\n";
        rxbuf->enque(pkt);
    }
}

void LteRlcUmRealisticAutoD2D::deleteQueues(MacNodeId nodeId) {
    UmTxEntities::iterator tit;
    UmRxEntities::iterator rit;

    LteNodeType nodeType;
    std::string nodeTypePar = getAncestorPar("nodeType").stdstringValue();
    if (strcmp(nodeTypePar.c_str(), "ENODEB") == 0)
        nodeType = ENODEB;
    else
        nodeType = UE;

    // at the UE, delete all connections
    // at the eNB, delete connections related to the given UE
    for (tit = txEntities_.begin(); tit != txEntities_.end();) {
        if (nodeType == UE
                || (nodeType == ENODEB && MacCidToNodeId(tit->first) == nodeId)) {
            delete tit->second;        // Delete Entity
            txEntities_.erase(tit++);    // Delete Elem
        } else {
            ++tit;
        }
    }
    for (rit = rxEntities_.begin(); rit != rxEntities_.end();) {
        if (nodeType == UE
                || (nodeType == ENODEB && MacCidToNodeId(rit->first) == nodeId)) {
            delete rit->second;        // Delete Entity
            rxEntities_.erase(rit++);    // Delete Elem
        } else {
            ++rit;
        }
    }
}
