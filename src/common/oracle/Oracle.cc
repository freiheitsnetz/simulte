#include "common/oracle/Oracle.h"
#include "corenetwork/binder/LteBinder.h"
#include "stack/phy/layer/LtePhyBase.h"
#include <fstream>
#include <iostream>

Oracle* Oracle::SINGLETON = nullptr;

void Oracle::initialize() {
    EV << "Oracle::initialize" << std::endl;
    cConfigOption simTimeConfig("sim-time-limit", true, cConfigOption::Type::CFG_DOUBLE, "s", "300", "");
    maxSimTime = getEnvir()->getConfig()->getAsDouble(&simTimeConfig);

    scheduleAt(0.05, &configMessage);
}

/**
 * Final configuration at some time point when other network devices are deployed and accessible.
 */
void Oracle::configure() {
    EV << "Oracle::configure" << std::endl;
    std::vector<EnbInfo*>* list = getBinder()->getEnbList();
    EV << list->size() << " " << list->at(0)->id << endl;
    std::vector<UeInfo*>* ueList = getBinder()->getUeList();
    EV << getPosition(ueList->at(0)->id).x << getPosition(ueList->at(0)->id).y << " txPwr=" << getTxPower(ueList->at(0)->id, Direction::D2D) << endl;

    // Calculate all pair-wise SINRs.
    std::map<MacNodeId, std::map<MacNodeId, std::vector<double>>> SINRMap;
    for (size_t i = 0; i < ueList->size(); i++) {
        for (size_t j = 0; j < ueList->size(); j++) {
            if (j == i)
                continue;
            MacNodeId from = ueList->at(i)->id;
            MacNodeId to = ueList->at(j)->id;
            std::vector<double> SINRs = getSINR(from, to);
            SINRMap[from][to] = SINRs;
        }
    }
    // And print them.
    for (size_t i = 0; i < ueList->size(); i++) {
            for (size_t j = 0; j < ueList->size(); j++) {
                if (i == j)
                    continue;
                MacNodeId from = ueList->at(i)->id;
                MacNodeId to = ueList->at(j)->id;
                std::vector<double> SINRs = SINRMap[from][to];
                EV << "SINR[" << getName(from) << "][" << getName(to) << "] = " << SINRs.at(0);
                for (size_t i = 1; i < SINRs.size(); i++)
                	EV << ", " << SINRs.at(i);
                EV << "." << endl;

//                ofstream sinrsFile;
//                sinrsFile.open("sinrs", std::ios_base::app);
//                sinrsFile << SINRs.at(0) << endl;
//                sinrsFile.close();
//
//                ofstream attFile;
//                attFile.open("att", std::ios_base::app);
//                attFile << getAttenuation(from, to) << endl;
//                attFile.close();
            }
    }
}

void Oracle::handleMessage(cMessage *msg) {
    EV << "Oracle::handleMessage" << std::endl;
    if (msg == &configMessage) {
        configure();
    }
}

MacNodeId Oracle::getEnodeBID() const {
    std::vector<EnbInfo*>* list = getBinder()->getEnbList();
    if (list->size() == 0)
        throw cRuntimeError("Oracle::getEnodeBID called, but eNodeB list is empty.");
    return list->at(0)->id;
}

size_t Oracle::getNumRBs() const {
	return getBinder()->getNumBands();
}

Direction Oracle::determineDirection(const MacNodeId from, const MacNodeId to) const {
    Direction dir;
    if (from == getEnodeBID())
        dir = Direction::DL; // eNodeB -DL-> UE.
    else if (to == getEnodeBID())
        dir = Direction::UL; // UE -UL-> eNodeB.
    else
        dir = Direction::D2D; // UE -D2D-> UE.
    return dir;
}

LtePhyBase* Oracle::getPhyBase(const MacNodeId id) const {
    LtePhyBase* phyBase = nullptr;
    if (id != getEnodeBID()) {
		std::vector<UeInfo*>* ueList = getBinder()->getUeList();
		std::vector<UeInfo*>::iterator iterator = ueList->begin();
		while (iterator != ueList->end()) {
			if ((*iterator)->id == id) {
				phyBase = (*iterator)->phy;
				break;
			}
			iterator++;
		}
    } else {
    	throw runtime_error("Oracle::getPhyBase(eNodeB) not supported.");
    }

    if (phyBase == nullptr)
        throw cRuntimeError("Oracle::getPhyBase couldn't find node's LtePhyBase.");
    return phyBase;
}

