/*
 * LteReuseTester.h
 *
 *  Created on: Nov 1, 2017
 *      Author: kunterbunt
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTEREUSETESTER_H_
#define STACK_MAC_SCHEDULING_MODULES_LTEREUSETESTER_H_


#include "stack/mac/scheduling_modules/LteSchedulerBase.h"
#include "common/oracle/Oracle.h"

using namespace std;

class LteReuseTester : public LteSchedulerBase {
public:
	virtual void schedule(std::set<MacCid>& connections) override {
		EV << NOW << " LteReuseTester::schedule" << std::endl;
		for (std::set<MacCid>::const_iterator iterator = connections.begin(); iterator != connections.end(); iterator++) {
			MacCid connection = *iterator;



			for (Band band = 0; band < Oracle::get()->getNumRBs(); band++) {
				scheduleUeReuse(connection, band);
			}
		}
	}
};


#endif /* STACK_MAC_SCHEDULING_MODULES_LTEREUSETESTER_H_ */
