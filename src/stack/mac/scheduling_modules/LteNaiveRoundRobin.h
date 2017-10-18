/*
 * LteNaiveRoundRobin.h
 *
 *  Created on: Oct 18, 2017
 *      Author: kunterbunt
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTENAIVEROUNDROBIN_H_
#define STACK_MAC_SCHEDULING_MODULES_LTENAIVEROUNDROBIN_H_

#include <omnetpp.h>
#include "stack/mac/scheduling_modules/LteSchedulerBase.h"
#include "common/LteCommon.h"

/**
 * A pure (non-deficit-)round-robin scheduler implementation: the available resources are distributed on a one-resource-per-user basis.
 */
class LteNaiveRoundRobin : public virtual LteSchedulerBase {
public:
	virtual void prepareSchedule() override;

	virtual void commitSchedule() override;

protected:
	MacNodeId lastScheduledID = 0;
	MacNodeId maxId = 0;
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTENAIVEROUNDROBIN_H_ */
