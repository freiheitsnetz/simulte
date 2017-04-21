/*
 * omniscientEntity.h
 */

#ifndef COMMON_OMNISCIENTENTITY_OMNISCIENTENTITY_H_
#define COMMON_OMNISCIENTENTITY_OMNISCIENTENTITY_H_

#include <vector>

#include <omnetpp.h>
#include <LteCommon.h>
#include <LteBinder.h>
#include <LteAmc.h>
#include <LteMacEnb.h>
#include <LteFeedback.h>
// For mobility.
#include <L3AddressResolver.h>
#include <ModuleAccess.h>
#include <IMobility.h>
// For SINR.
#include <LteRealisticChannelModel.h>
#include <LteFeedbackPkt.h>
#include <LteAirFrame.h>
// For feedback computation <-> CQI computation.
#include <LteFeedbackComputationRealistic.h>

#include <SchedulingMemory.h>

class ExposedFeedbackComputer;

/**
 * Implements an omniscient network entity that provides access to the following domains:
 *  Current physical device locations
 *  Current device speed
 *  SINR values for UE-UE and UE-BS links in any direction or power level (periodically saved and current)
 *  Channel Quality Indicators both as reported by the nodes and computed (periodically saved and current)
 */
class OmniscientEntity : public omnetpp::cSimpleModule {
public:
    OmniscientEntity();
    virtual ~OmniscientEntity();

    static OmniscientEntity* get() {
        return SINGLETON;
    }

    /**
     * Applies the Shannon-Hartley theorem C=B*log2(1 + SINR).
     * @param sinr The (Signal / Noise + Interference) ratio.
     * @return The channel capacity = maximum throughput of the channel.
     */
    double getChannelCapacity(const double sinr) const;

    /**
     * @param from Channel starting point.
     * @param to Channel end point.
     * @param band A specific band.
     * @param direction Transmission direction.
     * @param antenna
     * @param transmissionMode
     * @return The current CQI as stored by the AMC.
     */
    Cqi getReportedCqi(const MacNodeId from, const MacNodeId to, const uint band, const Direction direction, const Remote antenna, const TxMode transmissionMode) const;

    /**
     * @param transmissionMode
     * @param sinr
     * @return A CQI computed from SINR and transmission mode.
     */
    Cqi getCqi(const TxMode transmissionMode, const double sinr) const;

    /**
     * @param from Channel starting point.
     * @param to Channel end point.
     * @param time Moment in time.
     * @param transmissionMode Transmitter's transmission mode.
     * @return Channel Quality Indicator as computed right now if time>=NOW, or as stored in memory if earlier.
     */
    Cqi getCqi(const MacNodeId from, const MacNodeId to, const SimTime time, const TxMode transmissionMode) const;

    /**
     * @param from Transmitter ID.
     * @param to Receiver ID.
     * @param time Moment in time.
     * @param transmissionPower The transmitter's transmission power.
     * @param direction The transmission direction.
     * @return If time>=NOW the current value is computed. For earlier moments the memory is queried.
     */
    std::vector<double> getSINROld(MacNodeId from, MacNodeId to, const SimTime time, const double transmissionPower, const Direction direction) const;

    std::vector<double> getSINR(const MacNodeId from, const MacNodeId to, const SimTime time, const double txPower) const;

    /**
     * @param device The device's node ID.
     * @return The IMobility that describes this node's mobility. See inet/src/inet/mobility/contract/IMobility.h for implementation.
     */
    IMobility* getMobility(const MacNodeId& device) const;

    /**
     * @param device The device's node ID.
     * @return The device's physical position. Coord.{x,y,z} are publicly available.
     */
    Coord getPosition(const MacNodeId& device) const;

    /**
     * @param device The device's node ID.
     * @return The device's physical current speed. Coord.{x,y,z} are publicly available.
     */
    Coord getSpeed(const MacNodeId& device) const;

    double getMean(const std::vector<double>& values) const;

    /**
     * @param from
     * @param to
     * @return The transmission direction.
     */
    Direction determineDirection(const MacNodeId& from, const MacNodeId& to) const;

    /**
     * Transmission power is not set correctly in a device's associated UeInfo object.
     * @param device
     * @return The transmission power as set in the .NED for this UE.
     */
    double getTransmissionPower(const MacNodeId& device, Direction dir) const;

    MacNodeId getEnodeBId() const;

