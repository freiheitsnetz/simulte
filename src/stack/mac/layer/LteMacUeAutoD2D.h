//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_LTEMACUEAUTOD2D_H_
#define _LTE_LTEMACUEAUTOD2D_H_


#include "LteMacUe.h"
#include "LteMacEnbD2D.h"
#include "LteHarqBufferTxD2D.h"
#include "LteHarqBufferTx.h"
#include "LteSchedulerUeAutoD2DDl.h"
#include "LteSchedulerUeAutoD2DUl.h"


class MacBsr;
class LteSchedulerUeAutoD2DUl;
class LteSchedulerUeAutoD2DDl;

class LteMacUeAutoD2D : public LteMacBase
{
protected:
    /*
     * Map associating a nodeId with the corresponding RX H-ARQ buffer.
     * Used in eNB for D2D communications. The key value of the map is
     * the *receiver* of the D2D flow
     */
    HarqRxBuffersMirror harqRxBuffersD2DMirror_;
    /// Local LteDeployer
    LteDeployer *deployer_;
    // reference to the eNB
    LteMacUeAutoD2D* ueAutoD2D_;

    /// Lte AMC module
    LteAmc *amc_;

    /// Number of antennas (MACRO included)
    int numAntennas_;

    int eNodeBCount;
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
    TaggedSample* tSample_;

    /**
     * Variable used for Downlink energy consumption computation
     */
    /*******************************************************************************************/

    // Current subframe type
    LteSubFrameType currentSubFrameType_;
    // MBSFN pattern
//        std::vector<LteSubFrameType> mbsfnPattern_;
    //frame index in the mbsfn pattern
    int frameIndex_;
    //number of resource block allcated in last tti
    unsigned int lastTtiAllocatedRb_;

    /*******************************************************************************************/
    // Resource Elements per Rb
    std::vector<double> rePerRb_;

    // Resource Elements per Rb - MBSFN frames
    std::vector<double> rePerRbMbsfn_;

    /// Buffer for the BSRs
    LteMacBufferMap bsrbuf_;

    /// Lte Mac Scheduler - Downlink
    LteSchedulerUeAutoD2DDl* ueAutoD2DSchedulerDl_;

    /// Lte Mac Scheduler - Uplink
    LteSchedulerUeAutoD2DUl* ueAutoD2DSchedulerUl_;

    /// Number of RB Dl
    int numRbDl_;

    /// Number of RB Ul
    int numRbUl_;



    /**
     * Analyze gate of incoming packet
     * and call proper handler
     */
    virtual void handleMessage(cMessage *msg);

    /**
     * creates scheduling grants (one for each nodeId) according to the Schedule List.
     * It sends them to the  lower layer
     */
    virtual void sendGrants(LteMacScheduleList* scheduleList);

     /**
     * Reads MAC parameters for ue and performs initialization.
     */
    virtual void initialize(int stage);

    /**
     * macPduMake() creates MAC PDUs (one for each CID)
     * by extracting SDUs from Real Mac Buffers according
     * to the Schedule List.
     * It sends them to H-ARQ (at the moment lower layer)
     *
     * On UE it also adds a BSR control element to the MAC PDU
     * containing the size of its buffer (for that CID)
     */
    virtual void macPduMake(LteMacScheduleList* scheduleList);

    /**
     * macPduUnmake() extracts SDUs from a received MAC
     * PDU and sends them to the upper layer.
     *
     * On ENB it also extracts the BSR Control Element
     * and stores it in the BSR buffer (for the cid from
     * which packet was received)
     *
     * @param pkt container packet
     */
    virtual void macPduUnmake(cPacket* pkt);

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
     * bufferizePacket() is called every time a packet is
     * received from the upper layer
     */
    virtual bool bufferizePacket(cPacket* pkt);

    /**
     * handleUpperMessage() is called every time a packet is
     * received from the upper layer
     */
    virtual void handleUpperMessage(cPacket* pkt);

