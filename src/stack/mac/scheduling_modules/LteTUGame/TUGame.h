/*
 * TUGame.h
 *
 *  Created on: Dec 19, 2017
 *      Author: seba
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTETUGAME_TUGAME_H_
#define STACK_MAC_SCHEDULING_MODULES_LTETUGAME_TUGAME_H_

/**
 *
 * Implements the Transferable Utility Game components required for the application of Shapley's value.
 *
 */

#include "stack/mac/scheduling_modules/LteTUGame/shapley.h"

class TUGamePlayer : public Shapley::Player {
public:
    explicit TUGamePlayer(double contribution) : value(contribution) {}

    double getContribution() const override {
        return value;
    }

protected:
    double value;
};



#endif /* STACK_MAC_SCHEDULING_MODULES_LTETUGAME_TUGAME_H_ */
