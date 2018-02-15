/*
 * Oracle.h
 *
 *  Created on: Jul 26, 2017
 *      Author: kunterbunt
 */

#ifndef COMMON_ORACLE_ORACLE_H_
#define COMMON_ORACLE_ORACLE_H_

#include <vector>
#include <iostream>

#include <omnetpp.h>
#include <common/LteCommon.h>
#include "stack/mac/allocator/LteAllocatorUtils.h"
#include "stack/phy/feedback/LteFeedbackComputation.h"

/**
 * Implements an omniscient network entity.
 */
class Oracle : public omnetpp::cSimpleModule {
public:
    Oracle() : configMessage("Oracle::config") {
        Oracle::SINGLETON = this;
    }
    virtual ~Oracle() {
        std::cout << "oracle destr" << std::endl;
    }

    static Oracle* get() {
        return SINGLETON;
    }

    /**
     * @return Maximum simulation time.
     */
    double getMaxSimTime() const {
        return maxSimTime;
    }

    MacNodeId getEnodeBID() const;

    /**
     * @return The number of resources configured in the system.
     */
    size_t getNumRBs() const;

    /**
     * @param from
     * @param to
     * @return The transmission direction.
     */
    Direction determineDirection(const MacNodeId from, const MacNodeId to) const;

    /**
     * @return Pointer to the LtePhyBase of the specified node.
     */
    LtePhyBase* getPhyBase(const MacNodeId id) const;

    const UeInfo* getUeInfo(const MacNodeId& id) const;

    std::string getApplicationName(const MacNodeId& id) const;

    /**
     * @return The node's physical position.
     */
    inet::Coord getPosition(const MacNodeId id) const;

    std::string getName(const MacNodeId id) const;

    double getDistance(inet::Coord from, inet::Coord to) const;

    /**
     * @return The node's transmission power in the specified direction in dB.
     */
    double getTxPower(const MacNodeId id, Direction dir) const;

    /**
     * @return Signal-to-Interference+Noise-Ratio per resource.
     */
    std::vector<double> getSINR(const MacNodeId from, const MacNodeId to) const;

    /**
     * @return Path attenuation between 'from' and 'to'.
     */
    double getAttenuation(const MacNodeId from, const MacNodeId to) const;

    /**
     * @param considerThisTTI Whether to consider the current TTI's resource allocation, or the previous one.
     * @return Received signal of interfering nodes on each resource.
     */
    std::vector<double> getInCellInterference(const MacNodeId from, const MacNodeId to, bool considerThisTTI) const;

    Cqi getCQI(const MacNodeId from, const MacNodeId to) const;

    void printAllocation(std::vector<std::vector<AllocatedRbsPerBandMapA>>& allocatedRbsPerBand);

    void setFeedbackComputer(LteFeedbackComputation* computer) {
    	this->feedbackComputer = computer;
    }

    bool isD2DFlow(const MacCid& nodeId);

    double getD2DPenalty() const;

    /**
     * @return <max, min>
     */
    std::pair<double, double> stackelberg_getD2DPowerLimits() const;
    double stackelberg_getBeta() const;
    double stackelberg_getDelta() const;
    std::string stackelberg_getLeaderScheduler() const;
    bool stackelberg_shouldSetTxPower() const;

    /**
     * @return Linear channel gain where fading is averaged over all resource blocks in 'resources'.
     */
    double getChannelGain(MacNodeId from, MacNodeId to, std::vector<Band> resources) const;

    /**
     * @return Linear channel gain where fading is averaged over all resource blocks.
     */
    double getChannelGain(MacNodeId from, MacNodeId to) const;

    /**
     * @return The ID of that UE that is the transmission partner for 'id'.
     */
    MacNodeId getTransmissionPartner(const MacNodeId id) const;

//    unsigned int getAverageBytesPerBlock(const MacCid& connection) const;

    /**
     * @param d2d Whether to set the D2D transmission power or not.
     */
    void setUETxPower(MacNodeId id, bool d2d, double power_dBm);

protected:
    double maxSimTime = 0.0;

    void initialize() override;

    /**
     * Final configuration at some time point when other network devices are deployed and accessible.
     */
    void configure();

    void handleMessage(cMessage *msg) override;


private:
    static Oracle* SINGLETON;
    cMessage configMessage;
    LteFeedbackComputation* feedbackComputer = nullptr;

};

Define_Module(Oracle); // Register_Class also works... what's the difference?

#endif /* COMMON_ORACLE_ORACLE_H_ */
