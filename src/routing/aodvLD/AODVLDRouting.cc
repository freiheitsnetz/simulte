//
// Copyright (C) 2014 OpenSim Ltd.
// Author: Benjamin Seregi
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

#include <AODVLDRouting.h>
#include "inet/networklayer/ipv4/ICMPMessage.h"
#include "inet/networklayer/ipv4/IPv4Route.h"

#ifdef WITH_IDEALWIRELESS
#include "inet/linklayer/ideal/IdealMacFrame_m.h"
#endif // ifdef WITH_IDEALWIRELESS

#ifdef WITH_IEEE80211
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#endif // ifdef WITH_IEEE80211

#ifdef WITH_CSMA
#include "inet/linklayer/csma/CSMAFrame_m.h"
#endif // ifdef WITH_CSMA

#ifdef WITH_CSMACA
#include "inet/linklayer/csmaca/CsmaCaMacFrame_m.h"
#endif // ifdef WITH_CSMA

#ifdef WITH_LMAC
#include "inet/linklayer/lmac/LMacFrame_m.h"
#endif // ifdef WITH_LMAC

#ifdef WITH_BMAC
#include "inet/linklayer/bmac/BMacFrame_m.h"
#endif // ifdef WITH_BMAC

//TODO includes
//#ifdef WITH_LTE
//#endif
#include <LteAirFrame.h>
#include "inet/networklayer/common/IPSocket.h"
#include "inet/transportlayer/contract/udp/UDPControlInfo.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/lifecycle/NodeOperations.h"
#include "L3AddressResolver.h"

//Delete after debuggin
//#include <iostream>
//#include <fstream>


Define_Module(AODVLDRouting);
namespace inet {


void AODVLDRouting::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        lastBroadcastTime = SIMTIME_ZERO;
        rebootTime = SIMTIME_ZERO;
        rreqId = sequenceNum = 0;
        rreqCount = rerrCount = 0;
        host = getContainingNode(this);
        routingTable = getModuleFromPar<IRoutingTable>(par("routingTableModule"), this);
        interfaceTable = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
        networkProtocol = getModuleFromPar<INetfilter>(par("networkProtocolModule"), this);


        aodvLDUDPPort = par("udpPort");
        askGratuitousRREP = par("askGratuitousRREP");
        useHelloMessages = par("useHelloMessages");
        activeRouteTimeout = par("activeRouteTimeout");
        helloInterval = par("helloInterval");
        allowedHelloLoss = par("allowedHelloLoss");
        netDiameter = par("netDiameter");
        nodeTraversalTime = par("nodeTraversalTime");
        rerrRatelimit = par("rerrRatelimit");
        rreqRetries = par("rreqRetries");
        rreqRatelimit = par("rreqRatelimit");
        timeoutBuffer = par("timeoutBuffer");
        ttlStart = par("ttlStart");
        ttlIncrement = par("ttlIncrement");
        ttlThreshold = par("ttlThreshold");
        localAddTTL = par("localAddTTL");
        jitterPar = &par("jitter");
        periodicJitter = &par("periodicJitter");

        myRouteTimeout = par("myRouteTimeout");
        deletePeriod = par("deletePeriod");
        blacklistTimeout = par("blacklistTimeout");
        netTraversalTime = par("netTraversalTime");
        nextHopWait = par("nextHopWait");
        pathDiscoveryTime = par("pathDiscoveryTime");
        RREQCollectionTimeMin = par("RREQCollectionTimeMin");
        RREQCollectionTimeMax = par("RREQCollectionTimeMax");
        RREQTimerHopDependeny =par("RREQTimerHopDependeny").boolValue();
        DestinationAddress =L3AddressResolver().resolve(par("DestinationAddress").stringValue());


        //statistics
        WATCH(firstRREPArrives);
        WATCH(numHops);

        numRREQsent = registerSignal("numRREQsent");
        routeAvailability = registerSignal("routeAvailability");
        interRREQRREPTime = registerSignal("interRREQRREPTime");
        numFinalHops = registerSignal("numFinalHops");
        numRREQForwarded=registerSignal("numRREQForwarded");
        numSentRERR=registerSignal("numSentRERR");
        numReceivedRERR=registerSignal("numReceivedRERR");
        interRREPRouteDiscoveryTime=registerSignal("interRREPRouteDiscoveryTime");
        newSeqNum=registerSignal("newSeqNum");
        theoreticalRL=registerSignal("theoreticalRL");





    }
    else if (stage == INITSTAGE_ROUTING_PROTOCOLS) {
        metrikmodule = getModuleFromPar<ResidualLinklifetime>(par("residualLinkLifetimeModule"),this);
        NodeStatus *nodeStatus = dynamic_cast<NodeStatus *>(host->getSubmodule("status"));
        isOperational = !nodeStatus || nodeStatus->getState() == NodeStatus::UP;

        addressType = getSelfIPAddress().getAddressType();
        IPSocket socket(gate("ipOut"));
        socket.registerProtocol(IP_PROT_MANET);
        networkProtocol->registerHook(0, this);
        host->subscribe(NF_LINK_BREAK, this);


        if (useHelloMessages) {
            helloMsgTimer = new cMessage("HelloMsgTimer");

            // RFC 5148:
            // Jitter SHOULD be applied by reducing this delay by a random amount, so that
            // the delay between consecutive transmissions of messages of the same type is
            // equal to (MESSAGE_INTERVAL - jitter), where jitter is the random value.
            if (isOperational)
                scheduleAt(simTime() + helloInterval - periodicJitter->doubleValue(), helloMsgTimer);
        }

        expungeTimer = new cMessage("ExpungeTimer");
        counterTimer = new cMessage("CounterTimer");
        rrepAckTimer = new cMessage("RREPACKTimer");
        blacklistTimer = new cMessage("BlackListTimer");
        updateTimer = new cMessage("updateTimer");
        scheduleAt(simTime() + 0.1, updateTimer);

        if (isOperational)
            scheduleAt(simTime() + 1, counterTimer);
    }
}

void AODVLDRouting::handleMessage(cMessage *msg)
{
    if (!isOperational) {
        if (msg->isSelfMessage())
            throw cRuntimeError("Model error: self msg '%s' received when isOperational is false", msg->getName());

        EV_ERROR << "Application is turned off, dropping '" << msg->getName() << "' message\n";
        delete msg;
        return;
    }

    if (msg->isSelfMessage()) {
        if (dynamic_cast<WaitForRREP *>(msg))
            handleWaitForRREP((WaitForRREP *)msg);
        else if (msg == helloMsgTimer)
            sendHelloMessagesIfNeeded();
        else if (msg == expungeTimer)
            expungeRoutes();
        else if (msg == counterTimer) {
            rreqCount = rerrCount = 0;
            scheduleAt(simTime() + 1, counterTimer);
        }
        else if (msg == rrepAckTimer)
            handleRREPACKTimer();
        else if (msg == blacklistTimer)
            handleBlackListTimer();
        else if (dynamic_cast<WaitForRREQ *>(msg))
            handleRREQ((WaitForRREQ *)msg);
        else if (msg ==updateTimer){
            std::string temp= DestinationAddress.str();
            /*L3Address tmp = routingTable->getNextHopForDestination(DestinationAddress);
            simtime_t timestamp=simTime();
            scheduleAt(simTime() + 0.1, updateTimer);
            if(tmp.isUnspecified()){
            cTimestampedValue tmp(timestamp, 0.0);
            emit(routeAvailability,&tmp);
            }
            else{
            cTimestampedValue tmp(timestamp, 1.0);
            emit(routeAvailability,&tmp);
        }
        */
            IRoute* temproute=routingTable->findBestMatchingRoute(DestinationAddress);

            AODVLDRouteData *tempdata = temproute? dynamic_cast<AODVLDRouteData *>(temproute->getProtocolData()) : nullptr;
            if(tempdata!=nullptr){
            simtime_t timestamp=simTime();

            scheduleAt(simTime() + 0.1, updateTimer);

            if(tempdata->isActive()){
            cTimestampedValue tmp(timestamp, 1.0);
            emit(routeAvailability,&tmp);
            }
            else{
            cTimestampedValue tmp(timestamp, 0.0);
            emit(routeAvailability,&tmp);
        }

        }

        else{
            simtime_t timestamp=simTime();
            cTimestampedValue tmp(timestamp, 0.0);
            emit(routeAvailability,&tmp);
            scheduleAt(simTime() + 0.1, updateTimer);
        }
        }
        else
            throw cRuntimeError("Unknown self message");
    }
    else if (ICMPMessage *icmpPacket = dynamic_cast<ICMPMessage *>(msg)) {
        // ICMP packet arrived, dropped
        delete icmpPacket;
    }
    else {
        UDPPacket *udpPacket = check_and_cast<UDPPacket *>(msg);
        AODVLDControlPacket *ctrlPacket = check_and_cast<AODVLDControlPacket *>(udpPacket->decapsulate());
        INetworkProtocolControlInfo *udpProtocolCtrlInfo = check_and_cast<INetworkProtocolControlInfo *>(udpPacket->getControlInfo());
        L3Address sourceAddr = udpProtocolCtrlInfo->getSourceAddress();
        unsigned int arrivalPacketTTL = udpProtocolCtrlInfo->getHopLimit();

        switch (ctrlPacket->getPacketType()) {
            case RREQ:
                prehandleRREQ(check_and_cast<AODVLDRREQ *>(ctrlPacket), sourceAddr, arrivalPacketTTL);
                break;

            case RREP:
                handleRREP(check_and_cast<AODVLDRREP *>(ctrlPacket), sourceAddr);
                break;

            case RERR:
                handleRERR(check_and_cast<AODVLDRERR *>(ctrlPacket), sourceAddr);
                break;

            case RREPACK:
                handleRREPACK(check_and_cast<AODVLDRREPACK *>(ctrlPacket), sourceAddr);
                break;

            default:
                throw cRuntimeError("AODVLD Control Packet arrived with undefined packet type: %d", ctrlPacket->getPacketType());
        }
        delete udpPacket;
    }
}

INetfilter::IHook::Result AODVLDRouting::ensureRouteForDatagram(INetworkDatagram *datagram)
{
    Enter_Method("datagramPreRoutingHook");
    const L3Address& destAddr = datagram->getDestinationAddress();
    const L3Address& sourceAddr = datagram->getSourceAddress();

    if (destAddr.isBroadcast() || routingTable->isLocalAddress(destAddr) || destAddr.isMulticast())
        return ACCEPT;
    else {
        EV_INFO << "Finding route for source " << sourceAddr << " with destination " << destAddr << endl;
        IRoute *route = routingTable->findBestMatchingRoute(destAddr);
        AODVLDRouteData *routeData = route ? dynamic_cast<AODVLDRouteData *>(route->getProtocolData()) : nullptr;
        bool isActive = routeData && routeData->isActive();
        if (isActive && !route->getNextHopAsGeneric().isUnspecified()) {
            EV_INFO << "Active route found: " << route << endl;

            // Each time a route is used to forward a data packet, its Active Route
            // Lifetime field of the source, destination and the next hop on the
            // path to the destination is updated to be no less than the current
            // time plus ACTIVE_ROUTE_TIMEOUT.

            updateValidRouteLifeTime(destAddr, simTime() + activeRouteTimeout);
            updateValidRouteLifeTime(route->getNextHopAsGeneric(), simTime() + activeRouteTimeout);

            return ACCEPT;
        }
        else if (sourceAddr.isUnspecified() || routingTable->isLocalAddress(sourceAddr)) {
            bool isInactive = routeData && !routeData->isActive();
            // A node disseminates a RREQ when it determines that it needs a route
            // to a destination and does not have one available.  This can happen if
            // the destination is previously unknown to the node, or if a previously
            // valid route to the destination expires or is marked as invalid.

            EV_INFO << (isInactive ? "Inactive" : "Missing") << " route for destination " << destAddr << endl;

            delayDatagram(datagram);

            if (!hasOngoingRouteDiscovery(destAddr)) {
                // When a new route to the same destination is required at a later time
                // (e.g., upon route loss), the TTL in the RREQ IP header is initially
                // set to the Hop Count plus TTL_INCREMENT.



                if (isInactive)
                    startRouteDiscovery(destAddr, route->getMetric() + ttlIncrement);
                else
                    startRouteDiscovery(destAddr);
            }
            else
                EV_DETAIL << "Route discovery is in progress, originator " << getSelfIPAddress() << " target " << destAddr << endl;

            return QUEUE;
        }
        else
            return ACCEPT;
    }
}