const UeInfo* Oracle::getUeInfo(const MacNodeId& id) const {
	const UeInfo* info = nullptr;
	std::vector<UeInfo*>* ueList = getBinder()->getUeList();
	std::vector<UeInfo*>::iterator iterator = ueList->begin();
	while (iterator != ueList->end()) {
		if ((*iterator)->id == id) {
			info = (*iterator);
			break;
		}
		iterator++;
	}
	if (info == nullptr)
		throw cRuntimeError("Oracle::getUeInfo couldn't find node's UeInfo.");
	return info;
}

std::string Oracle::getApplicationName(const MacNodeId& id) const {
	const UeInfo* info = getUeInfo(id);
	cModule* module = info->ue;
	int numUdpApps = stoi(module->par("numUdpApps").info()),
		numTcpApps = stoi(module->par("numTcpApps").info());
	if (numUdpApps > 0) {
		return module->getSubmodule("udpApp", 0)->getClassName();
	} else if (numTcpApps > 0) {
		return module->getSubmodule("tcpApp", 0)->getClassName();
	} else {
		throw cRuntimeError("Oracle::getApplicationName but node has neither UDP nor TCP apps.");
	}
}

std::string Oracle::getName(const MacNodeId id) const {
	std::vector<UeInfo*>* ueList = getBinder()->getUeList();
	std::string name = "COULDN'T_DETERMINE_NAME";
	for (std::vector<UeInfo*>::iterator iterator = ueList->begin(); iterator != ueList->end(); iterator++) {
		if ((*iterator)->id == id) {
			name = (*iterator)->ue->getFullName();
		}
	}
	return name;
}

inet::Coord Oracle::getPosition(const MacNodeId id) const {
	if (id != getEnodeBID()) {
		LtePhyBase* phyBase = getPhyBase(id);
		return phyBase->getCoord();
	} else {
		cModule* eNodeB = getBinder()->getEnbList()->at(0)->eNodeB;
		inet::Coord coord(eNodeB->getSubmodule("mobility")->par("initialX").doubleValue(), eNodeB->getSubmodule("mobility")->par("initialY").doubleValue());
		return coord;
	}
}

double Oracle::getDistance(inet::Coord from, inet::Coord to) const {
    double distance = 0.0;
    return sqrt(pow(from.x - to.x, 2) + pow(from.y - to.y, 2));
}

double Oracle::getTxPower(const MacNodeId id, Direction dir) const {
	if (id == getEnodeBID()) {
		// Take any UE.
		LtePhyBase* phyBase = getPhyBase(getBinder()->getUeList()->at(0)->id);
		// Because all UEs know the eNodeB's txPower.
		return phyBase->getMacroTxPwr();
	} else {
		LtePhyBase* phyBase = getPhyBase(id);
		return phyBase->getTxPwr(dir);
	}
}

std::vector<double> Oracle::getSINR(const MacNodeId from, const MacNodeId to) const {
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
    uinfo.setTxPower(getTxPower(from, Direction::UL));
    uinfo.setD2dTxPower(getTxPower(from, Direction::D2D));
    LteRealisticChannelModel* channelModel = dynamic_cast<LteRealisticChannelModel*>(getPhyBase(from)->getChannelModel());
    return channelModel->getSINR_D2D(&frame, &uinfo, to, getPosition(to), getEnodeBID());
}

double Oracle::getChannelGain(MacNodeId from, MacNodeId to, vector<Band> resources) const {
    LteRealisticChannelModel* channelModel = nullptr;
    if (from != getEnodeBID()) {
        channelModel = dynamic_cast<LteRealisticChannelModel*>(getPhyBase(from)->getChannelModel());
    } else {
        channelModel = dynamic_cast<LteRealisticChannelModel*>(getPhyBase(to)->getChannelModel());
    }

    // These physical effects affect the signal on its way.
    double antennaGainTx = channelModel->antennaGainUe_;
    double antennaGainRx = channelModel->antennaGainUe_;
    double cableLoss = channelModel->cableLoss_;
    double speed = channelModel->computeSpeed(from, getPosition(from));
    double pathAttenuation = getAttenuation(from, to);

    // Now we can find the received signal strength.
    Direction dir = determineDirection(from, to);
    double receivedPower = getTxPower(from, dir);

    receivedPower -= pathAttenuation;
    receivedPower += antennaGainTx;
    receivedPower += antennaGainRx;
    receivedPower -= cableLoss;
    // Apply fading if enabled.
    double fading = 0.0;
    if (channelModel->fading_) {
        // We are interested in the average channel gain.
        // So we find the average fading over all resource blocks used.
        for (Band resource : resources) {
            if (channelModel->fadingType_ == LteRealisticChannelModel::FadingType::RAYLEIGH)
                fading += channelModel->rayleighFading(from, resource);
            else if (channelModel->fadingType_ == LteRealisticChannelModel::FadingType::JAKES) {
                // D2D is treated like downlink for receivers, so only if it's uplink does this have to be 'false'.
                bool cqiDl = (dir == UL ? false : true);
                fading += channelModel->jakesFading(from, speed, resource, cqiDl);
            }
        }
        fading /= ((double) channelModel->band_);
        receivedPower += fading;
    }

    double senderPower = getTxPower(from, dir);
    //cout << "antennaGainTx=" << antennaGainTx << " antennaGainRx=" << antennaGainRx << " cableLoss=" << cableLoss <<  " speed=" << speed << " pathAttenuation=" << pathAttenuation << " fading=" << fading << " receiverPower=" << getTxPower(to, dir) << " senderPower=" << senderPower << " receivedPower=" << receivedPower << endl;

    // Channel gain is a ratio power_in/power_out that tells you how much signal power is lost on the way.
    double gain_dBm = receivedPower - senderPower;
    double gain_linear = dBToLinear(gain_dBm);

    return gain_linear;
}

