/*
 * LteSchedulerBase.h
 *
 *  Created on: Oct 18, 2017
 *      Author: kunterbunt
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTESCHEDULERBASE_H_
#define STACK_MAC_SCHEDULING_MODULES_LTESCHEDULERBASE_H_

#include <omnetpp.h>
#include "stack/mac/scheduler/LteScheduler.h"
#include "common/LteCommon.h"

/**
 * Derive from this class to get basic functionality.
 */
class LteSchedulerBase : public virtual LteScheduler {
public:
	LteSchedulerBase() {}
	virtual ~LteSchedulerBase() {
		std::cout << dirToA(direction_) << ": " << numTTIs << " TTIs, of which "
				<< numTTIsWithNoActives << " had no active connections: "
				<< ((double) numTTIsWithNoActives / (double) numTTIs) << std::endl;
	}

	/**
	 * When the LteSchedulerEnb learns of an active connection it notifies the LteScheduler.
	 * It is essential to save this information. (I think this should be the default behaviour and be done in the LteScheduler class)
	*/
	void notifyActiveConnection(MacCid cid) override {
		EV_STATICCONTEXT;
		EV << NOW << " LteSchedulerBase::notifyActiveConnection(" << cid << ")" << std::endl;
		activeConnectionSet_.insert(cid);
	}

	/**
	 * When the LteSchedulerEnb learns of a connection going inactive it notifies the LteScheduler.
	*/
	void removeActiveConnection(MacCid cid) override {
		EV_STATICCONTEXT;
		EV << NOW << " LteSchedulerBase::removeActiveConnection(" << cid << ")" << std::endl;
		activeConnectionSet_.erase(cid);
	}

	virtual void commitSchedule() override {
		activeConnectionSet_ = activeConnectionTempSet_;
	}

protected:
	size_t numTTIs = 0, numTTIsWithNoActives = 0;

	enum SchedulingResult {
		OK = 0, TERMINATE, INACTIVE, INELIGIBLE
	};

	std::string schedulingResultToString(SchedulingResult result) {
		return (result == SchedulingResult::TERMINATE ? "TERMINATE" :
				result == SchedulingResult::INACTIVE ? "INACTIVE" :
				result == SchedulingResult::INELIGIBLE ? "INELIGIBLE"
				: "OK");
	}

	SchedulingResult schedule(MacCid connectionId, Band band) {
		bool terminate = false;
		bool active = true;
		bool eligible = true;

		std::vector<BandLimit> bandLimitVec;
		BandLimit bandLimit(band);
		bandLimitVec.push_back(bandLimit);

		// requestGrant(...) might alter the three bool values, so we can check them afterwards.
		unsigned long max = 4294967295U; // 2^32
		unsigned int granted = requestGrant(connectionId, max, terminate, active, eligible, &bandLimitVec);
		EV << " " << granted << " bytes granted." << std::endl;
		if (terminate)
			return SchedulingResult::TERMINATE;
		else if (!active)
			return SchedulingResult::INACTIVE;
		else if (!eligible)
			return SchedulingResult::INELIGIBLE;
		else
			return SchedulingResult::OK;
	}
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTESCHEDULERBASE_H_ */
