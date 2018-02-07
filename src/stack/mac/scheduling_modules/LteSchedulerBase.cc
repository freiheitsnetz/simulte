#include "stack/mac/scheduling_modules/LteSchedulerBase.h"
#include "stack/mac/layer/LteMacBase.h"
#include "stack/mac/buffer/LteMacQueue.h"

using namespace std;

LteSchedulerBase::SchedulingResult LteSchedulerBase::request(const MacCid& connectionId, const std::vector<Band>& resources) {
	bool terminate = false;
	bool active = true;
	bool eligible = true;

	std::vector<BandLimit> bandLimitVec;
	for (const Band& band : resources) {
		bandLimitVec.emplace_back(BandLimit(band));
	}

	// requestGrant(...) might alter the three bool values, so we can check them afterwards.
	unsigned long max = 4294967295U; // 2^32
	numBytesGrantedLast = requestGrant(connectionId, max, terminate, active, eligible, &bandLimitVec);

	LteSchedulerBase::SchedulingResult result;
	if (terminate)
		result = LteSchedulerBase::SchedulingResult::TERMINATE;
	else if (!active)
		result = LteSchedulerBase::SchedulingResult::INACTIVE;
	else if (!eligible)
		result = LteSchedulerBase::SchedulingResult::INELIGIBLE;
	else
		result = LteSchedulerBase::SchedulingResult::OK;

	EV << NOW << " " << dirToA(direction_) << " LteSchedulerBase::request RB allocation of node " << Oracle::get()->getName(MacCidToNodeId(connectionId)) << " ->";
	for (const Band& resource : resources)
	    EV << " " << resource;
	EV << " = " << schedulingResultToString(result) << std::endl;

	return result;
}

void LteSchedulerBase::allocate(std::map<MacCid, std::vector<Band>> allocationMatrix) {
	// This list'll be filled out.
	std::vector<std::vector<AllocatedRbsPerBandMapA>> allocatedRbsPerBand;
	allocatedRbsPerBand.resize(MAIN_PLANE + 1); // Just the main plane.
	allocatedRbsPerBand.at(MAIN_PLANE).resize(MACRO + 1); // Just the MACRO antenna.
	for (const auto& allocationItem : allocationMatrix) {
		const MacCid connectionId = allocationItem.first;
		const MacNodeId nodeId = MacCidToNodeId(connectionId);
		const std::vector<Band>& resources = allocationItem.second;
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

			unsigned int numBytesServed = getBytesOnBand(nodeId, resource, numRBs, dir);
			/*eNbScheduler_->mac_->getAmc()->computeBytesOnNRbs(nodeId, resource, codeword, numRBs, dir);*/
			// ... and store the decision in this data structure.
			allocatedRbsPerBand[MAIN_PLANE][MACRO][resource].ueAllocatedRbsMap_[nodeId] += 1;
			allocatedRbsPerBand[MAIN_PLANE][MACRO][resource].ueAllocatedBytesMap_[nodeId] += numBytesServed;
			allocatedRbsPerBand[MAIN_PLANE][MACRO][resource].allocated_ += 1;
		}

		// Add the nodeId to the scheduleList - needed for grants.
		std::pair<unsigned int, Codeword> scheduleListEntry;
		scheduleListEntry.first = connectionId;
		scheduleListEntry.second = 0; // Codeword.
		eNbScheduler_->storeScListId(scheduleListEntry, resources.size());

		EV << NOW << " LteSchedulerBase::requestReuse Scheduled node " << Oracle::get()->getName(MacCidToNodeId(connectionId))
		   << " for frequency reuse on RBs";
		for (const Band& resource : resources)
		    EV << " " << resource;
		EV << std::endl;
	}
	// Tell the scheduler about our decision.
	eNbScheduler_->storeAllocationEnb(allocatedRbsPerBand, NULL);
}

UserManager& LteSchedulerBase::getUserManager() {
    return this->userManager;
}

