//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_LTEMACUEREALISTICD2D_H_
#define _LTE_LTEMACUEREALISTICD2D_H_

#include "stack/mac/layer/LteMacUeRealistic.h"
#include "stack/mac/layer/LteMacEnbRealisticD2D.h"
#include "stack/mac/buffer/harq_d2d/LteHarqBufferTxD2D.h"
#include "common/LteCommon.h"
#include "corenetwork/binder/LteBinder.h"

class LteSchedulingGrant;
class LteSchedulerUeUl;
class LteBinder;

class LteSchedulerUeUnassistedD2D;

class ExposedLteMacEnb : public LteMacEnbRealisticD2D {
public:
    LteDeployer* getDeployer() {
        return LteMacEnb::deployer_;
    }
};

class LteMacUeRealisticD2D : public LteMacUeRealistic
{
    /// Local LteDeployer
    LteDeployer *deployer_;

    /// Lte AMC module
    LteAmc *amc_;

    /// Number of antennas (MACRO included)
    int numAntennas_;

    // reference to the eNB
    LteMacEnbRealisticD2D* enb_;
    ExposedLteMacEnb* eNodeBFunctions;

    LteSchedulerUeUnassistedD2D* lteSchedulerUeUnassistedD2DSLRx_; // Similar to eNB scheduler for DL
    LteSchedulerUeUnassistedD2D* lteSchedulerUeUnassistedD2DSLTx_; // Similar to eNB Scheduler for UL

    // RAC Handling variables
    bool racD2DMulticastRequested_;
    // Multicast D2D BSR handling
    bool bsrD2DMulticastTriggered_;

    // if true, use the preconfigured TX params for transmission, else use that signaled by the eNB
    bool usePreconfiguredTxParams_;
    UserTxParams* preconfiguredTxParams_;
    UserTxParams* getPreconfiguredTxParams();  // build and return new user tx params

    struct RacVariables
    {
        bool racRequested_ = false;
        unsigned int racBackoffTimer_;
        unsigned int maxRacTryouts_;
        unsigned int currentRacTry_;
        unsigned int minRacBackoff_;
        unsigned int maxRacBackoff_;

        unsigned int raRespTimer_;
        unsigned int raRespWinStart_;

        RacVariables()
        {

        }
    };

    std::map<MacCid,RacVariables> RacMap;

    MacNodeId lastContactedId_;

  protected:
    //Statistics
    simsignal_t activatedFrames_;
    simsignal_t sleepFrames_;
    simsignal_t wastedFrames_;
    simsignal_t cqiDlSpmux0_;
    simsignal_t cqiDlSpmux1_;
    simsignal_t cqiDlSpmux2_;
    simsignal_t cqiDlSpmux3_;
    simsignal_t cqiDlSpmux4_;
    simsignal_t cqiDlTxDiv0_;
    simsignal_t cqiDlTxDiv1_;
    simsignal_t cqiDlTxDiv2_;
    simsignal_t cqiDlTxDiv3_;
    simsignal_t cqiDlTxDiv4_;
    simsignal_t cqiDlMuMimo0_;
    simsignal_t cqiDlMuMimo1_;
    simsignal_t cqiDlMuMimo2_;
    simsignal_t cqiDlMuMimo3_;
    simsignal_t cqiDlMuMimo4_;
    simsignal_t cqiDlSiso0_;
    simsignal_t cqiDlSiso1_;
    simsignal_t cqiDlSiso2_;
    simsignal_t cqiDlSiso3_;
    simsignal_t cqiDlSiso4_;
    //number of resource block allcated in last tti
    unsigned int lastTtiAllocatedRb_;
    // Current subframe type
    LteSubFrameType currentSubFrameType_;
    /// Buffer for the BSRs
    LteMacBufferMap bsrbuf_;
    /*
     * Map associating a nodeId with the corresponding RX H-ARQ buffer.
     * Used in eNB for D2D communications. The key value of the map is
     * the *receiver* of the D2D flow
     */
    HarqRxBuffersMirror harqRxBuffersD2DMirror_;
    /**
     * Reads MAC parameters for ue and performs initialization.
     */
    virtual void initialize(int stage);

    /**
     * Analyze gate of incoming packet
     * and call proper handler
     */
    virtual void handleMessage(cMessage *msg);
    /**
     * bufferizeBsr() works much alike bufferizePacket()
     * but only saves the BSR in the corresponding virtual
     * buffer, eventually creating it if a queue for that
     * cid does not exists yet.
     *
     * @param bsr bsr to store
     * @param cid connection id for this bsr
     */
    void bufferizeBsr(MacBsr* bsr, MacCid cid);

    /**
     * macPduUnmake() extracts SDUs from a received MAC
     * PDU and sends them to the upper layer.
     *
     * For unassissted D2D it also extracts the BSR Control Element
     * and stores it in the BSR buffer (for the cid from
     * which packet was received)
     *
     *
     * @param pkt container packet
     */
    virtual void macPduUnmake(cPacket* pkt);

    /**
     * Main loop
     */
    virtual void handleSelfMessage();

    virtual void macHandleFeedbackPkt(cPacket *pkt);

    virtual void macHandleGrant(cPacket* pkt);

    /*
     * Checks RAC status
     */
    virtual void checkRAC();

