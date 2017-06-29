//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_LTESCHEDULERAUTOD2D_H_
#define _LTE_LTESCHEDULERAUTOD2D_H_

#include "LteCommon.h"
#include "LteMacUeAutoD2D.h"

/// forward declarations
class LteSchedulerUeAutoD2D;
//class LteMacUeAutoD2D;
//class LteAllocationModuleAutoD2D;

/**
 * Score-based schedulers descriptor.
 */
template<typename T, typename S>
struct SortedDesc_AutoD2D
{
    /// Connection identifier.
    T x_;
    /// Score value.
    S score_;

    /// Comparison operator to enable sorting.
    bool operator<(const SortedDesc_AutoD2D& y) const
        {
        return score_ < y.score_;
    }

  public:
    SortedDesc_AutoD2D(const T x, const S score)
    {
        x_ = x;
        score_ = score;
    }
};

/**
 * @class LteScheduler
 */
class LteSchedulerAutoD2D
{
  protected:

    /// MAC module, used to get parameters from NED
    LteMacUeAutoD2D *mac_;

    /// Associated LteSchedulerUeAutoD2D (it is the one who creates the LteScheduler)
    LteSchedulerUeAutoD2D* ueAutoD2DScheduler_;

//    // System allocator, carries out the block-allocation functions.
//    LteAllocationModuleAutoD2D *allocator_;

    /// Link Direction (DL/UL)
    Direction direction_;

    //! Set of active connections.
    ActiveSet activeConnectionSet_;

    //! General Active set. Temporary variable used in the two phase scheduling operations
    ActiveSet activeConnectionTempSet_;

    /// Cid List
    typedef std::list<MacCid> CidList;

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

//    CplexTest cplexTest_;

  public:

    /**
     * Default constructor.
     */
    LteSchedulerAutoD2D()
    {
        //    WATCH(activeSet_);
        activeConnectionSet_.clear();
    }
    /**
     * Destructor.
     */
    virtual ~LteSchedulerAutoD2D()
    {
    }

    /**
     * Initializes the LteScheduler.
     * @param ueAutoD2DScheduler ueAutoD2D scheduler
     */
    virtual void setUeAutoD2DScheduler(LteSchedulerUeAutoD2D* UeAutoD2DScheduler);



    // Scheduling functions ********************************************************************

    /**
     * The schedule function is splitted in two phases
     *  - in the first phase, carried out by the prepareSchedule(),
     *    the computation of the algorithm on temporary structures is performed
     *  - in the second phase, carried out by the storeSchedule(),
     *    the commitment to the permanent variables is performed
     *
     * In this way, if in the environment there's a special module which wants to execute
     * more schedulers, compare them and pick a single one, the execSchedule() of each
     * scheduler is executed, but only the storeSchedule() of the picked one will be executed.
     * The schedule() simply call the sequence of execSchedule() and storeSchedule().
     */

    void schedule();

    virtual void prepareSchedule()
    {
    }
    virtual void commitSchedule()
    {
    }
    // *****************************************************************************************

    /// performs request of grant to the eNbScheduler
    virtual unsigned int requestGrant(MacCid cid, unsigned int bytes, bool& terminate, bool& active, bool& eligible , std::vector<BandLimit>* bandLim = NULL);

    /// calls eNbScheduler rtxschedule()
    virtual bool scheduleRetransmissions();

    /// calls LteSchedulerEnbUl serveRacs()
    virtual void scheduleRacRequests();

    /// calls LteSchedulerEnbUl racGrantEnb()
    virtual void requestRacGrant(MacNodeId nodeId);

    virtual void notifyActiveConnection(MacCid activeCid)
    {
    }

    virtual void removeActiveConnection(MacCid cid)
    {
    }

    virtual void updateSchedulingInfo()
    {
    }

    ActiveSet readActiveSet()
    {
        ActiveSet::iterator it = activeConnectionSet_.begin();
        ActiveSet::iterator et = activeConnectionSet_.end();
        MacCid cid;
        for (; it != et; ++it)
        {
            cid = *it;
        }
        return activeConnectionSet_;
    }

  private:

    /**
     * Utility function.
     * Initializes grantType_ and grantSize_ maps using mac NED parameters.
     * Note: mac_ amd direction_ should be initialized.
     */
    void initializeGrants();

};

#endif // _LTE_LTESCHEDULERAUTOD2D_H_
