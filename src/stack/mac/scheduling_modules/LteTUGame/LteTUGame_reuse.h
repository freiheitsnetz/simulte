/*
 * LteTUGame_reuse.h
 *
 *  Created on: Feb 12, 2018
 *      Author: seba
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_LTETUGAME_LTETUGAME_REUSE_H_
#define STACK_MAC_SCHEDULING_MODULES_LTETUGAME_LTETUGAME_REUSE_H_

#include "stack/mac/scheduling_modules/LteTUGame/LteTUGame.h"

/**
 * Extends Transferable Utility game by randomly picking users to reuse another user's frequency resource.
 */
class LteTUGame_reuse : public LteTUGame {

public:
    LteTUGame_reuse() : LteTUGame() {}

    virtual void schedule(std::set<MacCid>& connections) override {
        EV << NOW << " LteTUGame_reuse::schedule" << std::endl;

        std::map<unsigned short, const TUGameUser*> schedulingMap_firstUse = getSchedulingMap(connections);

        if (!schedulingMap_firstUse.empty()) {
            for (Band resource = 0; resource < Oracle::get()->getNumRBs(); resource++) {
                scheduleUeReuse(schedulingMap_firstUse[resource]->getConnectionId(), resource);
            }
        }

        // Now randomly find users to share frequency resources.
        std::map<MacCid, std::vector<Band>> convertedMap;
        std::map<MacCid, std::vector<Band>> schedulingMap_reuse;
        if (!schedulingMap_firstUse.empty()) {
            for (Band rb = 0; rb < Oracle::get()->getNumRBs(); rb++) {
                convertedMap[schedulingMap_firstUse[rb]->getConnectionId()].push_back(rb);
            }

            schedulingMap_reuse = getSchedulingMap_reuse(connections, convertedMap);
        }


        for (ActiveSet::iterator iterator = connections.begin(); iterator != connections.end(); iterator++) {
            MacCid connection = *iterator;
            const std::vector<Band>& resources = convertedMap[connection];
            if (!resources.empty()) {
//              cout << NOW << " Schedule ";
                for (const Band& resource : resources) {
                    scheduleUeReuse(connection, resource);
//                  cout << resource << " ";
                }
//              cout << "to node " << MacCidToNodeId(connection) << endl;
            }

            const std::vector<Band>& resources_reuse = schedulingMap_reuse[connection];
            if (!resources_reuse.empty()) {
//              cout << NOW << " Reuse ";
                for (const Band& resource_reuse : resources_reuse) {
                    scheduleUeReuse(connection, resource_reuse);
//                  cout << resource_reuse << " ";
                }
//              cout << "to node " << MacCidToNodeId(connection) << endl;
            }
        }

    }

protected:
    std::map<MacCid, std::vector<Band>> getSchedulingMap_reuse(const std::set<MacCid>& connections, const std::map<MacCid, std::vector<Band>>& schedulingMap) {
        std::map<MacCid, std::vector<Band>> map;

        // No chance of finding a UE that hasn't been scheduled a resource if there's only one.
        if (connections.size() <= 1)
            return map;

        vector<MacCid> cids;
        for (std::set<MacCid>::const_iterator it = connections.begin(); it != connections.end(); it++) {
            MacCid connection = (*it);
            cids.push_back(connection);
            map[connection] = std::vector<Band>();
        }

        std::random_device rd; // Obtain a random number from hardware.
        std::mt19937 eng(rd()); // Use Mersenne-Twister as generator, give it the random number as seed.
        std::uniform_int_distribution<> distr(0, connections.size() - 1); // Define the range.
        for (Band resource = 0; resource < Oracle::get()->getNumRBs(); resource++) {
            bool alreadyScheduled = true;
            unsigned int random;
            MacCid cid;
            do {
                random = distr(eng);
                // Make sure we've not randomly picked that UE that was already scheduled this resource.
                cid = cids.at(random);
                if (schedulingMap.find(cid) == schedulingMap.end()) {
                    break;
                }
                const std::vector<Band>& resources = schedulingMap.at(cid);

                bool found = false;
                for (const Band& scheduled_resource : resources) {
                    if (scheduled_resource == resource) {
                        found = true;
                        break;
                    }
                }
                if (found)
                    continue;
                else
                    alreadyScheduled = false;
            } while (alreadyScheduled);

            map[cid].push_back(resource);
        }

        return map;
    }
};


#endif /* STACK_MAC_SCHEDULING_MODULES_LTETUGAME_LTETUGAME_REUSE_H_ */