AODVLDRouting::AODVLDRouting()
{
}

bool AODVLDRouting::hasOngoingRouteDiscovery(const L3Address& target)
{
    return waitForRREPTimers.find(target) != waitForRREPTimers.end();
}

void AODVLDRouting::startRouteDiscovery(const L3Address& target, unsigned timeToLive)
{
    EV_INFO << "Starting route discovery with originator " << getSelfIPAddress() << " and destination " << target << endl;
    ASSERT(!hasOngoingRouteDiscovery(target));
    AODVLDRREQ *rreq = createRREQ(target);
    addressToRreqRetries[target] = 0;

    simtime_t timestamp=simTime();
    double interTime=simTime().dbl()-RREP_Arrival_timestamp.dbl();
    cTimestampedValue tmp(timestamp, interTime);
    emit(interRREPRouteDiscoveryTime,&tmp);

    RREQsent= simTime();
    sendRREQ(rreq, addressType->getBroadcastAddress(), timeToLive);


}

L3Address AODVLDRouting::getSelfIPAddress() const
{
    return routingTable->getRouterIdAsGeneric();
}

void AODVLDRouting::delayDatagram(INetworkDatagram *datagram)
{
    EV_DETAIL << "Queuing datagram, source " << datagram->getSourceAddress() << ", destination " << datagram->getDestinationAddress() << endl;
    const L3Address& target = datagram->getDestinationAddress();
    targetAddressToDelayedPackets.insert(std::pair<L3Address, INetworkDatagram *>(target, datagram));
}

void AODVLDRouting::sendRREQ(AODVLDRREQ *rreq, const L3Address& destAddr, unsigned int timeToLive)
{


    // In an expanding ring search, the originating node initially uses a TTL =
    // TTL_START in the RREQ packet IP header and sets the timeout for
    // receiving a RREP to RING_TRAVERSAL_TIME milliseconds.
    // RING_TRAVERSAL_TIME is calculated as described in section 10.  The
    // TTL_VALUE used in calculating RING_TRAVERSAL_TIME is set equal to the
    // value of the TTL field in the IP header.  If the RREQ times out
    // without a corresponding RREP, the originator broadcasts the RREQ
    // again with the TTL incremented by TTL_INCREMENT.  This continues
    // until the TTL set in the RREQ reaches TTL_THRESHOLD, beyond which a
    // TTL = NET_DIAMETER is used for each attempt.

    if (rreqCount >= rreqRatelimit) {
        EV_WARN << "A node should not originate more than RREQ_RATELIMIT RREQ messages per second. Canceling sending RREQ" << endl;
        delete rreq;
        return;
    }

    auto rrepTimer = waitForRREPTimers.find(rreq->getDestAddr());
    WaitForRREP *rrepTimerMsg = nullptr;
    if (rrepTimer != waitForRREPTimers.end()) {
        rrepTimerMsg = rrepTimer->second;
        unsigned int lastTTL = rrepTimerMsg->getLastTTL();
        rrepTimerMsg->setDestAddr(rreq->getDestAddr());

        // The Hop Count stored in an invalid routing table entry indicates the
        // last known hop count to that destination in the routing table.  When
        // a new route to the same destination is required at a later time
        // (e.g., upon route loss), the TTL in the RREQ IP header is initially
        // set to the Hop Count plus TTL_INCREMENT.  Thereafter, following each
        // timeout the TTL is incremented by TTL_INCREMENT until TTL =
        // TTL_THRESHOLD is reached.  Beyond this TTL = NET_DIAMETER is used.
        // Once TTL = NET_DIAMETER, the timeout for waiting for the RREP is set
        // to NET_TRAVERSAL_TIME, as specified in section 6.3.

        if (timeToLive != 0) {
            rrepTimerMsg->setLastTTL(timeToLive);
            rrepTimerMsg->setFromInvalidEntry(true);
            cancelEvent(rrepTimerMsg);
        }
        else if (lastTTL + ttlIncrement < ttlThreshold) {
            ASSERT(!rrepTimerMsg->isScheduled());
            timeToLive = lastTTL + ttlIncrement;
            rrepTimerMsg->setLastTTL(lastTTL + ttlIncrement);
        }
        else {
            ASSERT(!rrepTimerMsg->isScheduled());
            timeToLive = netDiameter;
            rrepTimerMsg->setLastTTL(netDiameter);
        }
    }
    else {
        rrepTimerMsg = new WaitForRREP();
        waitForRREPTimers[rreq->getDestAddr()] = rrepTimerMsg;
        ASSERT(hasOngoingRouteDiscovery(rreq->getDestAddr()));

        timeToLive = ttlStart;
        rrepTimerMsg->setLastTTL(ttlStart);
        rrepTimerMsg->setFromInvalidEntry(false);
        rrepTimerMsg->setDestAddr(rreq->getDestAddr());
    }

    // Each time, the timeout for receiving a RREP is RING_TRAVERSAL_TIME.
    simtime_t ringTraversalTime = 2.0 * nodeTraversalTime * (timeToLive + timeoutBuffer);
    scheduleAt(simTime() + ringTraversalTime, rrepTimerMsg);

    simtime_t timestamp=simTime();
    cTimestampedValue tmp(timestamp, 1.0);
    emit(numRREQsent,&tmp);

    EV_INFO << "Sending a Route Request with target " << rreq->getDestAddr() << " and TTL= " << timeToLive << endl;
    sendAODVLDPacket(rreq, destAddr, timeToLive, jitterPar->doubleValue());
    rreqCount++;
}

void AODVLDRouting::sendRREP(AODVLDRREP *rrep, const L3Address& destAddr, unsigned int timeToLive)
{



    EV_INFO << "Sending Route Reply to " << destAddr << endl;

    // When any node transmits a RREP, the precursor list for the
    // corresponding destination node is updated by adding to it
    // the next hop node to which the RREP is forwarded.

    IRoute *destRoute = routingTable->findBestMatchingRoute(destAddr);
    const L3Address& nextHop = destRoute->getNextHopAsGeneric();
    AODVLDRouteData *destRouteData = check_and_cast<AODVLDRouteData *>(destRoute->getProtocolData());
    destRouteData->addPrecursor(nextHop);

    // The node we received the Route Request for is our neighbor,
    // it is probably an unidirectional link
    if (destRoute->getMetric() == 1) {
        // It is possible that a RREP transmission may fail, especially if the
        // RREQ transmission triggering the RREP occurs over a unidirectional
        // link.

        rrep->setAckRequiredFlag(true);

        // when a node detects that its transmission of a RREP message has failed,
        // it remembers the next-hop of the failed RREP in a "blacklist" set.

        failedNextHop = nextHop;

        if (rrepAckTimer->isScheduled())
            cancelEvent(rrepAckTimer);

        scheduleAt(simTime() + nextHopWait, rrepAckTimer);
    }
    sendAODVLDPacket(rrep, nextHop, timeToLive, 0);
}

AODVLDRREQ *AODVLDRouting::createRREQ(const L3Address& destAddr)
{
    AODVLDRREQ *rreqPacket = new AODVLDRREQ("AODVLD-RREQ");

    rreqPacket->setGratuitousRREPFlag(askGratuitousRREP);
    IRoute *lastKnownRoute = routingTable->findBestMatchingRoute(destAddr);

    rreqPacket->setPacketType(RREQ);

    // The Originator Sequence Number in the RREQ message is the
    // node's own sequence number, which is incremented prior to
    // insertion in a RREQ.
    sequenceNum++;
    simtime_t timestamp=simTime();
    cTimestampedValue tmp(timestamp, 0.0);
    emit(interRREQRREPTime,&tmp);

    rreqPacket->setOriginatorSeqNum(sequenceNum);

    if (lastKnownRoute && lastKnownRoute->getSource() == this) {
        // The Destination Sequence Number field in the RREQ message is the last
        // known destination sequence number for this destination and is copied
        // from the Destination Sequence Number field in the routing table.

        AODVLDRouteData *routeData = check_and_cast<AODVLDRouteData *>(lastKnownRoute->getProtocolData());
        if (routeData && routeData->hasValidDestNum()) {
            rreqPacket->setDestSeqNum(routeData->getDestSeqNum());
            rreqPacket->setUnknownSeqNumFlag(false);
        }
        else
            rreqPacket->setUnknownSeqNumFlag(true);
    }
    else
        rreqPacket->setUnknownSeqNumFlag(true); // If no sequence number is known, the unknown sequence number flag MUST be set.

    rreqPacket->setOriginatorAddr(getSelfIPAddress());
    rreqPacket->setDestAddr(destAddr);

    // The RREQ ID field is incremented by one from the last RREQ ID used
    // by the current node. Each node maintains only one RREQ ID.
    rreqId++;
    rreqPacket->setRreqId(rreqId);

    // The Hop Count field is set to zero.
    rreqPacket->setHopCount(0);

    // Before broadcasting the RREQ, the originating node buffers the RREQ
    // ID and the Originator IP address (its own address) of the RREQ for
    // PATH_DISCOVERY_TIME.
    // In this way, when the node receives the packet again from its neighbors,
    // it will not reprocess and re-forward the packet.

    /*RREQIdentifier rreqIdentifier(getSelfIPAddress(), rreqId);
    rreqsArrivalTime[rreqIdentifier] = simTime();*/
    rreqPacket->setByteLength(24);
    rreqPacket->setResidualLinklifetime(1000);
    return rreqPacket;
}