double Oracle::getChannelGain(MacNodeId from, MacNodeId to) const {
	vector<Band> resources;
	for (Band rb = 0; rb < getNumRBs(); rb++)
	    resources.push_back(rb);
	return getChannelGain(from, to, resources);
}

double Oracle::getAttenuation(const MacNodeId from, const MacNodeId to) const {
	Direction dir = determineDirection(from, to);

	LteRealisticChannelModel* channelModel = nullptr;
	if (from == getEnodeBID())
		channelModel = dynamic_cast<LteRealisticChannelModel*>(getPhyBase(to)->getChannelModel());
	else
		channelModel = dynamic_cast<LteRealisticChannelModel*>(getPhyBase(from)->getChannelModel());

	if (channelModel == nullptr)
		throw invalid_argument("Oracle::getAttenuation couldn't fetch channel model.");

	if (dir == Direction::D2D)
		return channelModel->getAttenuation_D2D(from, dir, getPosition(from), to, getPosition(to));
	else
		return channelModel->getAttenuation(from, dir, getPosition(to));
}

std::vector<double> Oracle::getInCellInterference(const MacNodeId from, const MacNodeId to, bool considerThisTTI) const {
	std::vector<double> interferences(getNumRBs(), 0.0);
	Direction dir = determineDirection(from, to);
	LteRealisticChannelModel* channelModel = dynamic_cast<LteRealisticChannelModel*>(getPhyBase(from)->getChannelModel());
	// Function call modifies 'interferences' vector.
	channelModel->computeInCellD2DInterference(getEnodeBID(), from, getPosition(from), to, getPosition(to), considerThisTTI, &interferences, dir);
	return interferences;
}

Cqi Oracle::getCQI(const MacNodeId from, const MacNodeId to) const {
	if (feedbackComputer == nullptr)
		throw runtime_error("Oracle::getCQI feedback computation module not yet set.");
	vector<double> sinrs = getSINR(from, to);
	double sinr = 0.0;
	for (double value : sinrs)
		sinr += value;
	sinr /= sinrs.size();
	return ((LteFeedbackComputationRealistic*) feedbackComputer)->getCqi(TxMode::SINGLE_ANTENNA_PORT0, sinr);

//    MacNodeId enodebId = getEnodeBID();
//    LteAmc* amc = ((LteMacEnb*) getMacByMacNodeId(enodebId))->getAmc();
//    Direction dir = determineDirection(from, to);
//    std::vector<Cqi> cqis;
//    try {
//		if (dir == Direction::D2D) {
//			LteSummaryFeedback feedback = amc->getFeedbackD2D(from, Remote::MACRO, TxMode::SINGLE_ANTENNA_PORT0, to);
//			cqis = feedback.getCqi(0);
//		} else {
//			LteSummaryFeedback feedback = amc->getFeedback(from, Remote::MACRO, TxMode::SINGLE_ANTENNA_PORT0, dir);
//			cqis = feedback.getCqi(0);
//		}
//    } catch (const exception& e) {
//    	cerr << "Oracle::getCQI couldn't determine CQI yet. Maybe no feedback has been reported?" << endl;
//    }
//    return cqis;
}

