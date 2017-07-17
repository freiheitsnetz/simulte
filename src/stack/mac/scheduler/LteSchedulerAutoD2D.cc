//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "LteSchedulerAutoD2D.h"
#include "LteSchedulerUeAutoD2D.h"
#include "LteSchedulerUeAutoD2DUl.h"

/**
 * TODO:
 * - rimuovere i commenti dalle funzioni quando saranno implementate nel ueAutoD2D scheduler
 */

void LteSchedulerAutoD2D::setUeAutoD2DScheduler(LteSchedulerUeAutoD2D* UeAutoD2DScheduler)
{
    ueAutoD2DScheduler_ = UeAutoD2DScheduler;
    direction_ = ueAutoD2DScheduler_->direction_;
    mac_ = ueAutoD2DScheduler_->mac_;
    initializeGrants();
}


unsigned int LteSchedulerAutoD2D::requestGrant(MacCid cid, unsigned int bytes, bool& terminate, bool& active, bool& eligible , std::vector<BandLimit>* bandLim)
{
    return ueAutoD2DScheduler_->scheduleGrant(cid, bytes, terminate, active, eligible ,bandLim);
}

bool LteSchedulerAutoD2D::scheduleRetransmissions()
{
    return ueAutoD2DScheduler_->rtxschedule();
}

void LteSchedulerAutoD2D::scheduleRacRequests()
{
    //return (dynamic_cast<LteSchedulerAutoD2DAutoD2DEnbUl*>(eNbScheduler_))->serveRacs();
}

void LteSchedulerAutoD2D::requestRacGrant(MacNodeId nodeId)
{
    //return (dynamic_cast<LteSchedulerAutoD2DAutoD2DEnbUl*>(eNbScheduler_))->racGrantEnb(nodeId);
}

void LteSchedulerAutoD2D::schedule()
{
    prepareSchedule();
    commitSchedule();
}

void LteSchedulerAutoD2D::initializeGrants()
{
    if (direction_ == DL)
    {
        grantTypeMap_[CONVERSATIONAL] = aToGrantType(mac_->par("grantTypeConversationalDl"));
        grantTypeMap_[STREAMING] = aToGrantType(mac_->par("grantTypeStreamingDl"));
        grantTypeMap_[INTERACTIVE] = aToGrantType(mac_->par("grantTypeInteractiveDl"));
        grantTypeMap_[BACKGROUND] = aToGrantType(mac_->par("grantTypeBackgroundDl"));

        grantSizeMap_[CONVERSATIONAL] = mac_->par("grantSizeConversationalDl");
        grantSizeMap_[STREAMING] = mac_->par("grantSizeStreamingDl");
        grantSizeMap_[INTERACTIVE] = mac_->par("grantSizeInteractiveDl");
        grantSizeMap_[BACKGROUND] = mac_->par("grantSizeBackgroundDl");
    }
    else if (direction_ == UL)
    {
        grantTypeMap_[CONVERSATIONAL] = aToGrantType(mac_->par("grantTypeConversationalUl"));
        grantTypeMap_[STREAMING] = aToGrantType(mac_->par("grantTypeStreamingUl"));
        grantTypeMap_[INTERACTIVE] = aToGrantType(mac_->par("grantTypeInteractiveUl"));
        grantTypeMap_[BACKGROUND] = aToGrantType(mac_->par("grantTypeBackgroundUl"));

        grantSizeMap_[CONVERSATIONAL] = mac_->par("grantSizeConversationalUl");
        grantSizeMap_[STREAMING] = mac_->par("grantSizeStreamingUl");
        grantSizeMap_[INTERACTIVE] = mac_->par("grantSizeInteractiveUl");
        grantSizeMap_[BACKGROUND] = mac_->par("grantSizeBackgroundUl");
    }
    else
    {
        throw cRuntimeError("Unknown direction %d", direction_);
    }
}
