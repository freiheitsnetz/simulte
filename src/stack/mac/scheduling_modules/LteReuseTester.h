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

		vector<User*> users = userManager.getUsers();
		MacNodeId senderId = 1025, receiverId = 1027;
		vector<double> interferenceVec = Oracle::get()->getInCellInterference(senderId, receiverId);
		double intMean = 0.0;
		for (size_t i = 0; i < interferenceVec.size(); i++)
		    intMean  += interferenceVec.at(i);
		if (intMean > 0.0) {
            intMean /= interferenceVec.size();
            totalIntMean += intMean;
            intsCollected++;
            Oracle::get()->scalarDouble("Interference", totalIntMean / ((double) intsCollected));
//            cout << "int(" << Oracle::get()->getName(senderId) << ", " << Oracle::get()->getName(receiverId) << ") = " << totalIntMean / intsCollected << endl;
		}

		vector<double> sinrVec = Oracle::get()->getInCellInterference(senderId, receiverId);
        double sinrMean = 0.0;
        for (size_t i = 0; i < sinrVec.size(); i++)
            sinrMean  += sinrVec.at(i);
        if (sinrMean > 0.0) {
            sinrMean /= sinrVec.size();
            totalSinrMean += intMean;
            sinrsCollected++;
            Oracle::get()->scalarDouble("SINR", totalSinrMean / ((double) sinrsCollected));
            cout << "sinr(" << Oracle::get()->getName(senderId) << ", " << Oracle::get()->getName(receiverId) << ") = " << totalSinrMean / sinrsCollected << endl;
        }
	}

	double totalIntMean = 0, totalSinrMean = 0;
	unsigned long intsCollected = 0, sinrsCollected = 0;
};


#endif /* STACK_MAC_SCHEDULING_MODULES_LTEREUSETESTER_H_ */