void Oracle::printAllocation(std::vector<std::vector<AllocatedRbsPerBandMapA>>& allocatedRbsPerBand) {
	for (Band resource = 0; resource < getNumRBs(); resource++) {
		std::cout << NOW << " Oracle::printAllocation Band " << resource << std::endl;
		AllocatedRbsPerBandInfo& info = allocatedRbsPerBand[MAIN_PLANE][MACRO][resource];
		UeAllocatedBlocksMapA& rbInfo = info.ueAllocatedRbsMap_;
		std::vector<UeInfo*> ueList = *getBinder()->getUeList();
		for (size_t i = 0; i < ueList.size(); i++) {
			MacNodeId id = ueList.at(i)->id;
			if (rbInfo.find(id) != rbInfo.end())
				std::cout << "\t -" << rbInfo[id] << "RBs-> " << getName(id) << std::endl;
		}
	}
}

bool Oracle::isD2DFlow(const MacCid& nodeId) {
	const UeInfo* info = getUeInfo(MacCidToNodeId(nodeId));
	cModule* module = info->ue;
	string d2dCapable = module->par("d2dCapable").info();
	return d2dCapable == "true";
//	unsigned short lcid = MacCidToLcid(nodeId);
//	cout << nodeId << " -> " << lcid << " == " << (lcid == D2D_SHORT_BSR ? "D@D_SHORT_BSR" : "no") << endl;
//	return lcid == D2D_SHORT_BSR || lcid == D2D_MULTI_SHORT_BSR || lcid == D2D;
}

double Oracle::getD2DPenalty() const {
	return par("d2dPenalty").doubleValue();
}

std::pair<double, double> Oracle::stackelberg_getD2DPowerLimits() const {
    double txmax = par("stackelberg_d2dPowerLimit_max").doubleValue(),
            txmin = par("stackelberg_d2dPowerLimit_min").doubleValue();
    return pair<double, double>(txmax, txmin);
}

double Oracle::stackelberg_getBeta() const {
    return par("stackelberg_beta").doubleValue();
}

double Oracle::stackelberg_getDelta() const {
    return par("stackelberg_delta").doubleValue();
}

std::string Oracle::stackelberg_getLeaderScheduler() const {
    return par("stackelberg_scheduleLeaders").stringValue();
}

MacNodeId Oracle::getTransmissionPartner(const MacNodeId id) const {
	string appName = getApplicationName(id);
	if (appName == string("inet::UDPBasicApp")) {
		try {
			const UeInfo* info = getUeInfo(id);
			string targetName = info->ue->getSubmodule("udpApp", 0)->par("destAddresses").stringValue();
			std::vector<UeInfo*>* ueList = getBinder()->getUeList();
			for (auto iterator = ueList->begin(); iterator != ueList->end(); iterator++) {
				const UeInfo* partnerInfo = *iterator;
				MacNodeId partnerId = partnerInfo->id;
				string partnerName = getName(partnerId);
				if (partnerName == targetName)
					return partnerId;
			}
		} catch (const exception& e) {
			throw invalid_argument("Oracle::getTransmissionPartner couldn't find partner for '" + getName(id) + "'.");
		}
	} else if (appName == string("inet::UDPSink")) {
		string targetName = getName(id);
		string from = "Rx", to = "Tx";
		size_t start_pos = targetName.find(from);
		targetName.replace(start_pos, from.length(), to);

		std::vector<UeInfo*>* ueList = getBinder()->getUeList();
		for (auto iterator = ueList->begin(); iterator != ueList->end(); iterator++) {
			const UeInfo* partnerInfo = *iterator;
			MacNodeId partnerId = partnerInfo->id;
			string partnerName = getName(partnerId);			;
			if (partnerName == targetName) {
				return partnerId;
			}
		}
	} else if (appName == string("VoIPSender")) {
	    try {
            const UeInfo* info = getUeInfo(id);
            string targetName = info->ue->getSubmodule("udpApp", 0)->par("destAddress").stringValue();
            std::vector<UeInfo*>* ueList = getBinder()->getUeList();
            for (auto iterator = ueList->begin(); iterator != ueList->end(); iterator++) {
                const UeInfo* partnerInfo = *iterator;
                MacNodeId partnerId = partnerInfo->id;
                string partnerName = getName(partnerId);
                if (partnerName == targetName)
                    return partnerId;
            }
        } catch (const exception& e) {
            throw invalid_argument("Oracle::getTransmissionPartner couldn't find partner for '" + getName(id) + "'.");
        }
	}

	throw invalid_argument("Oracle::getTransmissionPartner doesn't support '" + getName(id) + "' with app '" + appName + "'.");
}