AODVLDRREP *AODVLDRouting::createRREP(AODVLDRREQ *rreq, IRoute *destRoute, IRoute *originatorRoute, const L3Address& lastHopAddr)
{
    AODVLDRREP *rrep = new AODVLDRREP("AODVLD-RREP");
    rrep->setPacketType(RREP);

    // When generating a RREP message, a node copies the Destination IP
    // Address and the Originator Sequence Number from the RREQ message into
    // the corresponding fields in the RREP message.

    rrep->setDestAddr(rreq->getDestAddr());
    rrep->setOriginatorSeqNum(rreq->getOriginatorSeqNum());

    // OriginatorAddr = The IP address of the node which originated the RREQ
    // for which the route is supplied.
    rrep->setOriginatorAddr(rreq->getOriginatorAddr());

    // Processing is slightly different, depending on whether the node is
    // itself the requested destination (see section 6.6.1), or instead
    // if it is an intermediate node with an fresh enough route to the destination
    // (see section 6.6.2).

    if (rreq->getDestAddr() == getSelfIPAddress()) {    // node is itself the requested destination
        // If the generating node is the destination itself, it MUST increment
        // its own sequence number by one if the sequence number in the RREQ
        // packet is equal to that incremented value.

        if (!rreq->getUnknownSeqNumFlag() && sequenceNum + 1 == rreq->getDestSeqNum()){
            sequenceNum++;
            simtime_t timestamp=simTime();
            cTimestampedValue tmp(timestamp, 0.0);
            emit(interRREQRREPTime,&tmp);

        }


        // The destination node places its (perhaps newly incremented)
        // sequence number into the Destination Sequence Number field of
        // the RREP,
        rrep->setDestSeqNum(sequenceNum);

        // and enters the value zero in the Hop Count field
        // of the RREP.
        rrep->setHopCount(0);

        // The destination node copies the value MY_ROUTE_TIMEOUT
        // into the Lifetime field of the RREP.

        rrep->setLifeTime(myRouteTimeout);
        rrep->setResidualRouteLifetime(rreq->getResidualLinklifetime()+simTime());
    }
    //TODO For now: Don't allow an intermediate node to return a rrep
    /*else {    // intermediate node
              // it copies its known sequence number for the destination into
              // the Destination Sequence Number field in the RREP message.
        AODVLDRouteData *destRouteData = check_and_cast<AODVLDRouteData *>(destRoute->getProtocolData());
        AODVLDRouteData *originatorRouteData = check_and_cast<AODVLDRouteData *>(originatorRoute->getProtocolData());
        rrep->setDestSeqNum(destRouteData->getDestSeqNum());

        // The intermediate node updates the forward route entry by placing the
        // last hop node (from which it received the RREQ, as indicated by the
        // source IP address field in the IP header) into the precursor list for
        // the forward route entry -- i.e., the entry for the Destination IP
        // Address.
        destRouteData->addPrecursor(lastHopAddr);

        // The intermediate node also updates its route table entry
        // for the node originating the RREQ by placing the next hop towards the
        // destination in the precursor list for the reverse route entry --
        // i.e., the entry for the Originator IP Address field of the RREQ
        // message data.

        originatorRouteData->addPrecursor(destRoute->getNextHopAsGeneric());

        // The intermediate node places its distance in hops from the
        // destination (indicated by the hop count in the routing table)
        // Hop Count field in the RREP.

        rrep->setHopCount(destRoute->getMetric());

        // The Lifetime field of the RREP is calculated by subtracting the
        // current time from the expiration time in its route table entry.

        rrep->setLifeTime(destRouteData->getLifeTime() - simTime());
    }
*/
    rrep->setByteLength(20);
    return rrep;
}

AODVLDRREP *AODVLDRouting::createGratuitousRREP(AODVLDRREQ *rreq, IRoute *originatorRoute)
{
    ASSERT(originatorRoute != nullptr);
    AODVLDRREP *grrep = new AODVLDRREP("AODVLD-GRREP");
    AODVLDRouteData *routeData = check_and_cast<AODVLDRouteData *>(originatorRoute->getProtocolData());

    // Hop Count                        The Hop Count as indicated in the
    //                                  node's route table entry for the
    //                                  originator
    //
    // Destination IP Address           The IP address of the node that
    //                                  originated the RREQ
    //
    // Destination Sequence Number      The Originator Sequence Number from
    //                                  the RREQ
    //
    // Originator IP Address            The IP address of the Destination
    //                                  node in the RREQ
    //
    // Lifetime                         The remaining lifetime of the route
    //                                  towards the originator of the RREQ,
    //                                  as known by the intermediate node.

    grrep->setPacketType(RREP);
    grrep->setHopCount(originatorRoute->getMetric());
    grrep->setDestAddr(rreq->getOriginatorAddr());
    grrep->setDestSeqNum(rreq->getOriginatorSeqNum());
    grrep->setOriginatorAddr(rreq->getDestAddr());
    grrep->setLifeTime(routeData->getLifeTime());

    grrep->setByteLength(20);
    return grrep;
}

void AODVLDRouting::handleRREP(AODVLDRREP *rrep, const L3Address& sourceAddr)
{

    EV_INFO << "AODVLD Route Reply arrived with source addr: " << sourceAddr << " originator addr: " << rrep->getOriginatorAddr()
            << " destination addr: " << rrep->getDestAddr() << endl;

    if (rrep->getOriginatorAddr().isUnspecified()) {
        EV_INFO << "This Route Reply is a Hello Message" << endl;
        handleHelloMessage(rrep);
        delete rrep;
        return;
    }
    // When a node receives a RREP message, it searches (using longest-
    // prefix matching) for a route to the previous hop.

    // If needed, a route is created for the previous hop,
    // but without a valid sequence number (see section 6.2)



    IRoute *previousHopRoute = routingTable->findBestMatchingRoute(sourceAddr);

    if (!previousHopRoute || previousHopRoute->getSource() != this) {
        // create without valid sequence number
        previousHopRoute = createRoute(sourceAddr, sourceAddr, 1, false, rrep->getOriginatorSeqNum(), true, simTime() + activeRouteTimeout,simTime() + rrep->getResidualRouteLifetime());
    }
    else
        updateRoutingTable(previousHopRoute, sourceAddr, 1, false, rrep->getOriginatorSeqNum(), true, simTime() + activeRouteTimeout,simTime() + rrep->getResidualRouteLifetime());

    // Next, the node then increments the hop count value in the RREP by one,
    // to account for the new hop through the intermediate node
    unsigned int newHopCount = rrep->getHopCount() + 1;
    /*
     * Metric here is the residualRouteLifetime which is the minRLL of all links
     * First idea: Use minRLL calculated in RREQ phase. When dest. generates RREP it adds the minRLL estimated in RREQ to RREP
     * So RREP contains minRLL of ROUTE not of LINK.
     *
     * ISSUE: RREP might not take same way back as RREQ due to Route change when better RREQ appear at a node.
     * Meaning the minRLL is for the older route.
     * Since the older route is definitely worse than the new route, the minRLL is set too low,
     * which is not a problem and corrected by later appearing RREP
     */


    rrep->setHopCount(newHopCount);

    // Then the forward route for this destination is created if it does not
    // already exist.

    IRoute *destRoute = routingTable->findBestMatchingRoute(rrep->getDestAddr());
    AODVLDRouteData *destRouteData = nullptr;
    simtime_t lifeTime = rrep->getLifeTime();
    simtime_t newResidualRouteLifetime =rrep->getResidualRouteLifetime();
    unsigned int destSeqNum = rrep->getDestSeqNum();

    if (destRoute && destRoute->getSource() == this) {    // already exists
        destRouteData = check_and_cast<AODVLDRouteData *>(destRoute->getProtocolData());
        // Upon comparison, the existing entry is updated only in the following circumstances:

        // (i) the sequence number in the routing table is marked as
        //     invalid in route table entry.

        if (!destRouteData->hasValidDestNum()) {
            updateRoutingTable(destRoute, sourceAddr, newHopCount, true, destSeqNum, true, simTime() + lifeTime, simTime()+newResidualRouteLifetime);

            // If the route table entry to the destination is created or updated,
            // then the following actions occur:
            //
            // -  the route is marked as active,
            //
            // -  the destination sequence number is marked as valid,
            //
            // -  the next hop in the route entry is assigned to be the node from
            //    which the RREP is received, which is indicated by the source IP
            //    address field in the IP header,
            //
            // -  the hop count is set to the value of the New Hop Count,
            //
            // -  the expiry time is set to the current time plus the value of the
            //    Lifetime in the RREP message,
            //
            // -  and the destination sequence number is the Destination Sequence
            //    Number in the RREP message.
        }
        // (ii) the Destination Sequence Number in the RREP is greater than
        //      the node's copy of the destination sequence number and the
        //      known value is valid, or
        else if (destSeqNum > destRouteData->getDestSeqNum()) {
            updateRoutingTable(destRoute, sourceAddr, newHopCount, true, destSeqNum, true, simTime() + lifeTime,simTime()+newResidualRouteLifetime);
        }
        else {
            // (iii) the sequence numbers are the same, but the route is
            //       marked as inactive, or
            if (destSeqNum == destRouteData->getDestSeqNum() && !destRouteData->isActive()) {
                updateRoutingTable(destRoute, sourceAddr, newHopCount, true, destSeqNum, true, simTime() + lifeTime,simTime()+newResidualRouteLifetime);
            }
            // (iv) the sequence numbers are the same, and the New Hop Count is
            //      smaller than the hop count in route table entry.

            /*TODO Deactivate? same sequence number but smaller hop count is not the metric for this*/
            //CHANGE: METRIC is now residualRouteLifetime, which is the minRLL calculated from RREQ phase
            //Meaning: When same seq#: newResidualLinklifeTime must be higher than old to update route information
            else if (destSeqNum == destRouteData->getDestSeqNum() && (simTime()+newResidualRouteLifetime) > destRouteData->getResidualRouteLifetime()) {
                updateRoutingTable(destRoute, sourceAddr, newHopCount, true, destSeqNum, true, simTime() + lifeTime,simTime()+newResidualRouteLifetime);
            }
        }
    }
    else {    // create forward route for the destination: this path will be used by the originator to send data packets
        destRoute = createRoute(rrep->getDestAddr(), sourceAddr, newHopCount, true, destSeqNum, true, simTime() + lifeTime, simTime() + newResidualRouteLifetime);
        destRouteData = check_and_cast<AODVLDRouteData *>(destRoute->getProtocolData());
    }

    // If the current node is not the node indicated by the Originator IP
    // Address in the RREP message AND a forward route has been created or
    // updated as described above, the node consults its route table entry
    // for the originating node to determine the next hop for the RREP
    // packet, and then forwards the RREP towards the originator using the
    // information in that route table entry.

    IRoute *originatorRoute = routingTable->findBestMatchingRoute(rrep->getOriginatorAddr());
    if (getSelfIPAddress() != rrep->getOriginatorAddr()) {
        // If a node forwards a RREP over a link that is likely to have errors or
        // be unidirectional, the node SHOULD set the 'A' flag to require that the
        // recipient of the RREP acknowledge receipt of the RREP by sending a RREP-ACK
        // message back (see section 6.8).

        if (originatorRoute && originatorRoute->getSource() == this) {
            AODVLDRouteData *originatorRouteData = check_and_cast<AODVLDRouteData *>(originatorRoute->getProtocolData());

            // Also, at each node the (reverse) route used to forward a
            // RREP has its lifetime changed to be the maximum of (existing-
            // lifetime, (current time + ACTIVE_ROUTE_TIMEOUT).

            simtime_t existingLifeTime = originatorRouteData->getLifeTime();
            originatorRouteData->setLifeTime(std::max(simTime() + activeRouteTimeout, existingLifeTime));

            if (simTime() > rebootTime + deletePeriod || rebootTime == 0) {
                // If a node forwards a RREP over a link that is likely to have errors
                // or be unidirectional, the node SHOULD set the 'A' flag to require that
                // the recipient of the RREP acknowledge receipt of the RREP by sending a
                // RREP-ACK message back (see section 6.8).

                if (rrep->getAckRequiredFlag()) {
                    AODVLDRREPACK *rrepACK = createRREPACK();
                    sendRREPACK(rrepACK, sourceAddr);
                    rrep->setAckRequiredFlag(false);
                }

                // When any node transmits a RREP, the precursor list for the
                // corresponding destination node is updated by adding to it
                // the next hop node to which the RREP is forwarded.

                destRouteData->addPrecursor(originatorRoute->getNextHopAsGeneric());

                // Finally, the precursor list for the next hop towards the
                // destination is updated to contain the next hop towards the
                // source (originator).

                IRoute *nextHopToDestRoute = routingTable->findBestMatchingRoute(destRoute->getNextHopAsGeneric());
                if (nextHopToDestRoute && nextHopToDestRoute->getSource() == this) {
                    AODVLDRouteData *nextHopToDestRouteData = check_and_cast<AODVLDRouteData *>(nextHopToDestRoute->getProtocolData());
                    nextHopToDestRouteData->addPrecursor(originatorRoute->getNextHopAsGeneric());
                }
                AODVLDRREP *outgoingRREP = rrep->dup();
                forwardRREP(outgoingRREP, originatorRoute->getNextHopAsGeneric(), 100);
            }
        }
        else
            EV_ERROR << "Reverse route doesn't exist. Dropping the RREP message" << endl;
    }
    else {
            updateRoutingTable(destRoute, sourceAddr, newHopCount, true, destSeqNum, true, simTime() + lifeTime, simTime() + newResidualRouteLifetime);
            if(rrep->getOriginatorSeqNum()== sequenceNum){
            RREP_Arrival_timestamp =simTime();
            double interRREQRREPTime_timestamp= simTime().dbl()-RREQsent.dbl();
            numHops= newHopCount;



            cTimestampedValue tmp1(RREP_Arrival_timestamp, interRREQRREPTime_timestamp);
            emit(interRREQRREPTime,&tmp1);
            cTimestampedValue tmp2(RREP_Arrival_timestamp, (double)numHops);
            emit(numFinalHops,&tmp2);
            cTimestampedValue tmp3(RREP_Arrival_timestamp, (double)rrep->getResidualRouteLifetime().dbl());
            emit(theoreticalRL,&tmp3);
            }
        if (hasOngoingRouteDiscovery(rrep->getDestAddr())) {
            EV_INFO << "The Route Reply has arrived for our Route Request to node " << rrep->getDestAddr() << endl;
            completeRouteDiscovery(rrep->getDestAddr());


        }
    }

    delete rrep;
}

