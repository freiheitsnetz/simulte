//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#ifndef STACK_MAC_SCHEDULING_MODULES_REASSIGNMENT_LTEREASSIGNMENT_H_
#define STACK_MAC_SCHEDULING_MODULES_REASSIGNMENT_LTEREASSIGNMENT_H_

#include <omnetpp.h>
#include <LteScheduler.h>
#include "LteCommon.h"

class LteReassignment : public virtual LteScheduler {
public:
    LteReassignment();
    virtual ~LteReassignment();

    enum SchedulingResult {
        OK = 0, TERMINATE, INACTIVE, INELIGIBLE
    };

    std::string schedulingResultToString(LteReassignment::SchedulingResult result) {
        return (result == SchedulingResult::TERMINATE ? "TERMINATE" :
                result == SchedulingResult::INACTIVE ? "INACTIVE" :
                result == SchedulingResult::INELIGIBLE ? "INELIGIBLE" :
                "OK");
    }

    /**
     * Apply the algorithm on temporal object 'activeConnectionTempSet_'.
     */
    virtual void prepareSchedule() override;

    /**
     * Put the results from prepareSchedule() into the actually-used object 'activeConnectionSet_'.
     */
    virtual void commitSchedule() override;

    /**
     * When the LteSchedulerEnb learns of an active connection it notifies the LteScheduler.
     * It is essential to save this information. (I think this should be the default behaviour and be done in the LteScheduler class)
     */
    void notifyActiveConnection(MacCid cid) override {
        activeConnectionSet_.insert(cid);
    }

    /**
     * When the LteSchedulerEnb learns of a connection going inactive it notifies the LteScheduler.
     */
    void removeActiveConnection(MacCid cid) override {
        activeConnectionSet_.erase(cid);
    }

protected:
    LteReassignment::SchedulingResult schedule(MacCid connectionId, Band band);
};

#endif /* STACK_MAC_SCHEDULING_MODULES_REASSIGNMENT_LTEREASSIGNMENT_H_ */