LteSchedulerBase::~LteSchedulerBase() {
	std::cout << dirToA(direction_) << ": " << numTTIs << " TTIs, of which "
		<< numTTIsWithNoActives << " had no active connections: " << numTTIsWithNoActives << "/" << numTTIs << "="
		<< ((double) numTTIsWithNoActives / (double) numTTIs) << std::endl;
}

void LteSchedulerBase::prepareSchedule() {
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
		for (ActiveSet::iterator iterator = activeConnectionTempSet_.begin(); iterator != activeConnectionTempSet_.end();) {
			const MacCid& connection = *iterator;
			MacNodeId id = MacCidToNodeId(connection);

			// Make sure it's not been dynamically removed.
			if (id == 0 || getBinder()->getOmnetId(id) == 0) {
				EV << NOW << " LteSchedulerBase::prepareSchedule Connection " << connection << " of node "
						<< id << " removed from active connection set - appears to have been dynamically removed."
						<< std::endl;
				iterator = activeConnectionTempSet_.erase(iterator);
				continue;
			}

			// Make sure it's active.
			LteMacBuffer* bsrBuffer = direction_ == UL ? eNbScheduler_->bsrbuf_->at(connection) : eNbScheduler_->vbuf_->at(connection);
			if (bsrBuffer->isEmpty()) {
				EV << NOW << " LteSchedulerBase::prepareSchedule Connection " << connection << " of node "
						<< id << " removed from active connection set - no longer active as BSR buffer at eNB is empty."
						<< std::endl;
				iterator = activeConnectionTempSet_.erase(iterator);
				continue;
			}
			// Make sure CQI>0.
			Direction dir;
			if (direction_ == UL)
				dir = (MacCidToLcid(connection) == D2D_SHORT_BSR) ? D2D : (MacCidToLcid(connection) == D2D_MULTI_SHORT_BSR) ? D2D_MULTI : direction_;
			else
				dir = DL;
			const UserTxParams& info = eNbScheduler_->mac_->getAmc()->computeTxParams(id, dir);
			const std::set<Band>& bands = info.readBands();
			unsigned int codewords = info.getLayers().size();
			bool cqiNull = false;
			for (unsigned int i=0; i < codewords; i++) {
				if (info.readCqiVector()[i] == 0)
					cqiNull = true;
			}
			if (cqiNull) {
				EV << NOW << " LteSchedulerBase::prepareSchedule Connection " << connection << " of node "
						<< id << " removed from active connection set - CQI is zero."
						<< std::endl;
				iterator = activeConnectionTempSet_.erase(iterator);
				continue;
			}
			iterator++; // Only increment if we haven't erased an item. Otherwise the erase() returns the next valid position.
		}
	}

	// Check again, might have deleted all connections.
	if (activeConnectionTempSet_.size() == 0) {
		numTTIsWithNoActives++;
		return;
	}

	// All connections that still remain can actually be scheduled.
	for (const MacCid& connection : activeConnectionTempSet_) {
		// Instantiate a resource list for each connection.
		schedulingDecisions[connection] = std::vector<Band>();
		reuseDecisions[connection] = std::vector<Band>();
	}

	// Update active user list.
	userManager.update(activeConnectionTempSet_, LteSchedulerBase::setD2D, userFilter, [](unsigned short id) {
	    try {
	        MacNodeId partnerId = Oracle::get()->getTransmissionPartner(id);
	        cout << "User Manager found partner for '" << Oracle::get()->getName(id) << "': '" << Oracle::get()->getName(partnerId) << "'." << endl;
	        return partnerId;
	    } catch (const exception& e) {
	        cerr << "User Manager couldn't find transmission partner for " << Oracle::get()->getName(id) << ": '" << e.what() << "'." << endl;
	        return User::PARTNER_UNSET;
	    }
	});

	// Call respective schedule() function implementation.
	schedule(activeConnectionTempSet_);
}

