//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_LTEMAXCI_H_
#define _LTE_LTEMAXCI_H_

#include "LteSchedulerAutoD2D.h"

class LteMaxCiAutoD2D : public virtual LteSchedulerAutoD2D
{
  protected:

    typedef SortedDesc_AutoD2D<MacCid, unsigned int> ScoreDesc;
    typedef std::priority_queue<ScoreDesc> ScoreList;

  public:

    virtual void prepareSchedule();

    virtual void commitSchedule();

    // *****************************************************************************************

    void notifyActiveConnection(MacCid cid);

    void removeActiveConnection(MacCid cid);

    void updateSchedulingInfo();
};

#endif // _LTE_LTEMAXCI_H_
