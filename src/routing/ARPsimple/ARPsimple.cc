/*
 * Copyright (C) 2004 Andras Varga
 * Copyright (C) 2008 Alfonso Ariza Quintana (global arp)
 * Copyright (C) 2014 OpenSim Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <ARPsimple.h>


#include "inet/networklayer/arp/ipv4/ARPPacket_m.h"
#include "inet/linklayer/common/Ieee802Ctrl.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/networklayer/ipv4/IIPv4RoutingTable.h"
#include "inet/networklayer/contract/ipv4/IPv4ControlInfo.h"
#include "inet/networklayer/ipv4/IPv4Datagram.h"
#include "inet/networklayer/ipv4/IPv4InterfaceData.h"
#include "inet/common/lifecycle/NodeOperations.h"
#include "inet/common/lifecycle/NodeStatus.h"

namespace inet {

simsignal_t ARPsimple::sentReqSignal = registerSignal("sentReq");
simsignal_t ARPsimple::sentReplySignal = registerSignal("sentReply");

static std::ostream& operator<<(std::ostream& out, cMessage *msg)
{
    out << "(" << msg->getClassName() << ")" << msg->getFullName();
    return out;
}

static std::ostream& operator<<(std::ostream& out, const ARPsimple::ARPCacheEntry& e)
{
    if (e.pending)
        out << "pending (" << e.numRetries << " retries)";
    else
        out << "MAC:" << e.macAddress << "  age:" << floor(simTime() - e.lastUpdate) << "s";
    return out;
}

Define_Module(ARPsimple);

ARPsimple::ARPsimple()
{
}

void ARPsimple::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        retryTimeout = par("retryTimeout");
        retryCount = par("retryCount");
        cacheTimeout = par("cacheTimeout");
        respondToProxyARP = par("respondToProxyARP");

        netwOutGate = gate("netwOut");

        // init statistics
        numRequestsSent = numRepliesSent = 0;
        numResolutions = numFailedResolutions = 0;
        WATCH(numRequestsSent);
        WATCH(numRepliesSent);
        WATCH(numResolutions);
        WATCH(numFailedResolutions);

        WATCH_PTRMAP(arpCache);
    }
    else if (stage == INITSTAGE_NETWORK_LAYER_3) {    // IP addresses should be available
        ift = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
        rt = getModuleFromPar<IIPv4RoutingTable>(par("routingTableModule"), this);
        //Added for D2DMH
        binder_=check_and_cast<LteBinder*>(getSimulation()->getModuleByPath("binder"));

        isUp = isNodeUp();
    }
}

void ARPsimple::finish()
{
}

ARPsimple::~ARPsimple()
{
    for (auto & elem : arpCache)
        delete elem.second;
}

void ARPsimple::handleMessage(cMessage *msg)
{
    if (!isUp) {
        handleMessageWhenDown(msg);
        return;
    }

    if (msg->isSelfMessage()) {
        requestTimedOut(msg);
    }
    else {
        ARPPacket *arp = check_and_cast<ARPPacket *>(msg);
        processARPPacket(arp);
    }

    if (hasGUI())
        updateDisplayString();
}

void ARPsimple::handleMessageWhenDown(cMessage *msg)
{
    if (msg->isSelfMessage())
        throw cRuntimeError("Model error: self msg '%s' received when protocol is down", msg->getName());
    EV_WARN << "Protocol is turned off, dropping '" << msg->getName() << "' message\n";
    delete msg;
}

bool ARPsimple::handleOperationStage(LifecycleOperation *operation, int stage, IDoneCallback *doneCallback)
{
    Enter_Method_Silent();

    if (dynamic_cast<NodeStartOperation *>(operation)) {
        if ((NodeStartOperation::Stage)stage == NodeStartOperation::STAGE_NETWORK_LAYER)
            start();
    }
    else if (dynamic_cast<NodeShutdownOperation *>(operation)) {
        if ((NodeShutdownOperation::Stage)stage == NodeShutdownOperation::STAGE_NETWORK_LAYER)
            stop();
    }
    else if (dynamic_cast<NodeCrashOperation *>(operation)) {
        if ((NodeCrashOperation::Stage)stage == NodeCrashOperation::STAGE_CRASH)
            stop();
    }
    else {
        throw cRuntimeError("Unsupported operation '%s'", operation->getClassName());
    }
    return true;
}

void ARPsimple::start()
{
    ASSERT(arpCache.empty());
    isUp = true;
}

void ARPsimple::stop()
{
    isUp = false;
    flush();
}

void ARPsimple::flush()
{
    while (!arpCache.empty()) {
        auto i = arpCache.begin();
        ARPCacheEntry *entry = i->second;
        cancelAndDelete(entry->timer);
        entry->timer = nullptr;
        delete entry;
        arpCache.erase(i);
    }
}

bool ARPsimple::isNodeUp()
{
    NodeStatus *nodeStatus = dynamic_cast<NodeStatus *>(findContainingNode(this)->getSubmodule("status"));
    return !nodeStatus || nodeStatus->getState() == NodeStatus::UP;
}

void ARPsimple::updateDisplayString()
{
    std::stringstream os;

    os << arpCache.size() << " cache entries\nsent req:" << numRequestsSent
       << " repl:" << numRepliesSent << " fail:" << numFailedResolutions;

    getDisplayString().setTagArg("t", 0, os.str().c_str());
}

void ARPsimple::initiateARPResolution(ARPCacheEntry *entry)
{
    IPv4Address nextHopAddr = entry->myIter->first;
    entry->pending = true;
    entry->numRetries = 0;
    entry->lastUpdate = SIMTIME_ZERO;
    entry->macAddress = MACAddress::UNSPECIFIED_ADDRESS;
    sendARPRequest(entry->ie, nextHopAddr);

    // start timer
    cMessage *msg = entry->timer = new cMessage("ARP timeout");
    msg->setContextPointer(entry);
    scheduleAt(simTime() + retryTimeout, msg);

    numResolutions++;
    Notification signal(nextHopAddr, MACAddress::UNSPECIFIED_ADDRESS, entry->ie);
    emit(initiatedARPResolutionSignal, &signal);
}

void ARPsimple::sendPacketToNIC(cMessage *msg, const InterfaceEntry *ie, const MACAddress& macAddress, int etherType)
{
    // add control info with MAC address
    Ieee802Ctrl *controlInfo = new Ieee802Ctrl();
    controlInfo->setDest(macAddress);
    controlInfo->setEtherType(etherType);
    controlInfo->setInterfaceId(ie->getInterfaceId());
    msg->setControlInfo(controlInfo);

    // send out
    EV_INFO << "Sending " << msg << " to network protocol.\n";
    send(msg, netwOutGate);
}

void ARPsimple::sendARPRequest(const InterfaceEntry *ie, IPv4Address ipAddress)
{
    // find our own IPv4 address and MAC address on the given interface
    MACAddress myMACAddress = ie->getMacAddress();
    IPv4Address myIPAddress = ie->ipv4Data()->getIPAddress();

    // both must be set
    ASSERT(!myMACAddress.isUnspecified());
    ASSERT(!myIPAddress.isUnspecified());

    // fill out everything in ARP Request packet except dest MAC address
    ARPPacket *arp = new ARPPacket("arpREQ");
    arp->setByteLength(ARP_HEADER_BYTES);
    arp->setOpcode(ARP_REQUEST);
    arp->setSrcMACAddress(myMACAddress);
    arp->setSrcIPAddress(myIPAddress);
    arp->setDestIPAddress(ipAddress);

    sendPacketToNIC(arp, ie, MACAddress::BROADCAST_ADDRESS, ETHERTYPE_ARP);
    numRequestsSent++;
    emit(sentReqSignal, 1L);
}

void ARPsimple::requestTimedOut(cMessage *selfmsg)
{
    ARPCacheEntry *entry = (ARPCacheEntry *)selfmsg->getContextPointer();
    entry->numRetries++;
    if (entry->numRetries < retryCount) {
        // retry
        IPv4Address nextHopAddr = entry->myIter->first;
        EV_INFO << "ARP request for " << nextHopAddr << " timed out, resending\n";
        sendARPRequest(entry->ie, nextHopAddr);
        scheduleAt(simTime() + retryTimeout, selfmsg);
        return;
    }

    delete selfmsg;

    // max retry count reached: ARP failure.
    // throw out entry from cache
    EV << "ARP timeout, max retry count " << retryCount << " for " << entry->myIter->first << " reached.\n";
    Notification signal(entry->myIter->first, MACAddress::UNSPECIFIED_ADDRESS, entry->ie);
    emit(failedARPResolutionSignal, &signal);
    arpCache.erase(entry->myIter);
    delete entry;
    numFailedResolutions++;
}

bool ARPsimple::addressRecognized(IPv4Address destAddr, InterfaceEntry *ie)
{
    if (rt->isLocalAddress(destAddr)) {
        return true;
    }
    else if (respondToProxyARP) {
        // respond to Proxy ARP request: if we can route this packet (and the
        // output port is different from this one), say yes
        InterfaceEntry *rtie = rt->getInterfaceForDestAddr(destAddr);
        return rtie != nullptr && rtie != ie;
    }
    else {
        return false;
    }
}

void ARPsimple::dumpARPPacket(ARPPacket *arp)
{
    EV_DETAIL << (arp->getOpcode() == ARP_REQUEST ? "ARP_REQ" : arp->getOpcode() == ARP_REPLY ? "ARP_REPLY" : "unknown type")
              << "  src=" << arp->getSrcIPAddress() << " / " << arp->getSrcMACAddress()
              << "  dest=" << arp->getDestIPAddress() << " / " << arp->getDestMACAddress() << "\n";
}

void ARPsimple::processARPPacket(ARPPacket *arp)
{
    EV_INFO << "Received " << arp << " from network protocol.\n";
    dumpARPPacket(arp);

    // extract input port
    IMACProtocolControlInfo *ctrl = check_and_cast<IMACProtocolControlInfo *>(arp->removeControlInfo());
    InterfaceEntry *ie = ift->getInterfaceById(ctrl->getInterfaceId());
    delete ctrl;

    //
    // Recipe a'la RFC 826:
    //
    // ?Do I have the hardware type in ar$hrd?
    // Yes: (almost definitely)
    //   [optionally check the hardware length ar$hln]
    //   ?Do I speak the protocol in ar$pro?
    //   Yes:
    //     [optionally check the protocol length ar$pln]
    //     Merge_flag := false
    //     If the pair <protocol type, sender protocol address> is
    //         already in my translation table, update the sender
    //         hardware address field of the entry with the new
    //         information in the packet and set Merge_flag to true.
    //     ?Am I the target protocol address?
    //     Yes:
    //       If Merge_flag is false, add the triplet <protocol type,
    //           sender protocol address, sender hardware address> to
    //           the translation table.
    //       ?Is the opcode ares_op$REQUEST?  (NOW look at the opcode!!)
    //       Yes:
    //         Swap hardware and protocol fields, putting the local
    //             hardware and protocol addresses in the sender fields.
    //         Set the ar$op field to ares_op$REPLY
    //         Send the packet to the (new) target hardware address on
    //             the same hardware on which the request was received.
    //

    MACAddress srcMACAddress = arp->getSrcMACAddress();
    IPv4Address srcIPAddress = arp->getSrcIPAddress();

    if (srcMACAddress.isUnspecified())
        throw cRuntimeError("wrong ARP packet: source MAC address is empty");
    if (srcIPAddress.isUnspecified())
        throw cRuntimeError("wrong ARP packet: source IPv4 address is empty");

    bool mergeFlag = false;
    // "If ... sender protocol address is already in my translation table"
    auto it = arpCache.find(srcIPAddress);
    if (it != arpCache.end()) {
        // "update the sender hardware address field"
        ARPCacheEntry *entry = it->second;
        updateARPCache(entry, srcMACAddress);
        mergeFlag = true;
    }

    // "?Am I the target protocol address?"
    // if Proxy ARP is enabled, we also have to reply if we're a router to the dest IPv4 address
    if (addressRecognized(arp->getDestIPAddress(), ie)) {
        // "If Merge_flag is false, add the triplet protocol type, sender
        // protocol address, sender hardware address to the translation table"
        if (!mergeFlag) {
            ARPCacheEntry *entry;
            if (it != arpCache.end()) {
                entry = it->second;
            }
            else {
                entry = new ARPCacheEntry();
                auto where = arpCache.insert(arpCache.begin(), std::make_pair(srcIPAddress, entry));
                entry->myIter = where;
                entry->ie = ie;

                entry->pending = false;
                entry->timer = nullptr;
                entry->numRetries = 0;
            }
            updateARPCache(entry, srcMACAddress);
        }

        // "?Is the opcode ares_op$REQUEST?  (NOW look at the opcode!!)"
        switch (arp->getOpcode()) {
            case ARP_REQUEST: {
                EV_DETAIL << "Packet was ARP REQUEST, sending REPLY\n";

                // find our own IPv4 address and MAC address on the given interface
                MACAddress myMACAddress = ie->getMacAddress();
                IPv4Address myIPAddress = ie->ipv4Data()->getIPAddress();

                // "Swap hardware and protocol fields", etc.
                arp->setName("arpREPLY");
                IPv4Address origDestAddress = arp->getDestIPAddress();
                arp->setDestIPAddress(srcIPAddress);
                arp->setDestMACAddress(srcMACAddress);
                arp->setSrcIPAddress(origDestAddress);
                arp->setSrcMACAddress(myMACAddress);
                arp->setOpcode(ARP_REPLY);
                delete arp->removeControlInfo();
                sendPacketToNIC(arp, ie, srcMACAddress, ETHERTYPE_ARP);
                numRepliesSent++;
                emit(sentReplySignal, 1L);
                break;
            }

            case ARP_REPLY: {
                EV_DETAIL << "Discarding packet\n";
                delete arp;
                break;
            }

            case ARP_RARP_REQUEST:
                throw cRuntimeError("RARP request received: RARP is not supported");

            case ARP_RARP_REPLY:
                throw cRuntimeError("RARP reply received: RARP is not supported");

            default:
                throw cRuntimeError("Unsupported opcode %d in received ARP packet", arp->getOpcode());
        }
    }
    else {
        // address not recognized
        EV_INFO << "IPv4 address " << arp->getDestIPAddress() << " not recognized, dropping ARP packet\n";
        delete arp;
    }
}

void ARPsimple::updateARPCache(ARPCacheEntry *entry, const MACAddress& macAddress)
{
    EV_DETAIL << "Updating ARP cache entry: " << entry->myIter->first << " <--> " << macAddress << "\n";

    // update entry
    if (entry->pending) {
        entry->pending = false;
        delete cancelEvent(entry->timer);
        entry->timer = nullptr;
        entry->numRetries = 0;
    }
    entry->macAddress = macAddress;
    entry->lastUpdate = simTime();
    Notification signal(entry->myIter->first, macAddress, entry->ie);
    emit(completedARPResolutionSignal, &signal);
}
/*CHANGED for D2DMH. No Arp req will be sent anymore since binder knows all mac addresses*/
MACAddress ARPsimple::resolveL3Address(const L3Address& address, const InterfaceEntry *ie)
{
    IPv4Address tmp=address.toIPv4();
    return binder_->getMACfromIP(tmp);
}

L3Address ARPsimple::getL3AddressFor(const MACAddress& macAddr) const
{
    Enter_Method_Silent();

    if (macAddr.isUnspecified())
        return IPv4Address::UNSPECIFIED_ADDRESS;

    simtime_t now = simTime();
    for (const auto & elem : arpCache)
        if (elem.second->macAddress == macAddr && elem.second->lastUpdate + cacheTimeout >= now)
            return elem.first;


    return IPv4Address::UNSPECIFIED_ADDRESS;
}

} // namespace inet

