/*
 * LteUnassistedD2DDRR.cc
 *
 *  Created on: Aug 21, 2017
 *      Author: Sunil Upardrasta
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_UNASSISTEDHEURISTIC_LTEUNASSISTEDD2DDRR_H_
#define STACK_MAC_SCHEDULING_MODULES_UNASSISTEDHEURISTIC_LTEUNASSISTEDD2DDRR_H_

#include <map>
#include "common/Circular.h"
#include <stack/mac/scheduling_modules/unassistedHeuristic/LteUnassistedD2DSchedulingAgent.h>

class LteSchedulerUeUnassistedD2D;

class LteUnassistedD2DDRR: public virtual LteUnassistedD2DSchedulingAgent {
private:

  //! DRR descriptor.
  struct DrrDesc
  {
      //! DRR quantum, in bytes.
      unsigned int quantum_;
      //! Deficit, in bytes.
      unsigned int deficit_;
      //! True if this descriptor is in the active list.
      bool active_;
      //! True if this connection is eligible for service.
      bool eligible_;

      //! Create an inactive DRR descriptor.
      DrrDesc()
      {
          quantum_ = 0;
          deficit_ = 0;
          active_ = false;
          eligible_ = false;
      }
  };

  typedef std::map<MacCid, DrrDesc> DrrDescMap;
  typedef CircularList<MacCid> ActiveList;

  //! Deficit round-robin Active List
  ActiveList activeList_;

  //! Deficit round-robin Active List. Temporary variable used in the two phase scheduling operations
  ActiveList activeTempList_;

  //! Deficit round-robin descriptor per-connection map.
  DrrDescMap drrMap_;

  //! Deficit round-robin descriptor per-connection map. Temporary variable used in the two phase scheduling operations
  DrrDescMap drrTempMap_;

public:
  LteUnassistedD2DDRR(LteSchedulerUeUnassistedD2D* lteSchedulerUeUnassistedD2D_) {

      ueScheduler_ = lteSchedulerUeUnassistedD2D_;
  }

  ~LteUnassistedD2DDRR() {
  }


  // Scheduling functions ********************************************************************

  //virtual void schedule ();

  virtual void prepareSchedule();

  virtual void commitSchedule();

  // *****************************************************************************************

  void notifyActiveConnection(MacCid cid);

  void removeActiveConnection(MacCid cid);

  void updateSchedulingInfo();
};

#endif /* STACK_MAC_SCHEDULING_MODULES_UNASSISTEDHEURISTIC_LTEUNASSISTEDD2DDRR_H_ */
