# D2DMH (Device-to-Device multi-hop) Short Documentation
*More documentation in the source code*

## D2DMH combines SimuLTE with routing in the INET Framework
The Following steps were made
- Binder needs to enable D2D capability between all UEs: set `utilizeAODV=true` in module binder
- Mac addresses are needed to enable routing: set `utilizeAODV=true` in mac module (LTEMacUE)
- IP2LTE module can note handle ARP: set ARP module `networkLayer.arpType = GlobalARP` in `omnetpp.ini`
- IP2LTE needs to handle IP packets differently.
  - Packet's next hop are in the control info and not in the direct header anymore.
  - Broadcast IPs are mapped to multicast IPs.: set `utilizeAODV=true` in IP2LTE module
- Important change in `LteMacUeD2D::macHandleRac`:
```
if (racD2DMulticastRequested_) {
  bsrD2DMulticastTriggered_ = true;
  bsrTriggered_ = false; //ADDED
} else {
  bsrTriggered_ = true
  bsrD2DMulticastTriggered_ = false; //ADDED
}
```
- Lines with `ADDED` were added to handle multicast and unicast packets in one simulation

## In LTERealisticChannelModel:
- Unit disc path loss model implemented:
  - `Zero_Until_Tx_Range` &rarr; Attenuation is zero until `Tx_Range`, `inf` for larger
  - Necessary because implemented models have a maximum transmission range (10000m) and neighbor discovery module only relies on distance and transmission range.
  - Transmission range to be set in `config_channel.xml`

## AODV/LD
- Link Duration Module contained in every UE and contains following 3 submodules
  - SimpleNeighborDiscovery:
    - Responsible to determine if UE is reachable or not by comparing Euclidean distance with transmission range.
    - Emits a signal for AODV/LD when link is broken.
    - Is called every X seconds (could be adapted by parameter, `default = 1s`)
    - updates NeighborLinkTimeTable
  - NeighborLinkTimeTable: Contains information if and for how long UEs are neighbor of this UE and is able to offer it
  - ResidualLinkLifetime: Takes information of link lifetime distribution from CDF module and link lifetime of NeighborLinkTimeTable to estimate residual link lifetime
- CDF module
  - One module in whole simulation. Has information about Medina's PDF and CDF and is able to offer it
  - Contains mapping from $CDF(t) \to t$, because inversion of CDF is complex to calculate
  - Maximum for t is 24h: $CDF(\lt 24h) \to CDF(24h)$
- `AODVLDControlPackets_m`
  - Added residual link lifetime to RREQ
  - Added residual route lifetime to RREP
- AODVLDRouting
  - Minor adaption from AODV are commented in the code
  - Still no route repair implemented
  - Intermediate nodes are not allowed to send RREP, when they have a route to dest
  - prehandleRREQ:
    - RREQ are saved until a certain time
    - when new RREQ occur, it is compared to previously transmitted RREQ and the best RREQ which is candidate for rebroadcasting at the moment.
    - New RREQ is exchanged with the currently best one, when it is better than both.
  - handle RREQ: Timer expired and the best RREQ is rebroadcasted
  - Only RERR lead to invalidation of routes
  - RRL = min(RLL), RLL taken from RREQs &rarr; adaption needed for better RRL estimation