    /**
     * If used in conjunction with D2DModeSelectionMaxDatarate mode selection discipline,
     * this OmniscientEntity is made aware of the <from, <to, mode>> map that the mode selection
     * uses. The MaxDatarate scheduler makes use of it to determine D2D channel endpoints.
     */
    const std::map<MacNodeId, std::map<MacNodeId, LteD2DMode>>* getModeSelectionMap() const;
    void setModeSelectionMap(const std::map<MacNodeId, std::map<MacNodeId, LteD2DMode>>* map);

    int getNumberOfBands() const;

    /**
     * If configured to record band assignments,
     * the scheduler can record the scheduling decision made in a round and it'll be collected
     * here and finally printed to a file.
     */
    void recordSchedulingRound(const SchedulingMemory& memory);

protected:

    void initialize() override;

    /**
     * Final configuration at some time point when other network devices are deployed and accessible.
     */
    void configure();

    void handleMessage(cMessage *msg) override;

    /**
     * Takes snapshot of network statistics and puts them into memory.
     */
    void snapshot();

    std::vector<EnbInfo*>* getEnbInfo() const;

    std::vector<UeInfo*>* getUeInfo() const;

    OmnetId getId(const MacNodeId id) const;
    MacNodeId getId(const OmnetId id) const;

    UeInfo* getDeviceInfo(const MacNodeId device) const;

    EnbInfo* getENodeBInfo(MacNodeId id) const;

    ExposedFeedbackComputer* getFeedbackComputation() const;

    /**
     * @param vec Vector holding values.
     * @param name Name for values, e.g. UE_SINR
     * @return e.g. UE_SINR[0]=12 UE_SINR[1]=42
     */
    std::string vectorToString(const std::vector<double>& vec, const std::string& name) const;

    void printSchedulingHistory(const std::string& filename) const;

private:

    static OmniscientEntity* SINGLETON;

    LteBinder   *mBinder = nullptr;
    cMessage    *mSnapshotMsg = nullptr,
                *mConfigMsg = nullptr,
                *mSaveAllocationHistoryMsg = nullptr;
    double      mUpdateInterval,
                mConfigTimepoint,
                mBandwidth;
    LteAmc      *mAmc = nullptr;
    LteRealisticChannelModel    *mChannelModel = nullptr;
    MacNodeId mENodeBId;
    ExposedFeedbackComputer *mFeedbackComputer = nullptr;
    LteDeployer *mDeployer = nullptr;
    Coord mENodeBPosition;
    const std::map<MacNodeId, std::map<MacNodeId, LteD2DMode>>* mModeSelectionMap = nullptr;
    bool mShouldRecordBandAllocation;
    std::vector<SimTime> mBandAllocationTimepoints;
    std::vector<SchedulingMemory> mBandAllocationMemories;

    /**
     * The OmniscientEntity's memory holds information values that update periodically.
     */
    class Memory {
    public:
        Memory(double resolution, double maxSimTime, OmniscientEntity *parent)
                : mResolution(resolution), mMaxSimTime(maxSimTime), mParent(parent),
                  mTimepoints((unsigned int) (maxSimTime / resolution) + 1) {}
        virtual ~Memory() {}

        /**
         * @param time Point in time when the SINR was computed.
         * @param sinr SINR value.
         */
        void put(const SimTime time, const MacNodeId from, const MacNodeId to, const std::vector<double> sinrs) {
            double positionDouble = (time.dbl() / mResolution);
            int position = (int) positionDouble; // Round down.
            if (((size_t) position) >= mTimepoints.size()) {
                std::string err = "OmniscientEntity::Memory::put Position " + std::to_string(position) + " > maxPosition " + std::to_string(mTimepoints.size()) + "\n";
                throw cRuntimeError(err.c_str());
            }
            Timepoint *timepoint = &(mTimepoints.at(position));
            timepoint->put(from, to, sinrs);
        }

        /**
         * @param time Moment in the past you're interested in. Will be rounded to the nearest entry.
         * @param from One channel endpoint.
         * @return A <to, sinrs> map holding the values of all SINRs in all bands to all devices.
         */
        const std::map<MacNodeId, std::vector<double>>* get(const SimTime time, const MacNodeId from) const {
            double position = time.dbl() / mResolution;
            if (position >= mTimepoints.size()) {
                std::string err = "OmniscientEntity::Memory::get Position " + std::to_string(position) + " > maxPosition " + std::to_string(mTimepoints.size()) + "\n";
                throw cRuntimeError(err.c_str());
            }
            const Timepoint* timepoint = &(mTimepoints.at(position));
            const std::map<MacNodeId, std::vector<double>>* map = nullptr;
            try {
                map = timepoint->get(from);
            } catch (const cRuntimeError& err) {
                throw; // Forward exception.
            }

            return map;
        }