    /*
     * Checks RAC status
     */
    virtual void checkRAC(MacCid ueRxD2DId);

    /*
     * Receives and handles RAC responses
     */
    virtual void macHandleRac(cPacket* pkt);

    /*
     * Receives and handles RAC requests
     */
    virtual void macHandleRacRequest(cPacket* pkt);

    void macHandleD2DModeSwitch(cPacket* pkt);

    virtual LteMacPdu* makeBsr(int size);

    /**
     * macPduMakeUnassistedD2D() creates MAC PDUs (one for each CID)
     * by extracting SDUs from Real Mac Buffers according
     * to the Schedule List.
     * It sends them to H-ARQ (at the moment lower layer)
     *
     * On UE it also adds a BSR control element to the MAC PDU
     * containing the size of its buffer (for that CID)
     */
    virtual void macPduMake();

  public:
    /**
     * Getter for AMC module
     */
    virtual LteAmc *getAmc()
    {
        return amc_;
    }
    /// Returns the BSR virtual buffers
    LteMacBufferMap* getBsrVirtualBuffers()
    {
        return &bsrbuf_;
    }

    // Power Model Parameters
    /* minimum depleted power (W)
     * @par dir link direction
     */
    double getZeroLevel(Direction dir, LteSubFrameType type);
    /* idle state depleted power (W)
     * @par dir link direction
     */
    double getIdleLevel(Direction dir, LteSubFrameType type);
    /*
     * Update the number of allocatedRb in last tti
     * @par number of resource block
     */
    void allocatedRB(unsigned int rb);
    /*
      * getter for current sub-frame type.
      * It sould be NORMAL_FRAME_TYPE, MBSFN, SYNCRO
      * BROADCAST, PAGING and ABS
      * It is used in order to compute the energy consumption
      */
     LteSubFrameType getCurrentSubFrameType() const
     {
         return currentSubFrameType_;
     }
     /* per-block depletion unit (W)
      * @par dir link direction
      */
     double getPowerUnit(Direction dir, LteSubFrameType type);
    //
    // Getter for associated ENodeb
    //
    virtual  LteMacEnbRealisticD2D* getENodeB()
    {
        return eNodeBFunctions;
    }

    virtual LteDeployer* getDeployer(MacNodeId nodeId)
    {
        LteBinder* temp = getBinder();
        // Check if nodeId is a relay, if nodeId is a eNodeB
        // function GetNextHop returns nodeId
        // TODO change this behavior (its not needed unless we don't implement relays)
        MacNodeId id = temp->getNextHop(nodeId);
        OmnetId omnetid = temp->getOmnetId(id);
        return check_and_cast<LteDeployer*>(getSimulation()->getModule(omnetid)->getSubmodule("deployer"));
    }

    LteDeployer* getDeployer()
    {
        // Get local deployer
        if (deployer_ != NULL)
            return deployer_;

        return check_and_cast<LteDeployer*>(getParentModule()-> getParentModule()-> getParentModule()-> getSubmodule("eNodeB")->getSubmodule("deployer")); // Deployer
    }

    LteBinder* getSimulationBinder()
    {
        // Get local binder
        if (binder_ != NULL)
            return binder_;

        // return check_and_cast<LteDeployer*>(getParentModule()-> getParentModule()-> getParentModule()-> getSubmodule("eNodeB")->getSubmodule("binder")); // Binder
        return check_and_cast<LteBinder*>(getSimulation()->getModuleByPath("binder"));
    }
    LteBinder* geteNBBinder(LteMacEnbRealisticD2D* refEnb_)
    {
        binder_ = getBinder();
        return binder_;
    }

    MacNodeId geteNBMacID()
    {
        return enb_->getMacNodeId();
    }

    LteMacUeRealisticD2D();
    virtual ~LteMacUeRealisticD2D();

    virtual bool isD2DCapable()
    {
        return true;
    }

    virtual void triggerBsr(MacCid cid)
    {
        if (connDesc_[cid].getDirection() == D2D_MULTI)
            bsrD2DMulticastTriggered_ = true;
        else
            bsrTriggered_ = true;
    }
    // update the status of the "mirror" RX-Harq Buffer for this node
    void storeRxHarqBufferMirror(MacNodeId id, LteHarqBufferRxD2DMirror* mirbuff);
    // get the reference to the "mirror" buffers
    HarqRxBuffersMirror* getRxHarqBufferMirror();
    // delete the "mirror" RX-Harq Buffer for this node (useful at mode switch)
    void deleteRxHarqBufferMirror(MacNodeId id);

    /**
     * creates scheduling grants (one for each nodeId) according to the Schedule List.
     * It sends them to the  lower layer
     */
    virtual void sendGrants(LteMacScheduleList* scheduleList);
    /// Upper Layer Handler
    void fromRlc(cPacket *pkt);

    /// Lower Layer Handler
    void fromPhy(cPacket *pkt);


    /**
     * Returns the scheduling discipline for the given direction.
     * @par dir link direction
     */
    SchedDiscipline getSchedDiscipline();


    void cqiStatistics(MacNodeId id, Direction dir, LteFeedback fb);

    void setLastContactedId(MacNodeId id)
    {
        lastContactedId_ = id;
    }

    MacNodeId getLastContactedId()
    {
        return lastContactedId_;
    }

};

#endif
