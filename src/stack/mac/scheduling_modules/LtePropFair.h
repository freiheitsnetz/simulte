/*
 * LtePropFair.h
 *
 *  Created on: Jan 27, 2018
 *      Author: kunterbunt
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTEPROPFAIR_H_
#define STACK_MAC_SCHEDULING_MODULES_LTEPROPFAIR_H_

#include "stack/mac/scheduling_modules/LteSchedulerBase.h"
#include "stack/mac/scheduling_modules/LteTUGame/src/TUGameUser.h"

/**
 * Proportional Fair scheduler implementation.
 * In comparison to LtePf this implementation takes into account the actual bytes a user could send out,
 * maintaining realistic mean datarate values for all users, instead of the fraction of all RBs a user is allocated in one TTI.
 */
class LtePropFair : public virtual LteSchedulerBase {
public:
	LtePropFair();
	virtual ~LtePropFair();

	virtual void schedule(std::set<MacCid>& connections) override;

	virtual void commitSchedule();

protected:
	std::vector<TUGameUser*> users;
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTEPROPFAIR_H_ */