        /**
         * @param time Moment in the past you're interested in. Will be rounded to the nearest entry.
         * @param from One channel endpoint.
         * @param to The other channel endpoint.
         * @return SINR values for all bands.
         */
        std::vector<double> get(const SimTime time, const MacNodeId from, const MacNodeId to) const {
            const std::map<MacNodeId, std::vector<double>>* sinrMap = nullptr;
            try {
                sinrMap = Memory::get(time, from);
            } catch (const cRuntimeError& err) {
                throw;
            }
            return sinrMap->at(to);
        }

        /**
         * @return A string representation of all saved values up to the current moment in time.
         */
        std::string toString() {
            std::string description = "";
            std::vector<UeInfo*>* ueInfo = mParent->getUeInfo();
            for (SimTime time(mParent->mConfigTimepoint + mParent->mUpdateInterval); time <= NOW; time += mResolution) {
                description += "Time " + std::to_string(time.dbl()) + ":\n";
                // Go through all channel starting points...
                for (size_t i = 0; i < ueInfo->size(); i++) {
                    MacNodeId from = ueInfo->at(i)->id;
                    // Find statistics to eNodeB.
                    std::vector<double> sinrs_eNodeB;
                    try {
                        sinrs_eNodeB = get(time.dbl(), from, mParent->mENodeBId);
                    } catch (const cRuntimeError& err) {
                        throw cRuntimeError(std::string("OmniscientEntity::Memory::toString() encountered an exception when it tried to fetch a saved entry: " + std::string(err.what())).c_str());
                    }
                    description += "UE[" + std::to_string(from) + "]\n\t\t-> eNodeB CQI=" + std::to_string(mParent->getCqi(TxMode::SINGLE_ANTENNA_PORT0, mParent->getMean(sinrs_eNodeB))) + " " + mParent->vectorToString(sinrs_eNodeB, "SINR") + "\n";
                    // And do the same for all UE end points...
                    for (size_t j = 0; j < ueInfo->size(); j++) {
                        MacNodeId to = ueInfo->at(j)->id;
                        if (from == to)
                            continue;
                        std::vector<double> sinrs;
                        try {
                            sinrs = get(time.dbl(), from, to);
                        } catch (const cRuntimeError& err) {
                            throw cRuntimeError(std::string("OmniscientEntity::Memory::toString() encountered an exception when it tried to fetch a saved entry: " + std::string(err.what())).c_str());
                        }
                        description += "\t\t-> UE[" + std::to_string(to) + "] CQI=" + std::to_string(mParent->getCqi(TxMode::SINGLE_ANTENNA_PORT0, mParent->getMean(sinrs))) + " " + mParent->vectorToString(sinrs, "SINR") + "\n";
                    }
                }
            }
            return description;
        }

    private:
        /** The update resolution. Memory will hold 1 timepoint entry every mResolution simulation time steps. */
        const double mResolution;
        const double mMaxSimTime;
        OmniscientEntity *mParent = nullptr;

        /**
         * For each point in time, keep values for every (device, device) pair.
         */
        class Timepoint {
        public:
            Timepoint() {}
            virtual ~Timepoint() {}

            void put(const MacNodeId from, const MacNodeId to, const std::vector<double> sinrs) {
                std::map<MacNodeId, std::vector<double>>& map = mSinrMap[from];
                map[to] = sinrs;
            }
            /**
             * @return A <to, sinrs> map.
             */
            const std::map<MacNodeId, std::vector<double>>* get(const MacNodeId from) const {
                if (mSinrMap.size() == 0) {
                    throw cRuntimeError(std::string("Memory::Timepoint::get(" + std::to_string(from) + ") called but there's no entries for this timepoint.").c_str());
                }
                const std::map<MacNodeId, std::vector<double>>* map = &(mSinrMap.at(from));
                return map;
            }

            const std::map<MacNodeId, std::map<MacNodeId, std::vector<double>>>* get() const {
                return &(mSinrMap);
            }

        protected:
            /**
             * A map that looks like this: <from, <to, sinrs>>. Only SINRs are kept because CQIs can be computed from them directly.
             */
            std::map<MacNodeId, std::map<MacNodeId, std::vector<double>>> mSinrMap;
        };

        std::vector<Timepoint> mTimepoints;
    };
    OmniscientEntity::Memory *mMemory = nullptr;
};

Define_Module(OmniscientEntity); // Register_Class also works... what's the difference?

#endif /* COMMON_OMNISCIENTENTITY_OMNISCIENTENTITY_H_ */
