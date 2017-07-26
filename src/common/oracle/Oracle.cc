#include "common/oracle/Oracle.h"
#include <stack/phy/layer/LtePhyBase.h>

/**
 * Derived class exposes otherwise hidden deployer_ member pointer.
 */
class ExposedLteMacEnb : public LteMacEnb {
public:
    LteDeployer* getDeployer() {
        return LteMacEnb::deployer_;
    }
};

/**
 * Derived class exposes otherwise hidden CQI computation.
 */
class ExposedFeedbackComputer : public LteFeedbackComputationRealistic {
public:
    Cqi getCqi(TxMode txmode, double snr) {
        return LteFeedbackComputationRealistic::getCqi(txmode, snr);
    }
};

Oracle* Oracle::SINGLETON = nullptr;

Oracle::Oracle()
    : mBinder(getBinder()),
      mSnapshotMsg(new cMessage("Oracle::snapshot")),
      mConfigMsg(new cMessage("Oracle::config")),
      mSaveAllocationHistoryMsg(new cMessage("Oracle::saveAllocationHistory")),
      mUpdateInterval(0.01) // Will be properly set to provided .NED value in initialize()
    {
    Oracle::SINGLETON = this;
    }

Oracle::~Oracle() {
    if (mMemory != nullptr)
        delete mMemory;
    if (mFeedbackComputer != nullptr)
        delete mFeedbackComputer;
    if (!mConfigMsg->isScheduled())
        delete mConfigMsg;
    if (!mSnapshotMsg->isScheduled())
        delete mSnapshotMsg;
    if (!mSaveAllocationHistoryMsg->isScheduled())
        delete mSaveAllocationHistoryMsg;
}

double Oracle::getChannelCapacity(const double sinr) const {
    // Apply Shannon-Hartley theorem.
    double capacity = mBandwidth * log2(1 + sinr);
    return capacity;
}

Cqi Oracle::getReportedCqi(const MacNodeId from, const MacNodeId to, const uint band, const Direction direction, const Remote antenna, const TxMode transmissionMode) const {
    if (from == to)
        throw cRuntimeError(std::string("Oracle::getReportedCqi shouldn't be called for from==to!").c_str());
    if (mAmc == nullptr)
        throw cRuntimeError(std::string("Oracle::getCqi called before the AMC was registered. You should call this method after final configuration is done.").c_str());

    // Cellular link.
    if (from == mENodeBId) {
        return mAmc->getFeedback(to, antenna, transmissionMode, direction).getCqi(0, band);
    } else if (to == mENodeBId) {
        return mAmc->getFeedback(from, antenna, transmissionMode, direction).getCqi(0, band);
    // D2D link.
    } else {
        return mAmc->getFeedbackD2D(from, antenna, transmissionMode, to).getCqi(0, band);
    }
}

Cqi Oracle::getCqi(const TxMode transmissionMode, const double sinr) const {
    return mFeedbackComputer->getCqi(transmissionMode, sinr);
}

Cqi Oracle::getCqi(const MacNodeId from, const MacNodeId to, const SimTime time, const TxMode transmissionMode) const {
    if (from == to)
        throw cRuntimeError(std::string("Oracle::getCqi shouldn't be called for from==to!").c_str());
    // getSINR handles distinction of computing a current value or querying the memory.
    double txPower = getTransmissionPower(from, determineDirection(from, to));
    double meanSINR = getMean(getSINR(from, to, time, txPower));
    return getCqi(transmissionMode, meanSINR);
}


std::vector<double> Oracle::getSINR(const MacNodeId from, const MacNodeId to, const SimTime time, const double txPower) const {
    if (from == to)
        throw cRuntimeError(std::string("Oracle::getSINR shouldn't be called for from==to!").c_str());

    if (time >= NOW) {
        EV << "Oracle::getSINR computes current value." << std::endl;
        Direction direction = determineDirection(from, to);
        LteAirFrame frame("feedback_pkt");
        UserControlInfo uinfo;
        uinfo.setFrameType(FEEDBACKPKT);
        uinfo.setIsCorruptible(false);
        uinfo.setCoord(getPosition(from));

        uinfo.setSourceId(from);
        uinfo.setD2dTxPeerId(from);
        uinfo.setDestId(to);
        uinfo.setD2dRxPeerId(to);

        uinfo.setDirection(direction);
        uinfo.setTxPower(txPower);
        uinfo.setD2dTxPower(txPower);
        return mChannelModel->getSINR_D2D(&frame, &uinfo, to, getPosition(to), mENodeBId);
    } else {
        EV << "Oracle::getSINR fetches value from memory." << std::endl;
        return mMemory->get(time, from, to);
    }
}