void AODVLDRouting::updateRoutingTable(IRoute *route, const L3Address& nextHop, unsigned int hopCount, bool hasValidDestNum, unsigned int destSeqNum, bool isActive, simtime_t lifeTime, simtime_t residualRouteLifetime)
{
    EV_DETAIL << "Updating existing route: " << route << endl;

    route->setNextHop(nextHop);
    route->setMetric(hopCount);

    AODVLDRouteData *routingData = check_and_cast<AODVLDRouteData *>(route->getProtocolData());
    ASSERT(routingData != nullptr);

    routingData->setLifeTime(lifeTime);
    routingData->setDestSeqNum(destSeqNum);
    routingData->setIsActive(isActive);
    routingData->setHasValidDestNum(hasValidDestNum);
    routingData->setResidualRouteLifetime(residualRouteLifetime);

    EV_DETAIL << "Route updated: " << route << endl;

    scheduleExpungeRoutes();
}



void AODVLDRouting::sendAODVLDPacket(AODVLDControlPacket *packet, const L3Address& destAddr, unsigned int timeToLive, double delay)
{
    ASSERT(timeToLive != 0);

    INetworkProtocolControlInfo *networkProtocolControlInfo = addressType->createNetworkProtocolControlInfo();

    networkProtocolControlInfo->setHopLimit(timeToLive);

    networkProtocolControlInfo->setTransportProtocol(IP_PROT_MANET);
    networkProtocolControlInfo->setDestinationAddress(destAddr);
    networkProtocolControlInfo->setSourceAddress(getSelfIPAddress());

    // TODO: Implement: support for multiple interfaces
    InterfaceEntry *ifEntry = interfaceTable->getInterfaceByName("wlan");
    networkProtocolControlInfo->setInterfaceId(ifEntry->getInterfaceId());

    UDPPacket *udpPacket = new UDPPacket(packet->getName());
    udpPacket->encapsulate(packet);
    udpPacket->setSourcePort(aodvLDUDPPort);
    udpPacket->setDestinationPort(aodvLDUDPPort);
    udpPacket->setControlInfo(dynamic_cast<cObject *>(networkProtocolControlInfo));

    if (destAddr.isBroadcast())
        lastBroadcastTime = simTime();

    if (delay == 0)
        send(udpPacket, "ipOut");
    else
        sendDelayed(udpPacket, delay, "ipOut");
}

