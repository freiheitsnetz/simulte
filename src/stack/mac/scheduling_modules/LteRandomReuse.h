/*
 * LteRandomReuse.h
 *
 *  Created on: Feb 8, 2018
 *      Author: kunterbunt
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTERANDOMREUSE_H_
#define STACK_MAC_SCHEDULING_MODULES_LTERANDOMREUSE_H_

#include "stack/mac/scheduling_modules/LteSchedulerBase.h"

class LteRandomReuse : public LteSchedulerBase {
public:
	LteRandomReuse();

	virtual void schedule(std::set<MacCid>& connections) override;

protected:
	std::map<MacCid, std::vector<Band>> getSchedulingMap(const std::set<MacCid>& connections);
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTERANDOMREUSE_H_ */