std::vector<double> Oracle::getSINROld(MacNodeId from, MacNodeId to, const SimTime time, const double transmissionPower, const Direction direction) const {
    if (from == to)
        throw cRuntimeError(std::string("Oracle::getSINR shouldn't be called for from==to!").c_str());
    // Make sure eNodeB is the target.
//    if (from == mENodeBId) {
//        MacNodeId temp = from;
//        from = to;
//        to = temp;
//    }
    // Compute current value.
    if (time >= NOW) {
        EV << "Oracle::getSINR computes current value." << std::endl;
        LteAirFrame* frame = new LteAirFrame("feedback_pkt");
        UserControlInfo* uinfo = new UserControlInfo();
        uinfo->setFrameType(FEEDBACKPKT);
        uinfo->setIsCorruptible(false);
        uinfo->setSourceId(from);
        uinfo->setDirection(direction);
        uinfo->setTxPower(transmissionPower);
        uinfo->setDestId(to);
        std::vector<double> SINRs;

        // UE <-> eNodeB cellular channel.
        if (to == mENodeBId) {
            // I am pretty sure that there's a mistake in the simuLTE source code.
            // It incorrectly sets the eNodeB's position for its calculations. I am here setting the UE's position as the eNodeB's,
            // so that the distance is correct which is all that matters. (or maybe this is how you're supposed to do it...)
//            uinfo->setCoord(mENodeBPosition);
            uinfo->setCoord(getPosition(from));


            SINRs = mChannelModel->getSINR(frame, uinfo);
        // UE <-> UE D2D channel.
        } else {
            uinfo->setCoord(getPosition(from));
            uinfo->setD2dTxPeerId(from);
            uinfo->setD2dRxPeerId(to);
            uinfo->setD2dTxPower(transmissionPower);

            SINRs = mChannelModel->getSINR_D2D(frame, uinfo, to, getPosition(to), mENodeBId);
        }

        delete uinfo;
        delete frame;
        return SINRs;
    // Retrieve from memory.
    } else {
        EV << "Oracle::getSINR fetches value from memory." << std::endl;
        return mMemory->get(time, from, to);
    }
}

IMobility* Oracle::getMobility(const MacNodeId& device) const {
    cModule *host = nullptr;
    try {
        UeInfo* ueInfo = getDeviceInfo(device);
        host = ueInfo->ue;
    } catch (const cRuntimeError &e) {
        // Getting the mobility of an eNodeB?
        EnbInfo* enbInfo = getENodeBInfo(device);
        host = enbInfo->eNodeB;
    }
    if (host == nullptr)
        throw cRuntimeError(std::string("Oracle::getMobility couldn't find the device's cModule!").c_str());
    IMobility* mobilityModule = check_and_cast<IMobility*>(host->getSubmodule("mobility"));
    if (mobilityModule == nullptr)
        throw cRuntimeError(std::string("Oracle::getMobility couldn't find a mobility module!").c_str());
    return mobilityModule;
}

Coord Oracle::getPosition(const MacNodeId& device) const {
    return getMobility(device)->getCurrentPosition();
}

/**
 * @param device The device's node ID.
 * @return The device's physical current speed. Coord.{x,y,z} are publicly available.
 */
Coord Oracle::getSpeed(const MacNodeId& device) const {
    return getMobility(device)->getCurrentSpeed();
}

double Oracle::getMean(const std::vector<double>& values) const {
    double sum = 0.0;
    for (size_t i = 0; i < values.size(); i++)
        sum += values.at(i);
    return (sum / ((double) values.size()));
}

Direction Oracle::determineDirection(const MacNodeId& from, const MacNodeId& to) const {
    Direction dir;
    if (from == mENodeBId)
        dir = Direction::DL; // eNodeB -DL-> UE.
    else if (to == mENodeBId)
        dir = Direction::UL; // UE -UL-> eNodeB.
    else
        dir = Direction::D2D; // UE -D2D-> UE.
    return dir;
}

