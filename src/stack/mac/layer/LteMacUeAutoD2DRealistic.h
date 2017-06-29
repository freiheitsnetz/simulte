//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_LTEMACUEAUTOD2DREALISTIC_H_
#define _LTE_LTEMACUEAUTOD2DREALISTIC_H_

#include "LteMacUeAutoD2D.h"

class LteSchedulingGrant;
class LteSchedulerUeUl;
class LteBinder;

class LteMacUeAutoD2DRealistic : public LteMacUeAutoD2D
{
  protected:

    /// List of scheduled users - Downlink
    LteMacScheduleList* scheduleListDl_;
    /**
     * Analyze gate of incoming packet
     * and call proper handler
     */
    virtual void handleMessage(cMessage *msg);

    /**
     * macSduRequest() sends a message to the RLC layer
     * requesting MAC SDUs (one for each CID),
     * according to the Schedule List.
     */
     virtual void macSduRequest();

    /**
     * macPduMake() creates MAC PDUs (one for each CID)
     * by extracting SDUs from Real Mac Buffers according
     * to the Schedule List (stored after scheduling).
     * It sends them to H-ARQ
     */
    virtual void macPduMake(MacCid cid);


    /**
     * bufferizePacket() is called every time a packet is
     * received from the upper layer
     */
    virtual bool bufferizePacket(cPacket* pkt);

    /**
     * handleUpperMessage() is called every time a packet is
     * received from the upper layer
     */
    virtual void handleUpperMessage(cPacket* pkt);

    /**
     * Main loop
     */
    virtual void handleSelfMessage();

    /**
     * Flush Tx H-ARQ buffers for all users
     */
    virtual void flushHarqBuffers();

    // update the status of the "mirror" RX-Harq Buffer for this node
    void storeRxHarqBufferMirror(MacNodeId id, LteHarqBufferRxD2DMirror* mirbuff);
    // get the reference to the "mirror" buffers
//    HarqRxBuffersMirror* getRxHarqBufferMirror();
    // delete the "mirror" RX-Harq Buffer for this node (useful at mode switch)
    void deleteRxHarqBufferMirror(MacNodeId id);
    // send the D2D Mode Switch signal to the transmitter of the given flow
    void sendModeSwitchNotification(MacNodeId srcId, MacNodeId dst, LteD2DMode oldMode, LteD2DMode newMode);

  public:

    LteMacUeAutoD2DRealistic();
    virtual ~LteMacUeAutoD2DRealistic();

    virtual bool isAutoD2DCapable()
    {
        return false;
    }

};

#endif