void AODVLDRouting::prehandleRREQ(AODVLDRREQ *rreq, const L3Address& sourceAddr, unsigned int timeToLive)
{
    EV_INFO << "AODVLD Route Request arrived with source addr: " << sourceAddr << " originator addr: " << rreq->getOriginatorAddr()
            << " destination addr: " << rreq->getDestAddr() << endl;

    // A node ignores all RREQs received from any node in its blacklist set.

    auto blackListIt = blacklist.find(sourceAddr);
    if (blackListIt != blacklist.end()) {
        EV_INFO << "The sender node " << sourceAddr << " is in our blacklist. Ignoring the Route Request" << endl;
        delete rreq;
        return;
    }



    //Check if the minRLL of the RREQ is higher than the last hop.
    simtime_t tempMetrik=metrikmodule->getMetrik(sourceAddr);
    if(rreq->getResidualLinklifetime()>tempMetrik){
        //The lower one has to be used. If true update the min RLL to to the last hop.
        rreq->setResidualLinklifetime (tempMetrik);
    }


    // When a node receives a RREQ, it first creates or updates a route to
    // the previous hop without a valid sequence number (see section 6.2).
    IRoute *previousHopRoute = routingTable->findBestMatchingRoute(sourceAddr);

    if (!previousHopRoute || previousHopRoute->getSource() != this) {
        // create without valid sequence number
        previousHopRoute = createRoute(sourceAddr, sourceAddr, 1, false, rreq->getOriginatorSeqNum(), true, simTime() + activeRouteTimeout,tempMetrik+simTime());
    }
    else
        updateRoutingTable(previousHopRoute, sourceAddr, 1, false, rreq->getOriginatorSeqNum(), true, simTime() + activeRouteTimeout,tempMetrik+simTime());


    RREQIdentifier  rreqIdentifier(rreq->getOriginatorAddr(), rreq->getRreqId());
    RREQAdditionalInfo rreqAddInfo;
    std::pair<RREQAdditionalInfo,AODVLDRREQ*> tmpPair;

    //auto checkRREQArrivalTime = rreqsArrivalTime.find(rreqIdentifier);


    /*//Check if there was a RREQ with the same ID from same source in the last "path discorvery time"
    //TODO Check if really needed
    if (checkRREQArrivalTime != rreqsArrivalTime.end() && simTime() - checkRREQArrivalTime->second <= pathDiscoveryTime){*/

    //Retry
    bool previouslyTransmitted=0; //Did we send the same RREQ already?
    bool currentBestExistend=0; //Is there a RREQ about to be sent already?
    std::map<const RREQIdentifier,std::pair<RREQAdditionalInfo,AODVLDRREQ*>,RREQIdentifierCompare>::iterator it;
    std::map<const RREQIdentifier,std::pair<RREQAdditionalInfo,AODVLDRREQ*>,RREQIdentifierCompare>::iterator it2;

    //New incoming RREQ has to have a lower RLL than same RREQ in both buffer. Only one entry can be found in each buffer
    for (auto i=LastTransmittedRREQ.begin(); i!=LastTransmittedRREQ.end();++i){
            if(i->first==rreqIdentifier){


                previouslyTransmitted = 1;
                it=i;
                break;
            }
    }
    for (auto i=CurrentBestRREQ.begin(); i!=CurrentBestRREQ.end();++i){
            if(i->first==rreqIdentifier)
                currentBestExistend=1;
            it2=i;
            break;
    }

     simtime_t tmpRLL=rreq->getResidualLinklifetime();
    //Compare

        if(previouslyTransmitted && currentBestExistend){
            if(it->second.second->getResidualLinklifetime() < tmpRLL){
                if(it2->second.second->getResidualLinklifetime()< tmpRLL){

                    rreqAddInfo.setSourceAddr(sourceAddr);
                    rreqAddInfo.setPacketTTL(timeToLive);
                    /* Put RREQIdentifier and RREQ into currentBestRREQ map*/
                    /* No Timer schedule needed because there is a previously transmitted RREQ
                     * Map size can be higher than one entry, because different RREQ from same and even different originator can occur
                     */
                    tmpPair.first= rreqAddInfo;
                    tmpPair.second= rreq;
                    CurrentBestRREQ[rreqIdentifier]=tmpPair;
                }
            }
        }
        else if(!previouslyTransmitted && currentBestExistend){
                if(it2->second.second->getResidualLinklifetime()< tmpRLL){
                    rreqAddInfo.setSourceAddr(sourceAddr);
                    rreqAddInfo.setPacketTTL(timeToLive);
                    /* Put RREQIdentifier and RREQ into currentBestRREQ map*/
                    /* No Timer schedule needed because there is a previously transmitted RREQ
                     * Map size can be higher than one entry, because different RREQ from same and even different originator can occur
                     *
                     */
                    tmpPair.first= rreqAddInfo;
                    tmpPair.second= rreq;
                    CurrentBestRREQ[rreqIdentifier]=tmpPair;

            }
        }

        else if(previouslyTransmitted && !currentBestExistend){
                if(it->second.second->getResidualLinklifetime()< tmpRLL){
                    rreqAddInfo.setSourceAddr(sourceAddr);
                    rreqAddInfo.setPacketTTL(timeToLive);
                    /* Put RREQIdentifier and RREQ into currentBestRREQ map*/
                    /*
                     * Map size can be higher than one entry, because different RREQ from same and even different originator can occur
                     * CAN POSSIBLE NOT HANDLE DIFFERENT RREQ AT THE SAME TIME
                     */
                     tmpPair.first= rreqAddInfo;
                     tmpPair.second= rreq;
                     CurrentBestRREQ[rreqIdentifier]=tmpPair;
                     rreqcollectionTimer.push_back(new WaitForRREQ("RREQCollectionTimer"));
                     rreqcollectionTimer.back()->setOriginatorAddr(rreq->getOriginatorAddr());
                     rreqcollectionTimer.back()->setRreqID(rreq->getRreqId());
                     if(RREQCollectionTimeMin==RREQCollectionTimeMax){
                         if(RREQTimerHopDependeny)
                             scheduleAt(simTime()+RREQCollectionTimeMin*rreq->getHopCount(),rreqcollectionTimer.back());
                         else
                             scheduleAt(simTime()+RREQCollectionTimeMin,rreqcollectionTimer.back());
                     }
                     else{
                         if(RREQTimerHopDependeny)
                             scheduleAt(simTime()+uniform(RREQCollectionTimeMin*rreq->getHopCount(),RREQCollectionTimeMax*rreq->getHopCount()),rreqcollectionTimer.back());
                         else
                             scheduleAt(simTime()+uniform(RREQCollectionTimeMin,RREQCollectionTimeMax),rreqcollectionTimer.back());
                     }
                    }
                }
        else if(!previouslyTransmitted && !currentBestExistend){

                    rreqAddInfo.setSourceAddr(sourceAddr);
                    rreqAddInfo.setPacketTTL(timeToLive);
                    /* Put RREQIdentifier and RREQ into currentBestRREQ map*/
                    /*
                     * Map size can be higher than one entry, because different RREQ from same and even different originator can occur
                     * CAN POSSIBLE NOT HANDLE DIFFERENT RREQ AT THE SAME TIME
                     */
                     tmpPair.first= rreqAddInfo;
                     tmpPair.second= rreq;
                     CurrentBestRREQ[rreqIdentifier]=tmpPair;
                     rreqcollectionTimer.push_back(new WaitForRREQ("RREQCollectionTimer"));
                     rreqcollectionTimer.back()->setOriginatorAddr(rreq->getOriginatorAddr());
                     rreqcollectionTimer.back()->setRreqID(rreq->getRreqId());
                     if(RREQCollectionTimeMin==RREQCollectionTimeMax){
                           if(RREQTimerHopDependeny)
                                 scheduleAt(simTime()+RREQCollectionTimeMin*rreq->getHopCount(),rreqcollectionTimer.back());
                           else
                           scheduleAt(simTime()+RREQCollectionTimeMin,rreqcollectionTimer.back());
                        }
                     else{
                           if(RREQTimerHopDependeny)
                                  scheduleAt(simTime()+uniform(RREQCollectionTimeMin*rreq->getHopCount(),RREQCollectionTimeMax*rreq->getHopCount()),rreqcollectionTimer.back());
                           else
                           scheduleAt(simTime()+uniform(RREQCollectionTimeMin,RREQCollectionTimeMax),rreqcollectionTimer.back());
                        }

                }


    //-------






}


 void AODVLDRouting::handleRREQ(WaitForRREQ *rreqTimer){


      //Find the RREQ whichs timer expired
     const RREQIdentifier tmpRREQidentifier(rreqTimer->getOriginatorAddr(),rreqTimer->getRreqID());
      for (auto it=CurrentBestRREQ.begin(); it!=CurrentBestRREQ.end();++it){
            if(it->first==tmpRREQidentifier){
                if(it==CurrentBestRREQ.end())
                    cRuntimeError("No rreq matching to timer");
      AODVLDRREQ *rreq=it->second.second;
      RREQAdditionalInfo rreqAddInfo=it->second.first;
      delete rreqTimer;


    // First, it first increments the hop count value in the RREQ by one, to
    // account for the new hop through the intermediate node.

    rreq->setHopCount(rreq->getHopCount() + 1);

    // Then the node searches for a reverse route to the Originator IP Address (see
    // section 6.2), using longest-prefix matching.

    IRoute *reverseRoute = routingTable->findBestMatchingRoute(rreq->getOriginatorAddr());

    // If need be, the route is created, or updated using the Originator Sequence Number from the
    // RREQ in its routing table.
    //
    // When the reverse route is created or updated, the following actions on
    // the route are also carried out:
    //
    //   1. the Originator Sequence Number from the RREQ is compared to the
    //      corresponding destination sequence number in the route table entry
    //      and copied if greater than the existing value there
    //
    //   2. the valid sequence number field is set to true;
    //
    //   3. the next hop in the routing table becomes the node from which the
    //      RREQ was received (it is obtained from the source IP address in
    //      the IP header and is often not equal to the Originator IP Address
    //      field in the RREQ message);
    //
    //   4. the hop count is copied from the Hop Count in the RREQ message;
    //
    //   Whenever a RREQ message is received, the Lifetime of the reverse
    //   route entry for the Originator IP address is set to be the maximum of
    //   (ExistingLifetime, MinimalLifetime), where
    //
    //   MinimalLifetime = (current time + 2*NET_TRAVERSAL_TIME - 2*HopCount*NODE_TRAVERSAL_TIME).

    unsigned int hopCount = rreq->getHopCount();
    simtime_t minimalLifeTime = simTime() + 2 * netTraversalTime - 2 * hopCount * nodeTraversalTime;
    simtime_t newLifeTime = std::max(simTime(), minimalLifeTime);
    int rreqSeqNum = rreq->getOriginatorSeqNum();
    if (!reverseRoute || reverseRoute->getSource() != this) {    // create
        // This reverse route will be needed if the node receives a RREP back to the
        // node that originated the RREQ (identified by the Originator IP Address).
        reverseRoute = createRoute(rreq->getOriginatorAddr(), it->second.first.getSourceAddr(), hopCount, true, rreqSeqNum, true, newLifeTime,simTime() + rreq->getResidualLinklifetime()+simTime());
    }
    else {
        AODVLDRouteData *routeData = check_and_cast<AODVLDRouteData *>(reverseRoute->getProtocolData());
        int routeSeqNum = routeData->getDestSeqNum();
        int newSeqNum = std::max(routeSeqNum, rreqSeqNum);


        /* THIS BLOCK IS OLD (Of normal AODV)
        int newHopCount = rreq->getHopCount();    // Note: already incremented by 1.
        int routeHopCount = reverseRoute->getMetric();


         *
        // The route is only updated if the new sequence number is either
        //
        //   (i)       higher than the destination sequence number in the route
        //             table, or
        //
        //   (ii)      the sequence numbers are equal, but the hop count (of the
        //             new information) plus one, is smaller than the existing hop
        //             count in the routing table, or
        //
        //   (iii)     the sequence number is unknown.

        if (rreqSeqNum > routeSeqNum ||
            (rreqSeqNum == routeSeqNum && newHopCount < routeHopCount) ||
            rreq->getUnknownSeqNumFlag())
        {
            updateRoutingTable(reverseRoute, it->first.getSourceAddr(), hopCount, true, newSeqNum, true, newLifeTime);
        }
    }
    */

    /*Since the function (handleRREQ(...)) is only called when a better route has been determined, the routing table must be updated every time */
    updateRoutingTable(reverseRoute, it->second.first.getSourceAddr(), hopCount, true, newSeqNum, true, newLifeTime,simTime() +rreq->getResidualLinklifetime()+simTime());

  }
    // A node generates a RREP if either:
    //
    // (i)       it is itself the destination, or
    //
    // (ii)      it has an active route to the destination, the destination
    //           sequence number in the node's existing route table entry
    //           for the destination is valid and greater than or equal to
    //           the Destination Sequence Number of the RREQ (comparison
    //           using signed 32-bit arithmetic), and the "destination only"
    //           ('D') flag is NOT set.

    // After a node receives a RREQ and responds with a RREP, it discards
    // the RREQ.  If the RREQ has the 'G' flag set, and the intermediate
    // node returns a RREP to the originating node, it MUST also unicast a
    // gratuitous RREP to the destination node.

    IRoute *destRoute = routingTable->findBestMatchingRoute(rreq->getDestAddr());
    AODVLDRouteData *destRouteData = destRoute ? dynamic_cast<AODVLDRouteData *>(destRoute->getProtocolData()) : nullptr;

    // check (i)
    if (rreq->getDestAddr() == getSelfIPAddress()) {
        EV_INFO << "I am the destination node for which the route was requested" << endl;

        // create RREP
        AODVLDRREP *rrep = createRREP(rreq, destRoute, reverseRoute, it->second.first.getSourceAddr());

        // send to the originator
        sendRREP(rrep, rreq->getOriginatorAddr(), 255);
        CurrentBestRREQ.erase(it);

        return;    // discard RREQ, in this case, we do not forward it.
    }

    // check (ii) //TODO do we really need this? Solution: Only send a RREP back if the RRL of the route to destination is higher,
    // than the minRLL contained in RREQ. RRL in RREP is then set to min RLL in RREQ. So only if links of previous nodes are the bottleneck,
    // use existing node
    if(getSelfIPAddress()!=rreq->getOriginatorAddr()){

    if (destRouteData && destRouteData->isActive() && destRouteData->hasValidDestNum() &&
        destRouteData->getDestSeqNum() >= rreq->getDestSeqNum()&&destRouteData->getResidualRouteLifetime()>rreq->getResidualLinklifetime())
    {
        EV_INFO << "I am an intermediate node who has information about a route to " << rreq->getDestAddr() << endl;

        if (destRoute->getNextHopAsGeneric() == it->second.first.getSourceAddr()) {
            EV_WARN << "This RREP would make a loop. Dropping it" << endl;
            CurrentBestRREQ.erase(it);

            return;
        }

        // create RREP
        AODVLDRREP *rrep = createRREP(rreq, destRoute, reverseRoute, it->second.first.getSourceAddr());

        // send to the originator
        sendRREP(rrep, rreq->getOriginatorAddr(), 255);

        if (rreq->getGratuitousRREPFlag()) {
            // The gratuitous RREP is then sent to the next hop along the path to
            // the destination node, just as if the destination node had already
            // issued a RREQ for the originating node and this RREP was produced in
            // response to that (fictitious) RREQ.

            IRoute *originatorRoute = routingTable->findBestMatchingRoute(rreq->getOriginatorAddr());
            AODVLDRREP *grrep = createGratuitousRREP(rreq, originatorRoute);
            sendGRREP(grrep, rreq->getDestAddr(), 100);
        }

        CurrentBestRREQ.erase(it);

        return;    // discard RREQ, in this case, we also do not forward it.
    }
    }

    // If a node does not generate a RREP (following the processing rules in
    // section 6.6), and if the incoming IP header has TTL larger than 1,
    // the node updates and broadcasts the RREQ to address 255.255.255.255
    // on each of its configured interfaces (see section 6.14).  To update
    // the RREQ, the TTL or hop limit field in the outgoing IP header is
    // decreased by one, and the Hop Count field in the RREQ message is
    // incremented by one, to account for the new hop through the
    // intermediate node. (!) Lastly, the Destination Sequence number for the
    // requested destination is set to the maximum of the corresponding
    // value received in the RREQ message, and the destination sequence
    // value currently maintained by the node for the requested destination.
    // However, the forwarding node MUST NOT modify its maintained value for
    // the destination sequence number, even if the value received in the
    // incoming RREQ is larger than the value currently maintained by the
    // forwarding node.

    if (it->second.first.getPacketTTL()> 0 && (simTime() > rebootTime + deletePeriod || rebootTime == 0)) {
        if (destRouteData)
            rreq->setDestSeqNum(std::max(destRouteData->getDestSeqNum(), rreq->getDestSeqNum()));
        rreq->setUnknownSeqNumFlag(false);


        AODVLDRREQ *outgoingRREQ = rreq->dup();
        forwardRREQ(outgoingRREQ, it->second.first.getPacketTTL());
        std::pair<RREQAdditionalInfo,AODVLDRREQ*> tmpPair(rreqAddInfo,rreq);
        LastTransmittedRREQ[tmpRREQidentifier]=tmpPair;



    }
    else
        EV_WARN << "Can't forward the RREQ because of its small (<= 1) TTL: " << it->second.first.getPacketTTL() << " or the AODVLD reboot has not completed yet" << endl;
    //delete rreq;

    CurrentBestRREQ.erase(it);



      }
      }
}