double Oracle::getTransmissionPower(const MacNodeId& device, Direction dir) const {
    double transmissionPower;
    std::string modulePath = getDeviceInfo(device)->ue->getFullPath() + ".nic.phy";
    cModule *mod = getModuleByPath(modulePath.c_str()); // Get a pointer to the device's module.
    try {
        if (dir == Direction::D2D)
            transmissionPower = mod->par("d2dTxPower");
        else
            transmissionPower = mod->par("ueTxPower");
    } catch (const cRuntimeError& e) {
        // @TODO This error shouldn't happen! If there's more than one UE in UED2DTX array, then for the second one there's no parameter 'd2dTxPower'.
        EV_ERROR << "Oracle::getTransmissionPower(MacNodeId= " << device << ") encountered exception:" << e.what()
                << std::endl << " Using default value \"transmissionPower=24.14973348\"" << std::endl;
        transmissionPower = 24.14973348;
    }
    return transmissionPower;
}

MacNodeId Oracle::getEnodeBId() const {
    return mENodeBId;
}

const std::map<MacNodeId, std::map<MacNodeId, LteD2DMode>>* Oracle::getModeSelectionMap() const {
    return mModeSelectionMap;
}

void Oracle::setModeSelectionMap(const std::map<MacNodeId, std::map<MacNodeId, LteD2DMode>>* map) {
    mModeSelectionMap = map;
}

double euclideanDistance(Coord from, Coord to) {
    return sqrt(pow(from.x - to.x, 2) + pow(from.y - to.y, 2) + pow(from.z - to.z, 2));
}

int Oracle::getNumberOfBands() const {
    if (mAmc == nullptr)
        throw cRuntimeError("Oracle::getNumberOfBands called before it could set the AMC reference!");
    return mAmc->getSystemNumBands();
}

void Oracle::recordSchedulingRound(const SchedulingMemory& memory) {
    if (!mShouldRecordBandAllocation) {
        EV << NOW << " Oracle::recordSchedulingRound skipping." << endl;
        return;
    }
    EV << NOW << " Oracle::recordSchedulingRound" << endl;
    mBandAllocationTimepoints.push_back(NOW);
    mBandAllocationMemories.push_back(SchedulingMemory(memory));
}

std::string Oracle::getUeName(const MacNodeId id) const {
    UeInfo* info = getDeviceInfo(id);
    int index = info->id;
    return std::string(info->ue->getName()) + "[" + std::to_string(index) + "]";
}

void Oracle::initialize() {
    EV << "Oracle::initialize" << std::endl;
    // This entity is being initialized before a lot of other entities, like the eNodeBs and UEs, are deployed.
    // That's why final configuration needs to take place a bit later.
    mConfigTimepoint = par("configTimepoint").doubleValue();
    mUpdateInterval = par("updateInterval").doubleValue();
    mBandwidth = par("resourceBlockBandwidth").doubleValue();
    mShouldRecordBandAllocation = par("recordBandAllocation").boolValue();
    mConsiderTerminateFlagInSchedulingFunction = par("considerTerminateFlag").boolValue();
    reassignmentSchedulingDirection = par("reassignmentSchedulingDirection").stdstringValue();
    cConfigOption simTimeConfig("sim-time-limit", true, cConfigOption::Type::CFG_DOUBLE, "s", "300", "");
    double maxSimTime = getEnvir()->getConfig()->getAsDouble(&simTimeConfig);

    mMemory = new Memory(mUpdateInterval, maxSimTime, this);
    scheduleAt(mConfigTimepoint, mConfigMsg);
    // Schedule first update.
    scheduleAt(mConfigTimepoint + mUpdateInterval, mSnapshotMsg);

    // Schedule saving allocation history to file at end of simulation.
    scheduleAt(maxSimTime, mSaveAllocationHistoryMsg);
}

/**
 * Final configuration at some time point when other network devices are deployed and accessible.
 */
