#include "common/oracle/Oracle.h"
#include "corenetwork/binder/LteBinder.h"
#include "stack/phy/layer/LtePhyBase.h"

Oracle* Oracle::SINGLETON = nullptr;

void Oracle::initialize() {
    EV << "Oracle::initialize" << std::endl;
    cConfigOption simTimeConfig("sim-time-limit", true, cConfigOption::Type::CFG_DOUBLE, "s", "300", "");
    maxSimTime = getEnvir()->getConfig()->getAsDouble(&simTimeConfig);

    scheduleAt(0.05, configMessage);
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
                EV << "Oracle::SINRs[" << from << "][" << to << "] = " << SINRs.at(0) << " at distance " << getDistance(getPosition(from), getPosition(to)) << endl;
            }
    }
    EV << "Oracle::SINRs[1025][1026] = " << SINRMap[1025][1026].at(0) << std::endl;
    EV << "Oracle::SINRs[1026][1027] = " << SINRMap[1026][1027].at(0) << std::endl;
    EV << "Oracle::SINRs[1027][1028] = " << SINRMap[1027][1028].at(0) << std::endl;
    EV << "Oracle::SINRs[1028][1029] = " << SINRMap[1028][1029].at(0) << std::endl;
    EV << "Oracle::SINRs[1029][1030] = " << SINRMap[1029][1030].at(0) << std::endl;

    EV << "Oracle::Att[1025][1026] = " << getAttenuation(1025, 1026) << std::endl;
	EV << "Oracle::Att[1026][1027] = " << getAttenuation(1026, 1027) << std::endl;
	EV << "Oracle::Att[1027][1028] = " << getAttenuation(1027, 1028) << std::endl;
	EV << "Oracle::Att[1028][1029] = " << getAttenuation(1028, 1029) << std::endl;
	EV << "Oracle::Att[1029][1030] = " << getAttenuation(1029, 1030) << std::endl;

	EV << "Oracle::int[1025][1036] = " << getInCellInterference(1029, 1030, true).at(0) << std::endl;
}

void Oracle::handleMessage(cMessage *msg) {
    EV << "Oracle::handleMessage" << std::endl;
    if (msg == configMessage)
        configure();
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
    std::vector<UeInfo*>* ueList = getBinder()->getUeList();
    std::vector<UeInfo*>::iterator iterator = ueList->begin();
    while (iterator != ueList->end()) {
        if ((*iterator)->id == id) {
            phyBase = (*iterator)->phy;
            break;
        }
        iterator++;
    }
    if (phyBase == nullptr)
        throw cRuntimeError("Oracle::getPhyBase couldn't find node's LtePhyBase.");
    return phyBase;
}

inet::Coord Oracle::getPosition(const MacNodeId id) const {
    LtePhyBase* phyBase = getPhyBase(id);
    return phyBase->getCoord();
}

double Oracle::getDistance(inet::Coord from, inet::Coord to) const {
    double distance = 0.0;
    return sqrt(pow(from.x - to.x, 2) + pow(from.y - to.y, 2));
}

double Oracle::getTxPower(const MacNodeId id, Direction dir) const {
    LtePhyBase* phyBase = getPhyBase(id);
    return phyBase->getTxPwr(dir);
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

double Oracle::getAttenuation(const MacNodeId from, const MacNodeId to) const {
	Direction dir = determineDirection(from, to);
	LteRealisticChannelModel* channelModel = dynamic_cast<LteRealisticChannelModel*>(getPhyBase(from)->getChannelModel());
	return channelModel->getAttenuation_D2D(from, dir, getPosition(from), to, getPosition(to));
}

std::vector<double> Oracle::getInCellInterference(const MacNodeId from, const MacNodeId to, bool considerThisTTI) const {
	std::vector<double> interferences(getNumRBs(), 0.0);
	Direction dir = determineDirection(from, to);
	LteRealisticChannelModel* channelModel = dynamic_cast<LteRealisticChannelModel*>(getPhyBase(from)->getChannelModel());
	// Function call modifies 'interferences' vector.
	channelModel->computeInCellD2DInterference(getEnodeBID(), from, getPosition(from), to, getPosition(to), considerThisTTI, &interferences, dir);
	return interferences;
}
