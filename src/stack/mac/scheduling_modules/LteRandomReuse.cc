/*
 * LteRandomReuse.cc
 *
 *  Created on: Feb 8, 2018
 *      Author: kunterbunt
 */

#include "LteRandomReuse.h"
#include "common/oracle/Oracle.h"
#include <random>

LteRandomReuse::LteRandomReuse() : LteSchedulerBase() {}

void LteRandomReuse::schedule(std::set<MacCid>& connections) {
	std::map<MacCid, std::vector<Band>> schedulingMap = getSchedulingMap(connections);

	for (ActiveSet::iterator iterator = connections.begin(); iterator != connections.end(); iterator++) {
		MacCid connection = *iterator;
		const std::vector<Band>& resources = schedulingMap[connection];
		for (size_t i = 0; i < resources.size(); i++)
			scheduleUeReuse(connection, resources.at(i));
	}
}

std::map<MacCid, std::vector<Band>> LteRandomReuse::getSchedulingMap(const std::set<MacCid>& connections) {
	std::map<MacCid, std::vector<Band>> map;
	for (ActiveSet::iterator iterator = connections.begin(); iterator != connections.end(); iterator++) {
		MacCid connection = *iterator;
		// Add empty resource list to scheduling map.
		map[connection] = std::vector<Band>();
	}

	std::random_device rd; // Obtain a random number from hardware.
	std::mt19937 eng(rd()); // Use Mersenne-Twister as generator, give it the random number as seed.
	std::uniform_int_distribution<> distr(0, connections.size() - 1); // Define the range.
	for (Band resource = 0; resource < Oracle::get()->getNumRBs(); resource++) {
		unsigned int random = distr(eng);
		map[random].push_back(resource);
	}

	return map;
}