void Oracle::configure() {
    EV << "Oracle::configure" << std::endl;
    // Get device lists.
    std::vector<EnbInfo*>* enbInfo = getEnbInfo();
    std::vector<UeInfo*>* ueInfo = getUeInfo();
    if (enbInfo->size() == 0)
        throw cRuntimeError("Oracle::configure can't get AMC pointer because I couldn't find an eNodeB!");
    // -> its ID -> the node -> cast to the eNodeB class
    mENodeBId = enbInfo->at(0)->id;
    ExposedLteMacEnb *eNodeB = (ExposedLteMacEnb*) getMacByMacNodeId(mENodeBId);
    // -> get the AMC.
    mAmc = eNodeB->getAmc();
    if (mAmc == nullptr)
        throw cRuntimeError("Oracle::configure couldn't find an AMC.");
    else
        EV << "\tFound AMC." << endl;

    // Remember eNodeB position.
    mENodeBPosition = getPosition(mENodeBId);

    // Get deployer pointer.
    mDeployer = eNodeB->getDeployer();
    if (mDeployer == nullptr)
        throw cRuntimeError("Oracle::configure couldn't find the deployer.");
    EV << "\tFound deployer." << endl;

    // Get a pointer to the channel model.
    mChannelModel = check_and_cast<LteRealisticChannelModel*>(ueInfo->at(0)->phy->getChannelModel());
    if (mChannelModel != nullptr)
        EV << "\tFound channel model." << std::endl;
    else
        throw cRuntimeError("Oracle::configure couldn't find a channel model.");

    // Construct a feedback computer.
    mFeedbackComputer = getFeedbackComputation();
    if (mFeedbackComputer == nullptr)
        throw cRuntimeError("Oracle::configure couldn't construct the feedback computer.");
    else
        EV << "\tConstructed feedback computer." << endl;

    // Print info about all network devices.
    // UEs...
    EV << "\tThere are " << ueInfo->size() << " UEs in the network: " << std::endl;
    for (size_t i = 0; i < ueInfo->size(); i++) {
        Coord position = getPosition(ueInfo->at(i)->id);
        EV << "\t\t#" << i+1 << ": has MacNodeId " << ueInfo->at(i)->id << " and OmnetID " << getId(ueInfo->at(i)->id)
           << " and sits at position (" << position.x << ", " << position.y << ") and has a mean SINR to the eNB of "
           << getMean(getSINR(ueInfo->at(i)->id, mENodeBId, NOW, getTransmissionPower(ueInfo->at(i)->id, Direction::UL)))
           << " which is " << euclideanDistance(position, mENodeBPosition) << "m away" << std::endl;
    }
    // eNodeB...
    std::vector<EnbInfo*>* EnbInfo = getEnbInfo();
    EV << "\tThere are " << EnbInfo->size() << " EnBs in the network: " << std::endl;
    for (size_t i = 0; i < EnbInfo->size(); i++) {
        Coord position = getPosition(EnbInfo->at(i)->id);
        EV << "\t\t#" << i+1 << ": has MacNodeId " << EnbInfo->at(i)->id << " and OmnetID " << getId(EnbInfo->at(i)->id)
           << " and sits at position (" << position.x << ", " << position.y << ")." << std::endl;
    }

    }

void Oracle::handleMessage(cMessage *msg) {
    EV << "Oracle::handleMessage" << std::endl;
    if (msg == mSnapshotMsg)
        snapshot();
    else if (msg == mConfigMsg)
        configure();
    else if (msg == mSaveAllocationHistoryMsg)
        printSchedulingHistory(par("historyFilename").stringValue());
}

void Oracle::snapshot() {
    EV << "Oracle::snapshot" << std::endl;
    std::vector<UeInfo*>* ueInfo = getUeInfo();
    // For all UEs...
    for (size_t i = 0; i < ueInfo->size(); i++) {
        MacNodeId from = ueInfo->at(i)->id;
        // Find SINR to the eNodeB (cellular uplink).
        std::vector<double> sinrs_eNodeB = getSINR(from, mENodeBId, NOW, getTransmissionPower(from, Direction::UL));
        mMemory->put(NOW, from, mENodeBId, sinrs_eNodeB);
        // And for all other UEs...
        for (size_t j = 0; j < ueInfo->size(); j++) {
            MacNodeId to = ueInfo->at(j)->id;
            // Ignore link to current node.
            if (from == to)
                continue;
            // Calculate and save the current SINR for the D2D link.
            std::vector<double> sinrs = getSINR(from, to, NOW, getTransmissionPower(from, Direction::D2D));
            mMemory->put(NOW, from, to, sinrs);
        }
    }

    // Print current memory.
//    EV << mMemory->toString() << std::endl;

    // Schedule next snapshot.
    scheduleAt(NOW + mUpdateInterval, mSnapshotMsg);
}

std::vector<EnbInfo*>* Oracle::getEnbInfo() const {
    return mBinder->getEnbList();
}
std::vector<UeInfo*>* Oracle::getUeInfo() const {
    return mBinder->getUeList();
}

OmnetId Oracle::getId(const MacNodeId id) const {
    return mBinder->getOmnetId(id);
}
MacNodeId Oracle::getId(const OmnetId id) const {
    return mBinder->getMacNodeIdFromOmnetId(id);
}