void LteSchedulerBase::commitSchedule() {
	EV << NOW << " LteSchedulerBase::commitSchedule" << std::endl;
	activeConnectionSet_ = activeConnectionTempSet_;
	// Request grants for resource allocation.
	for (const auto& item : schedulingDecisions) {
		MacCid connection = item.first;
		if (MacCidToNodeId(connection) == 0)
			continue;
		std::vector<Band> resources = item.second;
		if (!resources.empty()) {
			SchedulingResult result = request(connection, resources);
			reactToSchedulingResult(result, numBytesGrantedLast, connection);
		}
	}
	// Bypass the allocator and set resources to be frequency-reused.
	bool anythingToAllocate = false;
	for (std::map<MacCid, std::vector<Band>>::iterator it = reuseDecisions.begin(); it != reuseDecisions.end(); it++) {
		if (MacCidToNodeId((*it).first) == 0) {
			reuseDecisions.erase(it);
			it--;
		} else {
			if (!(*it).second.empty())
				anythingToAllocate = true;
		}
	}


	if (anythingToAllocate)
		allocate(reuseDecisions);
}

Direction LteSchedulerBase::getDirection(const MacCid& connection) {
	Direction dir;
	if (direction_ == UL)
		dir = (MacCidToLcid(connection) == D2D_SHORT_BSR) ? D2D : (MacCidToLcid(connection) == D2D_MULTI_SHORT_BSR) ? D2D_MULTI : UL;
	else
		dir = DL;
	return dir;
}

unsigned int LteSchedulerBase::getBytesOnBand(const MacNodeId& nodeId, const Band& band, const unsigned int& numBlocks, const Direction& dir) {
	return eNbScheduler_->mac_->getAmc()->computeBytesOnNRbs(nodeId, band, numBlocks, dir);
}

unsigned int LteSchedulerBase::getAverageBytesPerBlock(const MacCid& connection) {
	MacNodeId nodeId = MacCidToNodeId(connection);

	unsigned int totalAvailableRBs = 0,
				 availableBytes = 0;

	// Determine direction.
	Direction dir = getDirection(connection);

	// For each antenna...
	const UserTxParams& info = eNbScheduler_->mac_->getAmc()->computeTxParams(nodeId, dir);
	for (std::set<Remote>::iterator antennaIt = info.readAntennaSet().begin(); antennaIt != info.readAntennaSet().end(); antennaIt++) {
		// For each resource...
		for (Band resource = 0; resource != Oracle::get()->getNumRBs(); resource++) {
			// Determine number of RBs.
			unsigned int availableRBs = eNbScheduler_->readAvailableRbs(nodeId, *antennaIt, resource);
			totalAvailableRBs += availableRBs;
			if (availableRBs > 1)
				cerr << NOW << " LteTUGame::getAverageBytesPerBlock(" << nodeId << ") with availableRBs==" << availableRBs << "!" << endl;
			// Determine number of bytes on this 'logical band' (which is a resource block if availableRBs==1).
			availableBytes += getBytesOnBand(nodeId, resource, availableRBs, dir);
		}
	}

	// Average number of bytes available on 1 RB.
	return (totalAvailableRBs > 0) ? (availableBytes / totalAvailableRBs) : 0;
}


unsigned int LteSchedulerBase::getTotalDemand(const MacCid& connection) {
	LteMacBuffer* bsrBuffer = direction_ == UL ? eNbScheduler_->bsrbuf_->at(connection) : eNbScheduler_->vbuf_->at(connection);
	unsigned int bytesInBsrBuffer = bsrBuffer->getQueueOccupancy();
	return bytesInBsrBuffer;
}

unsigned int LteSchedulerBase::getCurrentDemand(const MacCid& connection) {
	LteMacBuffer* bsrBuffer = direction_ == UL ? eNbScheduler_->bsrbuf_->at(connection) : eNbScheduler_->vbuf_->at(connection);
	return bsrBuffer->front().first;
}

double LteSchedulerBase::getRBDemand(const MacCid& connection, const unsigned int& numBytes) {
	double bytesPerBlock = (double) getAverageBytesPerBlock(connection);
	return bytesPerBlock > 0 ? ((double) numBytes) / bytesPerBlock : 0.0;
}
