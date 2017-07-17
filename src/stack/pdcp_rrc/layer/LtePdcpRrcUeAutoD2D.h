//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_LTEPDCPRRCUEAUTOD2D_H_
#define _LTE_LTEPDCPRRCUEAUTOD2D_H_

#include <omnetpp.h>
#include "LtePdcpRrc.h"

/**
 * @class LtePdcp
 * @brief PDCP Layer
 *
 * This is the PDCP/RRC layer of LTE Stack (with AutoD2D support).
 *
 */
class LtePdcpRrcUeAutoD2D: public LtePdcpRrcBase {
//    std::map<MacNodeId, LteD2DMode> peeringMap_;

protected:

    virtual void initialize(int stage);
    virtual int numInitStages() const {
        return inet::NUM_INIT_STAGES;
    }

    virtual void handleMessage(cMessage *msg);

    void handleControlInfo(cPacket* upPkt, FlowControlInfo* lteInfo) {
        delete lteInfo;
    }

    // Is direction DL or UL
    Direction getDirection(MacNodeId destId) {
        if (binder_->checkD2DCapability(nodeId_, destId)
                && binder_->getD2DMode(nodeId_, destId) == DM)
            return D2D;
        return UL;
    }

    /**
     * handler for data port
     * @param pkt incoming packet
     */
    virtual void fromDataPort(cPacket *pkt);

    // handler for mode switch signal
    void pdcpHandleD2DModeSwitch(MacNodeId peerId, LteD2DMode newMode);

    MacNodeId getDestId(FlowControlInfo* lteInfo) {
        // dest id
        MacNodeId destId = binder_->getMacNodeId(
                IPv4Address(lteInfo->getDstAddr()));
        // master of this ue (myself or a relay)
        MacNodeId master = binder_->getNextHop(destId);
        if (master != nodeId_) { // ue is relayed, dest must be the relay
            destId = master;
        } // else ue is directly attached
        return destId;
    }

    Direction getDirection() {
        // Data coming from Dataport on ENB are always Downlink
        return D2D;
    }
public:

};

#endif