    /**
     * Main loop
     */
    virtual void handleSelfMessage();
    /**
     * macHandleFeedbackPkt is called every time a feedback pkt arrives on MAC
     */
    virtual void macHandleFeedbackPkt(cPacket* pkt);

    /*
     * Receives and handles RAC requests and responses
     */
    virtual void macHandleRac(cPacket* pkt);

    /*
     * Update UserTxParam stored in every lteMacPdu when an rtx change this information
     */
    virtual void updateUserTxParam(cPacket* pkt);

    /**
     * deleteQueues() must be called on handover
     * to delete queues for a given user
     *
     * @param nodeId Id of the node whose queues are deleted
     */
    //    virtual void deleteQueues(MacNodeId nodeId);
    /**
     * deleteQueues() on ENB performs actions
     * from base class and also deletes the BSR buffer
     *
     * @param nodeId id of node performig handover
     */
    virtual void deleteQueues(MacNodeId nodeId);

//###################33 From LteMacUe
    // false if currentHarq_ counter needs to be initialized
    bool firstTx;

    LteSchedulerUeUl* lcgScheduler_;

    // configured grant - one each codeword
    LteSchedulingGrant* schedulingGrant_;

    // current H-ARQ process counter
    unsigned char currentHarq_;

    // perodic grant handling
    unsigned int periodCounter_;
    unsigned int expirationCounter_;

    // RAC Handling variables

    bool racRequested_;
    unsigned int racBackoffTimer_;
    unsigned int maxRacTryouts_;
    unsigned int currentRacTry_;
    unsigned int minRacBackoff_;
    unsigned int maxRacBackoff_;

    unsigned int raRespTimer_;
    unsigned int raRespWinStart_;

    // BSR handling
    bool bsrTriggered_;


//########################

protected:

    // reference to the eNB
//    LteMacEnbD2D* enb_;

    // if true, use the preconfigured TX params for transmission, else use that signaled by the eNB
    bool usePreconfiguredTxParams_;
    UserTxParams* preconfiguredTxParams_;
    UserTxParams* getPreconfiguredTxParams();  // build and return new user tx params

    // RAC Handling variables
    bool racD2DMulticastRequested_;
    // Multicast D2D BSR handling
    bool bsrD2DMulticastTriggered_;
    /*
     * Build a BSR to be sent to the eNB containing the size of the buffers
     * of D2D connections
     */
    LteMacPdu* makeBsr(int size);

    /*
     * Receives and handles scheduling grants
     */
    virtual void macHandleGrant(cPacket* pkt);

    /*
     * Checks RAC status
     */
    virtual void checkRAC();


    void macHandleD2DModeSwitch(cPacket* pkt);

//##########################

  public:

    /*
     * Access scheduling grant
     */
    inline const LteSchedulingGrant* getSchedulingGrant() const
    {
        return schedulingGrant_;
    }
    /*
     * Access current H-ARQ pointer
     */
    inline const unsigned char getCurrentHarq() const
    {
        return currentHarq_;
    }
    /*
     * Access BSR trigger flag
     */
    inline const bool bsrTriggered() const
    {
        return bsrTriggered_;
    }

    /* utility functions used by LCP scheduler
     * <cid> and <priority> are returned by reference
     * @return true if at least one backlogged connection exists
     */
    bool getHighestBackloggedFlow(MacCid& cid, unsigned int& priority);
    bool getLowestBackloggedFlow(MacCid& cid, unsigned int& priority);
    // update the status of the "mirror" RX-Harq Buffer for this node
    void storeRxHarqBufferMirror(MacNodeId id, LteHarqBufferRxD2DMirror* mirbuff);
    // get the reference to the "mirror" buffers
    HarqRxBuffersMirror* getRxHarqBufferMirror();
    // delete the "mirror" RX-Harq Buffer for this node (useful at mode switch)
    void deleteRxHarqBufferMirror(MacNodeId id);
    // send the D2D Mode Switch signal to the transmitter of the given flow
    void sendModeSwitchNotification(MacNodeId srcId, MacNodeId dst, LteD2DMode oldMode, LteD2DMode newMode);

