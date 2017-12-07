//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_LTE_SCHEDULER_UE_AUTOD2D_H_
#define _LTE_LTE_SCHEDULER_UE_AUTOD2D_H_

#include "../scheduling_modules/unassistedHeuristic/LteUnassistedD2DSchedulingAgent.h"
#include "stack/mac/scheduler/LteSchedulerEnb.h"
#include "stack/mac/layer/LteMacUeRealisticD2D.h"
#include "stack/mac/scheduler/LteSchedulerUeUl.h"
#include "common/LteCommon.h"
#include "stack/mac/amc/LteAmc.h"
#include "stack/mac/amc/UserTxParams.h"

/**
 * @class LteSchedulerUeUnassistedD2D
 *
 * LTE Unassisted/Autonomous D2D UE scheduler.
 */

class LteScheduler;
class LteAllocationModule;
class LteMacUeRealisticD2D;

class LteSchedulerUeUnassistedD2D: public LteSchedulerEnb {

    /******************
     * Friend classes
     ******************/

    // Lte Scheduler Modules access grants
    friend class LteUnassistedD2DSchedulingAgent;
    friend class LteUnassistedD2DDRR;
    friend class LteUnassistedD2DPF;
    friend class LteUnassistedD2DMaxCI;
    friend class LteUnassistedD2DRandom;


protected:


    // Inner Scheduler - default to Standard LCG
    LteUnassistedD2DSchedulingAgent* lteUnassistedD2DSchedulingAgent_;

    typedef std::map<MacNodeId, unsigned char> HarqStatus;

    typedef std::map<MacNodeId, bool> RacStatus;

    /// Minimum scheduling unit, represents the MAC SDU size
    unsigned int scheduleUnit_;

    //! Uplink Synchronous H-ARQ process counter - keeps track of currently active process on connected UES.
    HarqStatus harqStatus_;

    //! RAC requests flags: signals whether an UE shall be granted the RAC allocation
    RacStatus racStatus_;
    /**
     * Grant type per class of service.
     * FIXED has size, URGENT has max size, FITALL always has 4294967295U size.
     */
    std::map<LteTrafficClass, GrantType> grantTypeMap_;

    /**
     * Grant size per class of service.
     * FIXED has size, URGENT has max size, FITALL always has 4294967295U size.
     */
    std::map<LteTrafficClass, int> grantSizeMap_;

    /**
     * Updates current schedule list with RAC grant responses.
     * @return TRUE if OFDM space is exhausted.
     */
    virtual bool racschedule();
    /**
     * Updates current schedule list with HARQ retransmissions.
     * @return TRUE if OFDM space is exhausted.
     */
    virtual bool rtxschedule();

    /*
     * OFDMA frame management
     */

    /**
     * Records assigned resource blocks statistics.
     */
    void resourceBlockStatistics(bool sleep = false);
    /**
     * Checks Harq Descriptors and return the first free codeword.
     *
     * @param id
     * @param cw
     * @return
     */
    virtual bool checkEligibility(MacNodeId id, Codeword& cw);

    /**
     * Returns the available space for a given user, antenna, logical band and codeword, in bytes.
     *
     * @param id MAC node Id
     * @param antenna antenna
     * @param b band
     * @param cw codeword
     * @return available space in bytes
     */
    unsigned int availableBytes(const MacNodeId id, const Remote antenna, Band b, Codeword cw, Direction dir, int limit = -1);

public:

    // Owner MAC module (can be LteMacEnb on eNB or LteMacRelayEnb on Relays). Set via initialize().
    LteMacUeRealisticD2D *ueMac_;
    /*
     * constructor
     */
    LteSchedulerUeUnassistedD2D(LteMacUeRealisticD2D * mac);
    LteSchedulerUeUnassistedD2D();
    /*
     * destructor
     */
    virtual ~LteSchedulerUeUnassistedD2D();
    /**
     * Set Direction and bind the internal pointers to the MAC objects.
     * @param dir link direction
     * @param mac pointer to MAC module
     */
    void initialize(Direction dir, LteMacUeRealisticD2D* mac);

    /**
     * signals RAC request to the scheduler (called by Rx D2D Ue)
     */
    virtual void signalRac(const MacNodeId nodeId) {
        racStatus_[nodeId] = true;
    }


    /* Performs the standard LCG scheduling algorithm
     * @returns reference to scheduling list
     * SL-DL-TX Scheduling done by Rx D2D UE
     */
    LteMacScheduleList* scheduleTxD2Ds();

    /* Performs the standard LCG scheduling algorithm
     * @returns reference to scheduling list
     * SL-UL-TX Scheduling done by Tx D2D UE
     */
    LteMacScheduleList* scheduleData();

    virtual unsigned int scheduleGrant(MacCid cid, unsigned int bytes,
            bool& terminate, bool& active, bool& eligible,
            std::vector<BandLimit>* bandLim = NULL, Remote antenna = MACRO,
            bool limitBl = false);

    // Overloaded in LTE_SCHEDULER_ENB_UL
    virtual unsigned int schedulePerAcidRtx(MacNodeId nodeId, Codeword cw,
            unsigned char acid, std::vector<BandLimit>* bandLim = NULL,
            Remote antenna = MACRO, bool limitBl = false);

    unsigned int schedulePerAcidRtxD2D(MacNodeId destId, MacNodeId senderId,
            Codeword cw, unsigned char acid, std::vector<BandLimit>* bandLim =
            NULL, Remote antenna = MACRO, bool limitBl = false);

    //! Updates HARQ descriptor current process pointer (to be called every TTI by main loop).
    void updateHarqDescs();

    unsigned int allocatedCws(MacNodeId nodeId)
    {
        return allocatedCws_[nodeId];
    }

    /**
     * Adds an entry (if not already in) to scheduling list.
     * The function calls the LteScheduler notify().
     * @param cid connection identifier
     */
    void backlog(MacCid cid);
    /*****************
     * UTILITIES
     *****************/

    /**
     * Returns a particular LteScheduler subclass,
     * implementing the given discipline.
     * @param discipline scheduler discipline
     */
    LteUnassistedD2DSchedulingAgent* getScheduler(SchedDiscipline discipline, LteSchedulerUeUnassistedD2D* lteSchedulerUeUnassistedD2D_);
};

#endif // _LTE_LTE_SCHEDULER_UE_AUTOD2D_H_