UeInfo* Oracle::getDeviceInfo(const MacNodeId device) const {
  std::vector<UeInfo*>* ueInfo = getUeInfo();
  for (size_t i = 0; i < ueInfo->size(); i++) {
    UeInfo* currentInfo = ueInfo->at(i);
    if (currentInfo->id == device)
      return currentInfo;
  }
  throw cRuntimeError(std::string("Oracle::getDeviceInfo can't find the requested device ID \"" + std::to_string(device) + "\"").c_str());
}

EnbInfo* Oracle::getENodeBInfo(MacNodeId id) const {
  std::vector<EnbInfo*>* enbInfo = getEnbInfo();
  for (size_t i = 0; i < enbInfo->size(); i++) {
      EnbInfo* currentInfo = enbInfo->at(i);
    if (currentInfo->id == id)
      return currentInfo;
  }
  throw cRuntimeError("Oracle::getENodeBInfo can't find the requested eNodeB ID!");
}

ExposedFeedbackComputer* Oracle::getFeedbackComputation() const {
    // We're construction the feedback computer from a description.
    // There's REAL and DUMMY. We want REAL.
    std::string feedbackName = "REAL";
    // The four needed parameters will be supplied in this map.
    std::map<std::string, cMsgPar> parameterMap;

    // Each one must be specified in the Oracle.ned.
    // @TODO Parse the channel.xml instead because that's the values we want.
    cMsgPar targetBler("targetBler");
    targetBler.setDoubleValue(par("targetBler"));
    parameterMap["targetBler"] = targetBler;

    cMsgPar lambdaMinTh("lambdaMinTh");
    lambdaMinTh.setDoubleValue(par("lambdaMinTh"));
    parameterMap["lambdaMinTh"] = lambdaMinTh;

    cMsgPar lambdaMaxTh("lambdaMaxTh");
    lambdaMaxTh.setDoubleValue(par("lambdaMaxTh"));
    parameterMap["lambdaMaxTh"] = lambdaMaxTh;

    cMsgPar lambdaRatioTh("lambdaRatioTh");
    lambdaRatioTh.setDoubleValue(par("lambdaRatioTh"));
    parameterMap["lambdaRatioTh"] = lambdaRatioTh;

    // Taken from simulte/src/stack/phy/layer/LtePhyEnb.cc line 415.
    return ((ExposedFeedbackComputer*) new LteFeedbackComputationRealistic(
            targetBler, mDeployer->getLambda(), lambdaMinTh, lambdaMaxTh,
            lambdaRatioTh, mDeployer->getNumBands()));
}

std::string Oracle::vectorToString(const std::vector<double>& vec, const std::string& name) const {
    std::string description = "";
    for (size_t i = 0; i < vec.size(); i++)
        description += name + "[" + std::to_string(i) + "]=" + std::to_string(vec.at(i)) + " ";
    return description;
}

void Oracle::printSchedulingHistory(const std::string& filename) const {
    if (!mShouldRecordBandAllocation) {
        EV << NOW << " Oracle::printSchedulingHistory skipping." << std::endl;
        return;
    }
    EV << NOW << " Oracle::printSchedulingHistory" << std::endl;
    std::ofstream outfile;
    outfile.open (filename);
    // Grab all UE infos.
    std::vector<UeInfo*>* ueInfo = getUeInfo();
    // Print node IDs.
    outfile << "\t";
    for (size_t j = 0; j < ueInfo->size(); j++) {
        MacNodeId id = ueInfo->at(j)->id;
        outfile << "\t" << getUeName(id);
    }
    outfile << "\n";
    for (size_t i = 0; i < mBandAllocationTimepoints.size(); i++) {
        const SimTime& timepoint = mBandAllocationTimepoints.at(i);
        outfile << timepoint.dbl();
        const SchedulingMemory& memory = mBandAllocationMemories.at(i);
        for (size_t j = 0; j < ueInfo->size(); j++) {
            const MacNodeId id = ueInfo->at(j)->id;
            try {
                const std::vector<Band>& bands = memory.getBands(id);
                const std::vector<bool>& reassignments = memory.getReassignments(id);
                for (size_t k = 0; k < bands.size(); k++)
                    outfile << (k == 0 ? "\t" : "") << bands.at(k) << (reassignments.at(k) ? "r" : "") << (k < bands.size() - 1 ? "," : "");
            } catch (const std::exception& e) {
                // No bands allocated for this timepoint.
                outfile << "\tX";
                continue;
            }
        }
        outfile << "\n";
    }
    outfile.close();
}