IRoute *AODVLDRouting::createRoute(const L3Address& destAddr, const L3Address& nextHop,
        unsigned int hopCount, bool hasValidDestNum, unsigned int destSeqNum,
        bool isActive, simtime_t lifeTime, simtime_t residualRouteLifetime)
{
    IRoute *newRoute = routingTable->createRoute();
    AODVLDRouteData *newProtocolData = new AODVLDRouteData();

    newProtocolData->setHasValidDestNum(hasValidDestNum);

    // active route
    newProtocolData->setIsActive(isActive);

    // A route towards a destination that has a routing table entry
    // that is marked as valid.  Only active routes can be used to
    // forward data packets.

    newProtocolData->setLifeTime(lifeTime);
    newProtocolData->setResidualRouteLifetime(residualRouteLifetime);
    newProtocolData->setDestSeqNum(destSeqNum);
    newProtocolData->setLinkFail(0);

    InterfaceEntry *ifEntry = interfaceTable->getInterfaceByName("wlan");    // TODO: IMPLEMENT: multiple interfaces
    if (ifEntry)
        newRoute->setInterface(ifEntry);

    newRoute->setDestination(destAddr);
    newRoute->setSourceType(IRoute::AODV);
    newRoute->setSource(this);
    newRoute->setProtocolData(newProtocolData);
    newRoute->setMetric(hopCount);
    newRoute->setNextHop(nextHop);
    newRoute->setPrefixLength(addressType->getMaxPrefixLength());

    EV_DETAIL << "Adding new route " << newRoute << endl;
    routingTable->addRoute(newRoute);
    scheduleExpungeRoutes();
    return newRoute;
}

void AODVLDRouting::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj DETAILS_ARG)
{
    Enter_Method("receiveChangeNotification");
    if (signalID == NF_LINK_BREAK) {
        //FOR LTE
        std::string name = source->getName();

        /*std::ofstream myfile;
        myfile.open ("Type.txt");
        myfile << name;
        myfile.close();
        */
        EV_DETAIL << "Received link break signal" << endl;
        // XXX: This is a hack for supporting both IdealMac and Ieee80211Mac. etc
        cPacket *frame = check_and_cast<cPacket *>(obj);
        INetworkDatagram *datagram=nullptr;
        if(name=="SimpleNeighborDiscovery"||name=="ip2lte"){
            datagram = check_and_cast<INetworkDatagram  *>(obj);
        }

        else if (false
#ifdef WITH_IEEE80211
            || dynamic_cast<ieee80211::Ieee80211Frame *>(frame)
#endif // ifdef WITH_IEEE80211
#ifdef WITH_IDEALWIRELESS
            || dynamic_cast<IdealMacFrame *>(frame)
#endif // ifdef WITH_IDEALWIRELESS
#ifdef WITH_CSMA
            || dynamic_cast<CSMAFrame *>(frame)
#endif // ifdef WITH_CSMA
#ifdef WITH_CSMACA
            || dynamic_cast<CsmaCaMacFrame *>(frame)
#endif // ifdef WITH_CSMACA
#ifdef WITH_LMAC
            || dynamic_cast<LMacFrame *>(frame)
#endif // ifdef WITH_LMAC
#ifdef WITH_BMAC
            || dynamic_cast<BMacFrame *>(frame)
#endif // ifdef WITH_BMAC
            )
            datagram = dynamic_cast<INetworkDatagram *>(frame->getEncapsulatedPacket());
        else
            throw cRuntimeError("Unknown packet type in NF_LINK_BREAK signal");
        if (datagram) {
            L3Address unreachableAddr = datagram->getDestinationAddress();
            if (unreachableAddr.getAddressType() == addressType) {
                // A node initiates processing for a RERR message in three situations:
                //
                //   (i)     if it detects a link break for the next hop of an active
                //           route in its routing table while transmitting data (and
                //           route repair, if attempted, was unsuccessful), or

                // TODO: Implement: local repair

                IRoute *route = routingTable->findBestMatchingRoute(unreachableAddr);

                if (route && route->getSource() == this)
                    handleLinkBreakSendRERR(route->getNextHopAsGeneric());
            }
        }
    }


}

void AODVLDRouting::handleLinkBreakSendRERR(const L3Address& unreachableAddr)
{
    // For case (i), the node first makes a list of unreachable destinations
    // consisting of the unreachable neighbor and any additional
    // destinations (or subnets, see section 7) in the local routing table
    // that use the unreachable neighbor as the next hop.

    // Just before transmitting the RERR, certain updates are made on the
    // routing table that may affect the destination sequence numbers for
    // the unreachable destinations.  For each one of these destinations,
    // the corresponding routing table entry is updated as follows:
    //
    // 1. The destination sequence number of this routing entry, if it
    //    exists and is valid, is incremented for cases (i) and (ii) above,
    //    and copied from the incoming RERR in case (iii) above.
    //
    // 2. The entry is invalidated by marking the route entry as invalid
    //
    // 3. The Lifetime field is updated to current time plus DELETE_PERIOD.
    //    Before this time, the entry SHOULD NOT be deleted.

    IRoute *unreachableRoute = routingTable->findBestMatchingRoute(unreachableAddr);
    std::string tempADDR=unreachableAddr.str();
std::string temp= getFullPath() ;
    if (!unreachableRoute || unreachableRoute->getSource() != this)
        return;

    std::vector<UnreachableNode> unreachableNodes;
    AODVLDRouteData *unreachableRouteData = check_and_cast<AODVLDRouteData *>(unreachableRoute->getProtocolData());

    if (unreachableRouteData->isActive()) {
        UnreachableNode node;
        node.addr = unreachableAddr;
        node.seqNum = unreachableRouteData->getDestSeqNum();
        unreachableNodes.push_back(node);
    }

    // For case (i), the node first makes a list of unreachable destinations
    // consisting of the unreachable neighbor and any additional destinations
    // (or subnets, see section 7) in the local routing table that use the
    // unreachable neighbor as the next hop.

    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IRoute *route = routingTable->getRoute(i);

        AODVLDRouteData *routeData = dynamic_cast<AODVLDRouteData *>(route->getProtocolData());
        if (routeData && routeData->isActive() && route->getNextHopAsGeneric() == unreachableAddr) {
            if (routeData->hasValidDestNum())
                routeData->setDestSeqNum(routeData->getDestSeqNum() + 1);

            EV_DETAIL << "Marking route to " << route->getDestinationAsGeneric() << " as inactive" << endl;

            routeData->setIsActive(false);
            routeData->setLifeTime(simTime() + deletePeriod);
            //AODVLD (LIfetime has no effekt)
            routeData->setLinkFail(true);
            routeData->setResidualRouteLifetime(simTime() + deletePeriod);
            scheduleExpungeRoutes();

            UnreachableNode node;
            node.addr = route->getDestinationAsGeneric();
            node.seqNum = routeData->getDestSeqNum();
            unreachableNodes.push_back(node);
        }
    }

    // The neighboring node(s) that should receive the RERR are all those
    // that belong to a precursor list of at least one of the unreachable
    // destination(s) in the newly created RERR.  In case there is only one
    // unique neighbor that needs to receive the RERR, the RERR SHOULD be
    // unicast toward that neighbor.  Otherwise the RERR is typically sent
    // to the local broadcast address (Destination IP == 255.255.255.255,
    // TTL == 1) with the unreachable destinations, and their corresponding
    // destination sequence numbers, included in the packet.

    if (rerrCount >= rerrRatelimit) {
        EV_WARN << "A node should not generate more than RERR_RATELIMIT RERR messages per second. Canceling sending RERR" << endl;
        return;
    }

    if (unreachableNodes.empty())
        return;

    AODVLDRERR *rerr = createRERR(unreachableNodes);
    rerrCount++;

    // broadcast
    EV_INFO << "Broadcasting Route Error message with TTL=1" << endl;
    sendAODVLDPacket(rerr, addressType->getBroadcastAddress(), 1, jitterPar->doubleValue());
    simtime_t timestamp =simTime();

    cTimestampedValue tmp1(timestamp, 1.0);
    emit(numSentRERR,&tmp1);
}

AODVLDRERR *AODVLDRouting::createRERR(const std::vector<UnreachableNode>& unreachableNodes)
{
    AODVLDRERR *rerr = new AODVLDRERR("AODVLD-RERR");
    unsigned int destCount = unreachableNodes.size();

    rerr->setPacketType(RERR);
    rerr->setDestCount(destCount);
    rerr->setUnreachableNodesArraySize(destCount);

    for (unsigned int i = 0; i < destCount; i++) {
        UnreachableNode node;
        node.addr = unreachableNodes[i].addr;
        node.seqNum = unreachableNodes[i].seqNum;
        rerr->setUnreachableNodes(i, node);
    }

    rerr->setByteLength(4 + 4 * 2 * destCount);
    return rerr;
}

void AODVLDRouting::handleRERR(AODVLDRERR *rerr, const L3Address& sourceAddr)
{
    std::string tempADDR=sourceAddr.str();
    std::string temp= getFullPath();
    EV_INFO << "AODVLD Route Error arrived with source addr: " << sourceAddr << endl;

    // A node initiates processing for a RERR message in three situations:
    // (iii)   if it receives a RERR from a neighbor for one or more
    //         active routes.
    unsigned int unreachableArraySize = rerr->getUnreachableNodesArraySize();
    std::vector<UnreachableNode> unreachableNeighbors;

    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IRoute *route = routingTable->getRoute(i);
        AODVLDRouteData *routeData = route ? dynamic_cast<AODVLDRouteData *>(route->getProtocolData()) : nullptr;

        if (!routeData)
            continue;

        // For case (iii), the list should consist of those destinations in the RERR
        // for which there exists a corresponding entry in the local routing
        // table that has the transmitter of the received RERR as the next hop.

        if (routeData->isActive() && route->getNextHopAsGeneric() == sourceAddr) {
            for (unsigned int j = 0; j < unreachableArraySize; j++) {
                if (route->getDestinationAsGeneric() == rerr->getUnreachableNodes(j).addr) {
                    // 1. The destination sequence number of this routing entry, if it
                    // exists and is valid, is incremented for cases (i) and (ii) above,
                    // ! and copied from the incoming RERR in case (iii) above.

                    routeData->setDestSeqNum(rerr->getUnreachableNodes(j).seqNum);
                    routeData->setIsActive(false);    // it means invalid, see 3. AODVLD Terminology p.3. in RFC 3561
                    routeData->setLifeTime(simTime() + deletePeriod);

                    routeData->setResidualRouteLifetime(simTime() + deletePeriod);

                    // The RERR should contain those destinations that are part of
                    // the created list of unreachable destinations and have a non-empty
                    // precursor list.

                    if (routeData->getPrecursorList().size() > 0) {
                        UnreachableNode node;
                        node.addr = route->getDestinationAsGeneric();
                        node.seqNum = routeData->getDestSeqNum();
                        unreachableNeighbors.push_back(node);
                    }
                    scheduleExpungeRoutes();
                }
            }
        }
    }

    if (rerrCount >= rerrRatelimit) {
        EV_WARN << "A node should not generate more than RERR_RATELIMIT RERR messages per second. Canceling sending RERR" << endl;
        delete rerr;
        return;
    }

    if (unreachableNeighbors.size() > 0 && (simTime() > rebootTime + deletePeriod || rebootTime == 0)) {
        EV_INFO << "Sending RERR to inform our neighbors about link breaks." << endl;
        AODVLDRERR *newRERR = createRERR(unreachableNeighbors);
        sendAODVLDPacket(newRERR, addressType->getBroadcastAddress(), 1, 0);

        rerrCount++;
    }
    delete rerr;
    simtime_t timestamp =simTime();

    cTimestampedValue tmp1(timestamp, 1.0);
    emit(numReceivedRERR,&tmp1);
}

