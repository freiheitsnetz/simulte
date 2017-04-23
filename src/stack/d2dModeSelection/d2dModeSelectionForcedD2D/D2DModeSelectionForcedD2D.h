//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#ifndef STACK_D2DMODESELECTION_D2DMODESELECTIONFORCEDD2D_D2DMODESELECTIONFORCEDD2D_H_
#define STACK_D2DMODESELECTION_D2DMODESELECTIONFORCEDD2D_D2DMODESELECTIONFORCEDD2D_H_

#include "D2DModeSelectionBase.h"
#include "OmniscientEntity.h"

/**
 * Not really a 'selection': D2D mode is forced if a pair is capable of it.
 */
class D2DModeSelectionForcedD2D : public D2DModeSelectionBase {
public:
    D2DModeSelectionForcedD2D() {}
    virtual ~D2DModeSelectionForcedD2D() {}

    virtual void initialize(int stage) override;
protected:
    OmniscientEntity* mOracle = nullptr;

    virtual void doModeSelection();
};

#endif /* STACK_D2DMODESELECTION_D2DMODESELECTIONFORCEDD2D_D2DMODESELECTIONFORCEDD2D_H_ */
