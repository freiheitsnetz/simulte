/*
 * LteModdedStackelebrgGame.h
 *
 *  Created on: Feb 13, 2018
 *      Author: seba
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTESTACKELBERGGAME_LTEMODDEDSTACKELBERGGAME_H_
#define STACK_MAC_SCHEDULING_MODULES_LTESTACKELBERGGAME_LTEMODDEDSTACKELBERGGAME_H_

#include "stack/mac/scheduling_modules/LteStackelbergGame/LteStackelbergGame.h"
#include "common/oracle/Oracle.h"

class LteModdedStackelbergGame : public LteStackelbergGame {
public:
    LteModdedStackelbergGame();
    virtual ~LteModdedStackelbergGame();

    virtual void schedule(std::set<MacCid>& connections) override;

protected:
    /** This function is passed to the modified Stackelberg game so that it can calculate newly needed channel gains. */
    std::function<double(unsigned short, unsigned short)> getChannelGain = [&](unsigned short id1, unsigned short id2) {return Oracle::get()->getChannelGain(id1, id2);};

    unsigned long numCellD2DGames = 0, numD2DCellGames = 0, numD2DD2DGames = 0;
};

#endif /* STACK_MAC_SCHEDULING_MODULES_LTESTACKELBERGGAME_LTEMODDEDSTACKELBERGGAME_H_ */