bool AODVLDRouting::handleOperationStage(LifecycleOperation *operation, int stage, IDoneCallback *doneCallback)
{
    Enter_Method_Silent();
    if (dynamic_cast<NodeStartOperation *>(operation)) {
        if ((NodeStartOperation::Stage)stage == NodeStartOperation::STAGE_APPLICATION_LAYER) {
            isOperational = true;
            rebootTime = simTime();

            if (useHelloMessages)
                scheduleAt(simTime() + helloInterval - periodicJitter->doubleValue(), helloMsgTimer);

            scheduleAt(simTime() + 1, counterTimer);
        }
    }
    else if (dynamic_cast<NodeShutdownOperation *>(operation)) {
        if ((NodeShutdownOperation::Stage)stage == NodeShutdownOperation::STAGE_APPLICATION_LAYER) {
            isOperational = false;
            clearState();
        }
    }
    else if (dynamic_cast<NodeCrashOperation *>(operation)) {
        if ((NodeCrashOperation::Stage)stage == NodeCrashOperation::STAGE_CRASH) {
            isOperational = false;
            clearState();
        }
    }
    else
        throw cRuntimeError("Unsupported lifecycle operation '%s'", operation->getClassName());

    return true;
}

void AODVLDRouting::clearState()
{
    rerrCount = rreqCount = rreqId = sequenceNum = 0;
    addressToRreqRetries.clear();
    for (auto & elem : waitForRREPTimers)
        cancelAndDelete(elem.second);

    // FIXME: Drop the queued datagrams.
    //for (auto it = targetAddressToDelayedPackets.begin(); it != targetAddressToDelayedPackets.end(); it++)
    //    networkProtocol->dropQueuedDatagram(const_cast<const INetworkDatagram *>(it->second));

    targetAddressToDelayedPackets.clear();

    waitForRREPTimers.clear();
    rreqsArrivalTime.clear();

    if (useHelloMessages)
        cancelEvent(helloMsgTimer);

    cancelEvent(expungeTimer);
    cancelEvent(counterTimer);
    cancelEvent(blacklistTimer);
    cancelEvent(rrepAckTimer);
}

void AODVLDRouting::handleWaitForRREP(WaitForRREP *rrepTimer)
{
    EV_INFO << "We didn't get any Route Reply within RREP timeout" << endl;
    L3Address destAddr = rrepTimer->getDestAddr();

    ASSERT(addressToRreqRetries.find(destAddr) != addressToRreqRetries.end());
    if (addressToRreqRetries[destAddr] == rreqRetries) {
        EV_WARN << "Re-discovery attempts for node " << destAddr << " reached RREQ_RETRIES= " << rreqRetries << " limit. Stop sending RREQ." << endl;
        return;
    }

    AODVLDRREQ *rreq = createRREQ(destAddr);

    // the node MAY try again to discover a route by broadcasting another
    // RREQ, up to a maximum of RREQ_RETRIES times at the maximum TTL value.
    if (rrepTimer->getLastTTL() == netDiameter) // netDiameter is the maximum TTL value
        addressToRreqRetries[destAddr]++;

    sendRREQ(rreq, addressType->getBroadcastAddress(), 0);
}

void AODVLDRouting::forwardRREP(AODVLDRREP *rrep, const L3Address& destAddr, unsigned int timeToLive)
{
    EV_INFO << "Forwarding the Route Reply to the node " << rrep->getOriginatorAddr() << " which originated the Route Request" << endl;

    // RFC 5148:
    // When a node forwards a message, it SHOULD be jittered by delaying it
    // by a random duration.  This delay SHOULD be generated uniformly in an
    // interval between zero and MAXJITTER.
    sendAODVLDPacket(rrep, destAddr, 100, jitterPar->doubleValue());
}

void AODVLDRouting::forwardRREQ(AODVLDRREQ *rreq, unsigned int timeToLive)
{
    simtime_t timestamp=simTime();
    cTimestampedValue tmp(timestamp, 1.0);
    emit(numRREQForwarded,&tmp);
    EV_INFO << "Forwarding the Route Request message with TTL= " << timeToLive << endl;
    sendAODVLDPacket(rreq, addressType->getBroadcastAddress(), timeToLive, jitterPar->doubleValue());
}

void AODVLDRouting::completeRouteDiscovery(const L3Address& target)
{
    EV_DETAIL << "Completing route discovery, originator " << getSelfIPAddress() << ", target " << target << endl;
    ASSERT(hasOngoingRouteDiscovery(target));

    auto lt = targetAddressToDelayedPackets.lower_bound(target);
    auto ut = targetAddressToDelayedPackets.upper_bound(target);

    // reinject the delayed datagrams
    for (auto it = lt; it != ut; it++) {
        INetworkDatagram *datagram = it->second;
        EV_DETAIL << "Sending queued datagram: source " << datagram->getSourceAddress() << ", destination " << datagram->getDestinationAddress() << endl;
        networkProtocol->reinjectQueuedDatagram(const_cast<const INetworkDatagram *>(datagram));
    }

    // clear the multimap
    targetAddressToDelayedPackets.erase(lt, ut);

    // we have a route for the destination, thus we must cancel the WaitForRREPTimer events
    auto waitRREPIter = waitForRREPTimers.find(target);
    ASSERT(waitRREPIter != waitForRREPTimers.end());
    cancelAndDelete(waitRREPIter->second);
    waitForRREPTimers.erase(waitRREPIter);
}

void AODVLDRouting::sendGRREP(AODVLDRREP *grrep, const L3Address& destAddr, unsigned int timeToLive)
{
    EV_INFO << "Sending gratuitous Route Reply to " << destAddr << endl;

    IRoute *destRoute = routingTable->findBestMatchingRoute(destAddr);
    const L3Address& nextHop = destRoute->getNextHopAsGeneric();

    sendAODVLDPacket(grrep, nextHop, timeToLive, 0);
}

AODVLDRREP *AODVLDRouting::createHelloMessage()
{
    // called a Hello message, with the RREP
    // message fields set as follows:
    //
    //    Destination IP Address         The node's IP address.
    //
    //    Destination Sequence Number    The node's latest sequence number.
    //
    //    Hop Count                      0
    //
    //    Lifetime                       ALLOWED_HELLO_LOSS *HELLO_INTERVAL

    AODVLDRREP *helloMessage = new AODVLDRREP("AODVLD-HelloMsg");
    helloMessage->setPacketType(RREP);
    helloMessage->setDestAddr(getSelfIPAddress());
    helloMessage->setDestSeqNum(sequenceNum);
    helloMessage->setHopCount(0);
    helloMessage->setLifeTime(allowedHelloLoss * helloInterval);
    helloMessage->setByteLength(20);

    return helloMessage;
}

void AODVLDRouting::sendHelloMessagesIfNeeded()
{
    ASSERT(useHelloMessages);
    // Every HELLO_INTERVAL milliseconds, the node checks whether it has
    // sent a broadcast (e.g., a RREQ or an appropriate layer 2 message)
    // within the last HELLO_INTERVAL.  If it has not, it MAY broadcast
    // a RREP with TTL = 1

    // A node SHOULD only use hello messages if it is part of an
    // active route.
    bool hasActiveRoute = false;

    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IRoute *route = routingTable->getRoute(i);
        if (route->getSource() == this) {
            AODVLDRouteData *routeData = check_and_cast<AODVLDRouteData *>(route->getProtocolData());
            if (routeData->isActive()) {
                hasActiveRoute = true;
                break;
            }
        }
    }

    if (hasActiveRoute && (lastBroadcastTime == 0 || simTime() - lastBroadcastTime > helloInterval)) {
        EV_INFO << "It is hello time, broadcasting Hello Messages with TTL=1" << endl;
        AODVLDRREP *helloMessage = createHelloMessage();
        sendAODVLDPacket(helloMessage, addressType->getBroadcastAddress(), 1, 0);
    }

    scheduleAt(simTime() + helloInterval - periodicJitter->doubleValue(), helloMsgTimer);
}

void AODVLDRouting::handleHelloMessage(AODVLDRREP *helloMessage)
{
    const L3Address& helloOriginatorAddr = helloMessage->getDestAddr();
    IRoute *routeHelloOriginator = routingTable->findBestMatchingRoute(helloOriginatorAddr);

    // Whenever a node receives a Hello message from a neighbor, the node
    // SHOULD make sure that it has an active route to the neighbor, and
    // create one if necessary.  If a route already exists, then the
    // Lifetime for the route should be increased, if necessary, to be at
    // least ALLOWED_HELLO_LOSS * HELLO_INTERVAL.  The route to the
    // neighbor, if it exists, MUST subsequently contain the latest
    // Destination Sequence Number from the Hello message.  The current node
    // can now begin using this route to forward data packets.  Routes that
    // are created by hello messages and not used by any other active routes
    // will have empty precursor lists and would not trigger a RERR message
    // if the neighbor moves away and a neighbor timeout occurs.

    /*Route data contains residual route lifetime, which is in this case just the residual link lifetime, because it is just received from neighbor*/
    unsigned int latestDestSeqNum = helloMessage->getDestSeqNum();
    simtime_t newLifeTime = simTime() + allowedHelloLoss * helloInterval;

    if (!routeHelloOriginator || routeHelloOriginator->getSource() != this)
        createRoute(helloOriginatorAddr, helloOriginatorAddr, 1, true, latestDestSeqNum, true, newLifeTime,simTime() + metrikmodule->getMetrik(helloOriginatorAddr));
    else {
        AODVLDRouteData *routeData = check_and_cast<AODVLDRouteData *>(routeHelloOriginator->getProtocolData());
        simtime_t lifeTime = routeData->getLifeTime();
        updateRoutingTable(routeHelloOriginator, helloOriginatorAddr, 1, true, latestDestSeqNum, true, std::max(lifeTime, newLifeTime),simTime() + metrikmodule->getMetrik(helloOriginatorAddr));
    }

    // TODO: This feature has not implemented yet.
    // A node MAY determine connectivity by listening for packets from its
    // set of neighbors.  If, within the past DELETE_PERIOD, it has received
    // a Hello message from a neighbor, and then for that neighbor does not
    // receive any packets (Hello messages or otherwise) for more than
    // ALLOWED_HELLO_LOSS * HELLO_INTERVAL milliseconds, the node SHOULD
    // assume that the link to this neighbor is currently lost.  When this
    // happens, the node SHOULD proceed as in Section 6.11.
}

