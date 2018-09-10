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

package lte.routing.aodvLD;

//Copy from AODVRouting with different parameters
@namespace(inet);
simple AODVLDRouting
{

    parameters:
        @display("i=block/network2");
        string routingTableModule = default("^.routingTable");
        string interfaceTableModule = default("^.interfaceTable");
        string networkProtocolModule = default("^.networkLayer.ip");
        string residualLinkLifetimeModule = default("^.LD.ResidualLinklifetime");
        string neighborDiscoveryModule = default("^.LD.SimpleNeighborDiscovery");

        bool askGratuitousRREP = default(false); // see RFC 3561: 6.6.3
        bool useHelloMessages = default(false); // see RFC 3561: 6.9
        bool useLocalRepair = default(false); // see RFC 3561: 6.12 *not implemented yet*
        int udpPort = default(654);

        double maxPeriodicJitter @unit("s") = default(helloInterval / 4); // it MUST NOT be negative; it MUST NOT be greater than MESSAGE_INTERVAL/2; it SHOULD NOT be greater than MESSAGE_INTERVAL/4.
        volatile double periodicJitter @unit("s") = default(uniform(0s, maxPeriodicJitter)); // jitter for externally triggered message generation and message forwarding

        // RFC 5148:
        // need more revise: As well as the decision as to whether
        // to use jitter being dependent on the medium access control and
        // lower layers, the selection of the MAXJITTER parameter SHOULD
        // be appropriate to those mechanisms.
        double maxJitter @unit("s") = default(5ms);
        volatile double jitter @unit("s") = default(uniform(0ms, maxJitter)); // jitter for broadcasts

        double helloInterval @unit("s") = default(1s); // every helloInterval seconds a node broadcasts Hello messages (if it is necessary)
        int allowedHelloLoss = default(2); // allowedHelloLoss * helloInterval is the lifetime value for Hello messages
        double activeRouteTimeout @unit("s") = default(3s); // the timeout value for cached routes If Hello messages are used, then the
                                                            // ACTIVE_ROUTE_TIMEOUT parameter value MUST be more than the value
                                                            // (ALLOWED_HELLO_LOSS * HELLO_INTERVAL).
        int netDiameter = default(35); // the maximum possible number of hops between two nodes in the network
        double nodeTraversalTime @unit("s") = default(0.04s); // an estimation of the average one-hop traversal time
        int rerrRatelimit = default(10); // maximum number of RERR messages that the AODV may originate in 1s.
        int rreqRetries = default(2); // specifies the number of times AODV will repeat an expanded ring search for a destination
        int rreqRatelimit = default(10); // maximum number of RREQ messages that the AODV may originate in 1s.
        int timeoutBuffer = default(2); // plus time to wait for a delayed RREP (due to congestion) (to omit this buffer set it to 0)
        int ttlStart = default(2); // specifies the TTL value when initiating a route request
        int ttlIncrement = default(2); // specifies the value by which the TTL will be incremented each time a RREQ is retransmitted
        int ttlThreshold = default(7); // the maximum value of TTL over which NET_DIAMETER value will be used to broadcast any RREQ
        int localAddTTL = default(2); // it is used by the formula which calculates the initial TTL of the RREQ for a local repair
        double RLMaring @unit("s")=default(2s); //defining how much better later rreqs have to be in terms of residual linklifetime, in order to be used

        double myRouteTimeout @unit("s") = default(2 * activeRouteTimeout); // the value of the lifetime field that a destination node places in RREPs
        double deletePeriod @unit("s") = default(5 * max(activeRouteTimeout, helloInterval)); // the time after which an expired route is deleted
        double blacklistTimeout @unit("s") = default(rreqRetries * netTraversalTime); // the time after which a blacklisted node is removed from the blacklist
        double netTraversalTime @unit("s") = default(2 * nodeTraversalTime * netDiameter); // an estimation of the traversal time for the complete network
        double nextHopWait @unit("s") = default(nodeTraversalTime + 0.01s); // timeout for a RREP-ACK
        double pathDiscoveryTime @unit("s") = default(2 * netTraversalTime); // buffer timeout for each broadcasted RREQ message
        double RREQCollectionTimeMin @unit("s") = default(0.01s); // Minumum time same RREQ is saved and exchanged to better RREQ until rebroadcst
        double RREQCollectionTimeMean @unit("s") = default(0.05s); // Mean of exponential ditributed addition to  RREQCollectionTimeMin
        double RREQinLastTransTimer @unit("s") = default(3s); //Time when RREQ is thrown out of LastTransmittedBuffer
        bool RREQTimerHopDependeny = default(false); // if true: More number of hops of RREQ lead to higher RREQ waiting time (unfinished and untested)
        bool constRREQTimer=default(true); //If true only RREQCollectionTimeMin is used as timer (no exponential addition)
        string DestinationAddress = default("10.0.3.3"); //Destination of transmitted packets. To determine if results are to be measured
        // "0.0.0.0" will lead to not measure results. Additionally prevents other nodes to start route discovery.
        
        
        
        @signal[numRREQsent];
        @statistic[numRREQsent](title="numRREQsent"; record=sum);
        @signal[routeAvailability]; //Periodically saves if route is present or not (Too large intervals will lead to oversee link failures)
        @statistic[routeAvailability](title="routeAvailability"; record=vector);
        @signal[interRREPRouteDiscoveryTime]; //From last RREP to next route discovery (Route Duration)
        @statistic[interRREPRouteDiscoveryTime](title="interRREPRouteDiscoveryTime"; record=vector);
        
        @signal[interRREQRREPTime]; //Route Discovery Start - RREP arrive ( can occure multiple times for 1 RREQ)
        @statistic[interRREQRREPTime](title="interRREQRREPTime"; record=vector);
        @signal[numFinalHops];//numHop of final routes
        @statistic[numFinalHops](title="numFinalHops"; record=vector);
        @signal[numRREQForwarded];
        @statistic[numRREQForwarded](title="numRREQForwarded"; record=sum);
        @signal[numSentRERR];//numHop of final routes
        @statistic[numSentRERR](title="numSentRERR"; record=sum);
        
        @signal[sentAODVLDpackets];
        @statistic[sentAODVLDpackets](title="sentAODVLDpackets"; record=sum);
        
        @signal[numReceivedRERR];
        @statistic[numReceivedRERR](title="numReceivedRERR"; record=sum);
        @signal[ newSeqNum];
        @statistic[ newSeqNum](title=" newSeqNum"; record=vector);
        @signal[theoreticalRL];
        @statistic[theoreticalRL](title=" theoreticalRL"; record=vector);
  
       
        

  
    gates:
        input ipIn;
        output ipOut;
}
