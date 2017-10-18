/*
 * LteNaiveRoundRobin.cc
 *
 *  Created on: Oct 18, 2017
 *      Author: kunterbunt
 */

#include "LteNaiveRoundRobin.h"

LteNaiveRoundRobin::LteNaiveRoundRobin() {
}

LteNaiveRoundRobin::~LteNaiveRoundRobin() {
	std::cout << dirToA(direction_) << ": " << numTTIs << " TTIs, of which " << numTTIsWithNoActives << " had no active connections: " << ((double) numTTIsWithNoActives / (double) numTTIs) << std::endl;
}

void LteNaiveRoundRobin::prepareSchedule() {
	if (maxId == 0) {
		std::vector<UeInfo*>* ueList = getBinder()->getUeList();
		for (size_t i = 0; i < ueList->size(); i++) {
			if (std::string("ueD2DTx").compare(ueList->at(i)->ue->getName()) == 0) {
				MacNodeId id = ueList->at(i)->id;
				if (id > maxId)
					maxId = id;
			}
		}
	}

	// Copy currently active connections to a working copy.
	activeConnectionTempSet_ = activeConnectionSet_;
	EV << NOW << " LteNaiveRoundRobin::prepareSchedule" << std::endl;
	numTTIs++;
	if (activeConnectionTempSet_.size() == 0) {
		numTTIsWithNoActives++;
		return;
	}

	if (lastScheduledID == 0)
		lastScheduledID = MacCidToNodeId(*activeConnectionTempSet_.begin());

	for (Band band = 0; band < getBinder()->getNumBands(); band++) {
		MacCid connection;
		MacNodeId id;
		for (ActiveSet::iterator iterator = activeConnectionTempSet_.begin(); iterator != activeConnectionTempSet_.end(); iterator++) {
			connection = *iterator;
			id = MacCidToNodeId(connection);
			if (lastScheduledID < maxId) {
				if (id <= lastScheduledID)
					continue;
				else
					break;
			} else {
				if (id >= lastScheduledID)
					continue;
				else
					break;
			}
		}
		SchedulingResult result = schedule(connection, band);
		EV << NOW << " LteNaiveRoundRobin::prepareSchedule Scheduled node " << id <<
				" on RB " << band << ": " << schedulingResultToString(result) << std::endl;
		lastScheduledID = id;

//		std::cout << lastScheduledID << std::endl;
	}
}

void LteNaiveRoundRobin::commitSchedule() {
	EV_STATICCONTEXT;
	EV << NOW << " LteNaiveRoundRobin::commitSchedule" << std::endl;
	activeConnectionSet_ = activeConnectionTempSet_;
}
