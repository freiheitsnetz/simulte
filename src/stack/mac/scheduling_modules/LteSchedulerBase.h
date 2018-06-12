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
#include "stack/mac/scheduler/LteSchedulerEnb.h" // add to getScheduler() function in .cc (also #include <your scheduler>).
#include "common/LteCommon.h" // add to enum 'SchedDiscipline' and 'SchedDisciplineTable disciplines[]'.
#include "common/oracle/Oracle.h"
#include "stack/mac/buffer/LteMacBuffer.h"
#include "stack/mac/scheduling_modules/common/UserManager.h"

/**
 * Derive from this class to get basic functionality.
 */
class LteSchedulerBase : public virtual LteScheduler {
public:
	LteSchedulerBase() : LteScheduler() {}
	virtual ~LteSchedulerBase();

	enum SchedulingResult {
		OK = 0, TERMINATE, INACTIVE, INELIGIBLE
	};

	std::string schedulingResultToString(SchedulingResult result) {
		return (result == SchedulingResult::TERMINATE ? "TERMINATE" :
				result == SchedulingResult::INACTIVE ? "INACTIVE" :
				result == SchedulingResult::INELIGIBLE ? "INELIGIBLE"
				: "OK");
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

	/**
	 * Schedule all connections. Call scheduleUe and scheduleUeReuse to remember decisions.
	 * Decisions will be applied at commitSchedule().
	 */
	virtual void schedule(std::set<MacCid>& connections) = 0;

	virtual void prepareSchedule() override;

	/**
	 * When a schedule is committed, this function can react to scheduling grant request results in whatever way.
	 */
	virtual void reactToSchedulingResult(const SchedulingResult& result, unsigned int numBytesGranted, const MacCid& connection) {
		// Override to do something here.
	}

	virtual void commitSchedule() override;

	const size_t getNumTTIs() const {
		return numTTIs;
	}

	UserManager& getUserManager();

	static void setD2D(User* user) {
        user->setD2D(Oracle::get()->isD2DFlow(user->getConnectionId()));
    }

	static MacNodeId findPartnerFunc(MacNodeId id) {
		try {
			MacNodeId partnerId = Oracle::get()->getTransmissionPartner(id);
			cout << "User Manager found partner for '" << Oracle::get()->getName(id) << "': '" << Oracle::get()->getName(partnerId) << "'." << endl;
			return partnerId;
		} catch (const exception& e) {
			cerr << "User Manager couldn't find transmission partner for " << Oracle::get()->getName(id) << ": '" << e.what() << "'." << endl;
			return User::PARTNER_UNSET;
		}
	}

	void setEnbSchedulerPtr(LteSchedulerEnb* scheduler) {
        this->eNbScheduler_ = scheduler;
    }

	LteSchedulerEnb* getEnbSchedulerPtr() {
	    return this->eNbScheduler_;
	}

protected:
	unsigned int numBytesGrantedLast = 0;
	/** Maps a connection to the list of resources that the schedule() function decided to schedule to it. */
	std::map<MacCid, std::vector<Band>> schedulingDecisions;
	/** Maps a connection to the list of resources that the schedule() function decided to schedule to it for frequency reuse. */
	std::map<MacCid, std::vector<Band>> reuseDecisions;

	UserManager userManager;

	/**
	 * Remember to schedule 'resource' to 'connection' at the end of this scheduling round.
	 */
	void scheduleUe(const MacCid& connection, const Band& resource) {
		schedulingDecisions[connection].push_back(resource);
	}

	/**
	 * Remember to schedule 'resource' to 'connection' at the end of this scheduling round by bypassing the allocator and directly setting
	 * the corresponding entry in the allocation matrix.
	 */
	void scheduleUeReuse(const MacCid& connection, const Band& resource) {
		reuseDecisions[connection].push_back(resource);
	}

	/**
	 * @return Can be D2D, D2D_MULTI, UL, DL.
	 */
	Direction getDirection(const MacCid& connection);

	/**
	 * @param nodeId
	 * @param band The logical band you're interested in.
	 * @param numBlocks The number of resource blocks available on the band (should be 1!).
	 * @param dir
	 */
	unsigned int getBytesOnBand(const MacNodeId& nodeId, const Band& band, const unsigned int& numBlocks, const Direction& dir);

	/**
	 * @return Average number of bytes available on 1 RB.
	 */
	unsigned int getAverageBytesPerBlock(const MacCid& connection);

	/**
	 * @return The total demand in bytes of 'connection' found by scanning the BSR buffer at the eNodeB.
	 */
	unsigned int getTotalDemand(const MacCid& connection);

	/**
	 * @return Number of bytes the UE wants to send according to the first BSR stored.
	 */
	unsigned int getCurrentDemand(const MacCid& connection);

	/**
	 * @return Number of blocks required to serve 'numBytes' for 'connection'.
	 */
	double getRBDemand(const MacCid& connection, const unsigned int& numBytes);

	static bool defaultFilter(User* user) {
	    return false; // don't filter.
	}

	std::function<bool (User* user)> userFilter = LteSchedulerBase::defaultFilter;

	/**
	 * Sets the user filter applied at the user manager. Let it return 'true' to filter a user. That user will not be in the user list then.
	 */
	void setUserFilter(std::function<bool (User* user)> filter) {
        this->userFilter = filter;
    }

private:
	size_t numTTIs = 0, numTTIsWithNoActives = 0;

	/**
	 * Requests a scheduling grant for the given connection and list of resources.
	 */
	SchedulingResult request(const MacCid& connectionId, const std::vector<Band>& resources);

	/**
	 * Bypasses the allocator, and instead directly allocates resources to nodes.
	 */
	void allocate(std::map<MacCid, std::vector<Band>> allocationMatrix);
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTESCHEDULERBASE_H_ */