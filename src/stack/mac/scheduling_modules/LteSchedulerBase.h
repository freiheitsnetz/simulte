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
				<< numTTIsWithNoActives << " had no active connections: numNoActives/numTTI="
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

	/**
	 * Schedule all connections and save decisions to the schedulingDecisions map.
	 */
	virtual void schedule(std::set<MacCid>& connections) = 0;

	virtual void prepareSchedule() override {
		EV << NOW << " LteSchedulerBase::prepareSchedule" << std::endl;
		// Copy currently active connections to a working copy.
		activeConnectionTempSet_ = activeConnectionSet_;
		// Prepare this round's scheduling decision map.
		schedulingDecisions.clear();
		reuseDecisions.clear();
		// Keep track of the number of TTIs.
		numTTIs++;
		if (activeConnectionTempSet_.size() == 0) {
			numTTIsWithNoActives++;
			return;
		} else {
			// Make sure each connection belongs to an existing node in the simulation.
			for (const MacCid& connection : activeConnectionTempSet_) {
				MacNodeId id = MacCidToNodeId(connection);
				if (id == 0 || getBinder()->getOmnetId(id) == 0) {
					activeConnectionTempSet_.erase(connection);
					EV << NOW << " LteSchedulerBase::prepareSchedule Connection " << connection << " of node "
							<< id << " removed from active connection set - appears to have been dynamically removed."
							<< std::endl;
				} else {
					// Instantiate a resource list for each connection.
					schedulingDecisions[connection] = std::vector<Band>();
					reuseDecisions[connection] = std::vector<Band>();
				}
			}
		}
		// Call purely virtual schedule() function.
		schedule(activeConnectionTempSet_);
	}

	virtual void commitSchedule() override {
		EV << NOW << " LteSchedulerBase::commitSchedule" << std::endl;
		activeConnectionSet_ = activeConnectionTempSet_;
		// Request grants for resource allocation.
		for (auto const &item : schedulingDecisions) {
			MacCid connection = item.first;
			std::vector<Band> resources = item.second;
			if (resources.size() > 0)
				request(connection, resources);
		}
		// Bypass the allocator and set resources to be frequency-reused.
		for (auto const &item : reuseDecisions) {
			MacCid connection = item.first;
			std::vector<Band> resources = item.second;
			if (resources.size() > 0)
				requestReuse(connection, resources);
		}
	}

protected:
	size_t numTTIs = 0, numTTIsWithNoActives = 0;
	/** Maps a connection to the list of resources that the schedule() function decided to schedule to it. */
	std::map<MacCid, std::vector<Band>> schedulingDecisions;
	/** Maps a connection to the list of resources that the schedule() function decided to schedule to it for frequency reuse. */
	std::map<MacCid, std::vector<Band>> reuseDecisions;

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
	 * Requests a scheduling grant for the given connection and list of resources.
	 */
	SchedulingResult request(MacCid connectionId, std::vector<Band> resources) {
		bool terminate = false;
		bool active = true;
		bool eligible = true;

		std::vector<BandLimit> bandLimitVec;
		for (const Band& band : resources)
		    bandLimitVec.push_back(BandLimit(band));

		// requestGrant(...) might alter the three bool values, so we can check them afterwards.
		unsigned long max = 4294967295U; // 2^32
		unsigned int granted = requestGrant(connectionId, max, terminate, active, eligible, &bandLimitVec);

        SchedulingResult result;
		if (terminate)
			result = SchedulingResult::TERMINATE;
		else if (!active)
		    result = SchedulingResult::INACTIVE;
		else if (!eligible)
		    result = SchedulingResult::INELIGIBLE;
		else
		    result = SchedulingResult::OK;

		EV << NOW << " LteSchedulerBase::request Scheduled node " << MacCidToNodeId(connectionId) << " on RBs";
        for (const Band& resource : resources)
            EV << " " << resource;
        EV << ": " << schedulingResultToString(result) << std::endl;

        return result;
	}

	/**
	 * Lets the given connection reuse the given resources by bypassing the allocator.
	 */
	void requestReuse(MacCid connectionId, std::vector<Band> resources) {
		// This list'll be filled out.
		std::vector<std::vector<AllocatedRbsPerBandMapA>> allocatedRbsPerBand;
		allocatedRbsPerBand.resize(MAIN_PLANE + 1); // Just the main plane.
		allocatedRbsPerBand.at(MAIN_PLANE).resize(MACRO + 1); // Just the MACRO antenna.
		MacNodeId nodeId = MacCidToNodeId(connectionId);
		// For every resource...
		for (const Band& resource : resources) {
			// ... determine the number of bytes we can serve ...
			Codeword codeword = 0;
			unsigned int numRBs = 1;
			Direction dir;
			if (MacCidToLcid(connectionId) == D2D_MULTI_SHORT_BSR)
				dir = D2D_MULTI;
			else
				dir = (MacCidToLcid(connectionId) == D2D_SHORT_BSR) ? D2D : direction_;
			unsigned int numBytesServed = eNbScheduler_->mac_->getAmc()->computeBytesOnNRbs(nodeId, resource, codeword, numRBs, dir);
			// ... and store the decision in this data structure.
			allocatedRbsPerBand[MAIN_PLANE][MACRO][resource].ueAllocatedRbsMap_[nodeId] += 1;
			allocatedRbsPerBand[MAIN_PLANE][MACRO][resource].ueAllocatedBytesMap_[nodeId] += numBytesServed;
			allocatedRbsPerBand[MAIN_PLANE][MACRO][resource].allocated_ += 1;
		}

		// Tell the scheduler about our decision.
		std::set<Band> occupiedBands = eNbScheduler_->getOccupiedBands();
		eNbScheduler_->storeAllocationEnb(allocatedRbsPerBand, &occupiedBands);
		// Add the nodeId to the scheduleList - needed for grants.
		std::pair<unsigned int, Codeword> scheduleListEntry;
		scheduleListEntry.first = connectionId;
		scheduleListEntry.second = 0; // Codeword.
		eNbScheduler_->storeScListId(scheduleListEntry, resources.size());

		EV << NOW << " LteSchedulerBase::requestReuse Scheduled node " << MacCidToNodeId(connectionId)
				<< " for frequency reuse on RBs" << std::endl;
		for (const Band& resource : resources)
			EV << " " << resource;
		EV << std::endl;
	}
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTESCHEDULERBASE_H_ */