/*Change LifeTime to residualRouteLifetime*/
//TODO Is LifeTime still really needed?
void AODVLDRouting::expungeRoutes()
{
    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IRoute *route = routingTable->getRoute(i);
        if (route->getSource() == this) {
            AODVLDRouteData *routeData = check_and_cast<AODVLDRouteData *>(route->getProtocolData());
            ASSERT(routeData != nullptr);
            if(routeData->getLinkFail()==0&&routeData->getResidualRouteLifetime() <= simTime())
                routeData->setResidualRouteLifetime(simTime()+50);
            if (routeData->getResidualRouteLifetime() <= simTime()) {
                if (routeData->isActive()) {
                    EV_DETAIL << "Route to " << route->getDestinationAsGeneric() << " expired and set to inactive. It will be deleted after DELETE_PERIOD time" << endl;
                    // An expired routing table entry SHOULD NOT be expunged before
                    // (current_time + DELETE_PERIOD) (see section 6.11).  Otherwise, the
                    // soft state corresponding to the route (e.g., last known hop count)
                    // will be lost.
                    routeData->setIsActive(false);
                    routeData->setResidualRouteLifetime(simTime() + deletePeriod);
                }
                else {
                    // Any routing table entry waiting for a RREP SHOULD NOT be expunged
                    // before (current_time + 2 * NET_TRAVERSAL_TIME).
                    if (hasOngoingRouteDiscovery(route->getDestinationAsGeneric())) {
                        EV_DETAIL << "Route to " << route->getDestinationAsGeneric() << " expired and is inactive, but we are waiting for a RREP to this destination, so we extend its lifetime with 2 * NET_TRAVERSAL_TIME" << endl;
                        routeData->setResidualRouteLifetime(simTime() + 2 * netTraversalTime+ netDiameter);
                    }
                    else {
                        EV_DETAIL << "Route to " << route->getDestinationAsGeneric() << " expired and is inactive and we are not expecting any RREP to this destination, so we delete this route" << endl;
                        routingTable->deleteRoute(route);
                    }
                }
            }
        }
    }
    scheduleExpungeRoutes();
}
/*Change LifeTime to residualRouteLifetime*/
void AODVLDRouting::scheduleExpungeRoutes()
{
    simtime_t nextExpungeTime = SimTime::getMaxTime();
    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IRoute *route = routingTable->getRoute(i);

        if (route->getSource() == this) {
            AODVLDRouteData *routeData = check_and_cast<AODVLDRouteData *>(route->getProtocolData());
            ASSERT(routeData != nullptr);

            if (routeData->getResidualRouteLifetime() < nextExpungeTime)
                nextExpungeTime = routeData->getResidualRouteLifetime();
        }
    }
    if (nextExpungeTime == SimTime::getMaxTime()) {
        if (expungeTimer->isScheduled())
            cancelEvent(expungeTimer);
    }
    else {
        if (!expungeTimer->isScheduled())
            scheduleAt(nextExpungeTime, expungeTimer);
        else {
            if (expungeTimer->getArrivalTime() != nextExpungeTime) {
                cancelEvent(expungeTimer);
                scheduleAt(nextExpungeTime, expungeTimer);
            }
        }
    }
}

INetfilter::IHook::Result AODVLDRouting::datagramForwardHook(INetworkDatagram *datagram, const InterfaceEntry *inputInterfaceEntry, const InterfaceEntry *& outputInterfaceEntry, L3Address& nextHopAddress)
{
    // TODO: Implement: Actions After Reboot
    // If the node receives a data packet for some other destination, it SHOULD
    // broadcast a RERR as described in subsection 6.11 and MUST reset the waiting
    // timer to expire after current time plus DELETE_PERIOD.

    Enter_Method("datagramForwardHook");
    const L3Address& destAddr = datagram->getDestinationAddress();
    const L3Address& sourceAddr = datagram->getSourceAddress();
    IRoute *ipSource = routingTable->findBestMatchingRoute(sourceAddr);

    if (destAddr.isBroadcast() || routingTable->isLocalAddress(destAddr) || destAddr.isMulticast()) {
        if (routingTable->isLocalAddress(destAddr) && ipSource && ipSource->getSource() == this)
            updateValidRouteLifeTime(ipSource->getNextHopAsGeneric(), simTime() + activeRouteTimeout);

        return ACCEPT;
    }

    // TODO: IMPLEMENT: check if the datagram is a data packet or we take control packets as data packets

    IRoute *routeDest = routingTable->findBestMatchingRoute(destAddr);
    AODVLDRouteData *routeDestData = routeDest ? dynamic_cast<AODVLDRouteData *>(routeDest->getProtocolData()) : nullptr;

    // Each time a route is used to forward a data packet, its Active Route
    // Lifetime field of the source, destination and the next hop on the
    // path to the destination is updated to be no less than the current
    // time plus ACTIVE_ROUTE_TIMEOUT

    updateValidRouteLifeTime(sourceAddr, simTime() + activeRouteTimeout);
    updateValidRouteLifeTime(destAddr, simTime() + activeRouteTimeout);

    if (routeDest && routeDest->getSource() == this)
        updateValidRouteLifeTime(routeDest->getNextHopAsGeneric(), simTime() + activeRouteTimeout);

    // Since the route between each originator and destination pair is expected
    // to be symmetric, the Active Route Lifetime for the previous hop, along the
    // reverse path back to the IP source, is also updated to be no less than the
    // current time plus ACTIVE_ROUTE_TIMEOUT.

    if (ipSource && ipSource->getSource() == this)
        updateValidRouteLifeTime(ipSource->getNextHopAsGeneric(), simTime() + activeRouteTimeout);

    EV_INFO << "We can't forward datagram because we have no active route for " << destAddr << endl;
    if (routeDest && routeDestData && !routeDestData->isActive()) {    // exists but is not active
        // A node initiates processing for a RERR message in three situations:
        // (ii)      if it gets a data packet destined to a node for which it
        //           does not have an active route and is not repairing (if
        //           using local repair)

        // TODO: check if it is not repairing (if using local repair)

        // 1. The destination sequence number of this routing entry, if it
        // exists and is valid, is incremented for cases (i) and (ii) above,
        // and copied from the incoming RERR in case (iii) above.

        if (routeDestData->hasValidDestNum())
            routeDestData->setDestSeqNum(routeDestData->getDestSeqNum() + 1);

        // 2. The entry is invalidated by marking the route entry as invalid <- it is invalid

        // 3. The Lifetime field is updated to current time plus DELETE_PERIOD.
        //    Before this time, the entry SHOULD NOT be deleted.
        routeDestData->setLifeTime(simTime() + deletePeriod);

        sendRERRWhenNoRouteToForward(destAddr);
    }
    else if (!routeDest || routeDest->getSource() != this) // doesn't exist at all
        sendRERRWhenNoRouteToForward(destAddr);

    return ACCEPT;
}

void AODVLDRouting::sendRERRWhenNoRouteToForward(const L3Address& unreachableAddr)
{
    if (rerrCount >= rerrRatelimit) {
        EV_WARN << "A node should not generate more than RERR_RATELIMIT RERR messages per second. Canceling sending RERR" << endl;
        return;
    }
    std::vector<UnreachableNode> unreachableNodes;
    UnreachableNode node;
    node.addr = unreachableAddr;

    IRoute *unreachableRoute = routingTable->findBestMatchingRoute(unreachableAddr);
    AODVLDRouteData *unreachableRouteData = unreachableRoute ? dynamic_cast<AODVLDRouteData *>(unreachableRoute->getProtocolData()) : nullptr;

    if (unreachableRouteData && unreachableRouteData->hasValidDestNum())
        node.seqNum = unreachableRouteData->getDestSeqNum();
    else
        node.seqNum = 0;

    unreachableNodes.push_back(node);
    AODVLDRERR *rerr = createRERR(unreachableNodes);

    rerrCount++;
    EV_INFO << "Broadcasting Route Error message with TTL=1" << endl;
    sendAODVLDPacket(rerr, addressType->getBroadcastAddress(), 1, jitterPar->doubleValue());    // TODO: unicast if there exists a route to the source
}

void AODVLDRouting::cancelRouteDiscovery(const L3Address& destAddr)
{
    ASSERT(hasOngoingRouteDiscovery(destAddr));
    auto lt = targetAddressToDelayedPackets.lower_bound(destAddr);
    auto ut = targetAddressToDelayedPackets.upper_bound(destAddr);
    for (auto it = lt; it != ut; it++)
        networkProtocol->dropQueuedDatagram(const_cast<const INetworkDatagram *>(it->second));

    targetAddressToDelayedPackets.erase(lt, ut);
}

bool AODVLDRouting::updateValidRouteLifeTime(const L3Address& destAddr, simtime_t lifetime)
{
    IRoute *route = routingTable->findBestMatchingRoute(destAddr);
    if (route && route->getSource() == this) {
        AODVLDRouteData *routeData = check_and_cast<AODVLDRouteData *>(route->getProtocolData());
        if (routeData->isActive()) {
            simtime_t newLifeTime = std::max(routeData->getLifeTime(), lifetime);
            EV_DETAIL << "Updating " << route << " lifetime to " << newLifeTime << endl;
            routeData->setLifeTime(newLifeTime);
            return true;
        }
    }
    return false;
}

AODVLDRREPACK *AODVLDRouting::createRREPACK()
{
    AODVLDRREPACK *rrepACK = new AODVLDRREPACK("AODVLD-RREPACK");
    rrepACK->setPacketType(RREPACK);
    rrepACK->setByteLength(2);
    return rrepACK;
}

void AODVLDRouting::sendRREPACK(AODVLDRREPACK *rrepACK, const L3Address& destAddr)
{
    EV_INFO << "Sending Route Reply ACK to " << destAddr << endl;
    sendAODVLDPacket(rrepACK, destAddr, 100, 0);
}

void AODVLDRouting::handleRREPACK(AODVLDRREPACK *rrepACK, const L3Address& neighborAddr)
{
    // Note that the RREP-ACK packet does not contain any information about
    // which RREP it is acknowledging.  The time at which the RREP-ACK is
    // received will likely come just after the time when the RREP was sent
    // with the 'A' bit.
    ASSERT(rrepAckTimer->isScheduled());
    EV_INFO << "RREP-ACK arrived from " << neighborAddr << endl;

    IRoute *route = routingTable->findBestMatchingRoute(neighborAddr);
    if (route && route->getSource() == this) {
        EV_DETAIL << "Marking route " << route << " as active" << endl;
        AODVLDRouteData *routeData = check_and_cast<AODVLDRouteData *>(route->getProtocolData());
        routeData->setIsActive(true);
        cancelEvent(rrepAckTimer);
    }
}

void AODVLDRouting::handleRREPACKTimer()
{
    // when a node detects that its transmission of a RREP message has failed,
    // it remembers the next-hop of the failed RREP in a "blacklist" set.

    EV_INFO << "RREP-ACK didn't arrived within timeout. Adding " << failedNextHop << " to the blacklist" << endl;

    blacklist[failedNextHop] = simTime() + blacklistTimeout;    // lifetime

    if (!blacklistTimer->isScheduled())
        scheduleAt(simTime() + blacklistTimeout, blacklistTimer);
}

void AODVLDRouting::handleBlackListTimer()
{
    simtime_t nextTime = SimTime::getMaxTime();

    for (auto it = blacklist.begin(); it != blacklist.end(); ) {
        auto current = it++;

        // Nodes are removed from the blacklist set after a BLACKLIST_TIMEOUT period
        if (current->second <= simTime()) {
            EV_DETAIL << "Blacklist lifetime has expired for " << current->first << " removing it from the blacklisted addresses" << endl;
            blacklist.erase(current);
        }
        else if (nextTime > current->second)
            nextTime = current->second;
    }

    if (nextTime != SimTime::getMaxTime())
        scheduleAt(nextTime, blacklistTimer);
}

AODVLDRouting::~AODVLDRouting()
{
    clearState();
    delete helloMsgTimer;
    delete expungeTimer;
    delete counterTimer;
    delete rrepAckTimer;
    delete blacklistTimer;
    cOwnedObject *Del=NULL;
    int OwnedSize=this->defaultListSize();
    for(int i=0;i<OwnedSize;i++){
            Del=this->defaultListGet(0);
            this->drop(Del);
            delete Del;
    }


 /*   for(std::vector<WaitForRREQ *>::iterator it = rreqcollectionTimer.begin();it !=rreqcollectionTimer.end();++it){

        delete (*it);

    }
*/
}

} // namespace inet