    // update ID of the serving cell during handover
    virtual void doHandover(MacNodeId targetEnb);

    /// Returns the BSR virtual buffers
    LteMacBufferMap* getBsrVirtualBuffers()
    {
        return &bsrbuf_;
    }


    /**
     * Getter for AMC module
     */
    virtual LteAmc *getAmc()
    {
        return amc_;
    }

    /**
     * Getter for Deployer.
     */
    LteDeployer* getDeployer();

    /**
        * Getter for Deployer.
        */
   LteDeployer* getDeployer(MacNodeId nodeId);

    /**
     * Returns the number of system antennas (MACRO included)
     */
    virtual int getNumAntennas();

    /**
     * Returns the scheduling discipline for the given direction.
     * @par dir link direction
     */
    SchedDiscipline getSchedDiscipline(Direction dir);

    /**
     * Getter for RB Dl
     */
    int getNumRbDl();

    /**
     * Getter for RB Ul
     */
    int getNumRbUl();

    //! Lte Advanced Scheduler support

    /*
     * Getter for allocation RBs
     * @par dir link direction
     */
    unsigned int getAllocationRbs(Direction dir);

    /*
     * Getter for deadline
     * @par tClass traffic class
     * @par dir link direction
     */

    double getLteAdeadline(LteTrafficClass tClass, Direction dir);

    /*
     * Getter for slack-term
     * @par tClass traffic class
     * @par dir link direction
     */

    double getLteAslackTerm(LteTrafficClass tClass, Direction dir);

    /*
     * Getter for maximum urgent burst size
     * @par tClass traffic class
     * @par dir link direction
     */
    int getLteAmaxUrgentBurst(LteTrafficClass tClass, Direction dir);

    /*
     * Getter for maximum fairness burst size
     * @par tClass traffic class
     * @par dir link direction
     */
    int getLteAmaxFairnessBurst(LteTrafficClass tClass, Direction dir);

    /*
     * Getter for fairness history size
     * @par dir link direction
     */
    int getLteAhistorySize(Direction dir);
    /*
     * Getter for fairness TTI threshold
     * @par dir link direction
     */
    int getLteAgainHistoryTh(LteTrafficClass tClass, Direction dir);

    // Power Model Parameters
    /* minimum depleted power (W)
     * @par dir link direction
     */
    double getZeroLevel(Direction dir, LteSubFrameType type);
    /* idle state depleted power (W)
     * @par dir link direction
     */
    double getIdleLevel(Direction dir, LteSubFrameType type);
    /* per-block depletion unit (W)
     * @par dir link direction
     */
    double getPowerUnit(Direction dir, LteSubFrameType type);
    /* maximumum depletable power (W)
     * @par dir link direction
     */
    double getMaxPower(Direction dir, LteSubFrameType type);

    /* getter for the flag that enable PF legacy in LTEA scheduler
     *  @par dir link direction
     */
    bool getPfTmsAwareFlag(Direction dir);

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

    /*
     * Update the number of allocatedRb in last tti
     * @par number of resource block
     */
    void allocatedRB(unsigned int rb);

    /*
     * Return the current active set (active connections)
     * @par direction
     */
    ActiveSet getActiveSet(Direction dir);

    void cqiStatistics(MacNodeId id, Direction dir, LteFeedback fb);

    // get band occupation for this/previous TTI. Used for interference computation purposes
    unsigned int getBandStatus(Band b);
    unsigned int getPrevBandStatus(Band b);

  public:
    LteMacUeAutoD2D();
    virtual ~LteMacUeAutoD2D();

    virtual bool isD2DCapable()
    {
        return true;
    }
};

#endif _LTE_LTEMACUEAUTOD2D_H_
