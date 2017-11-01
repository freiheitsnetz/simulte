/*
 * LteReuseTester.h
 *
 *  Created on: Nov 1, 2017
 *      Author: kunterbunt
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTEREUSETESTER_H_
#define STACK_MAC_SCHEDULING_MODULES_LTEREUSETESTER_H_


#include "stack/mac/scheduling_modules/LteSchedulerBase.h"

class LteReuseTester : public virtual LteSchedulerBase {
public:
	virtual void schedule(std::set<MacCid>& connections) override {
		EV << NOW << " LteReuseTester::schedule" << std::endl;
		// Schedule resource 0 to the first connection.
		std::set<MacCid>::const_iterator iterator = connections.begin();
		MacCid originalConnection = *iterator;
		schedulingDecisions[originalConnection].push_back(0);
		// Schedule resource 0 to the second connection for frequency reuse.
		iterator++;
		MacCid reusingConnection = *iterator;
//		reuseDecisions[reusingConnection].push_back(0);
	}
};


#endif /* STACK_MAC_SCHEDULING_MODULES_LTEREUSETESTER_H_ */
