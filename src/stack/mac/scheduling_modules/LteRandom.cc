#include "LteRandom.h"
#include "LteSchedulerEnb.h"

void LteRandom::prepareSchedule()
{
    EV << NOW << " LteRandom::schedule " << eNbScheduler_->mac_->getMacNodeId() << endl;

    activeConnectionTempSet_ = activeConnectionSet_;
    int activeSetSize = activeConnectionTempSet_.size();
    eNbScheduler_->mac_->emit(numActiveConnections_, activeSetSize);

    try
    {
        for(int i = 0; i < activeSetSize; i++)
        {
            //Draw a random CID to schedule
            int randomCid =  getRandomInt(activeSetSize);

            ActiveSet::iterator it = activeConnectionTempSet_.begin();
            advance(it,randomCid-1);

            //Chosen cid to schedule
            MacCid chosenCid = *it;

            //Random bytes to allocate, between 0 and 'randomSchedulerBytes', set in LteMac.ned
            unsigned int randomBytes = getRandomInt(mac_->par("randomSchedulerBytes"));

            //Schedule the picked Cid with the random bytes
            bool terminate = false;
            bool active = true;
            bool eligible = true;

            Direction dir;
            if (direction_ == UL)
                dir = (MacCidToLcid(chosenCid) == D2D_SHORT_BSR) ? D2D : direction_;
            else
                dir = DL;
            MacNodeId nodeId = MacCidToNodeId(chosenCid);
            const UserTxParams& info = eNbScheduler_->mac_->getAmc()->computeTxParams(nodeId,dir);

            try
            {
                unsigned int granted = requestGrant (chosenCid, randomBytes, terminate, active, eligible);
                EV << NOW << "LteRandom::schedule granted " << granted << " bytes to connection " << chosenCid << endl;
                // Exit immediately if the terminate flag is set.
                if ( terminate ) break;

                if ( ! active || ! eligible )
                {
                    EV << NOW << "LteRandom::schedule  connection " << chosenCid << " was found ineligible" << endl;
                }

                // Set the connection as inactive if indicated by the grant ().
                if ( ! active )
                {
                    EV << NOW << "LteRandom::schedule scheduling connection " << chosenCid << " set to inactive " << endl;

                    activeConnectionTempSet_.erase (*it);
                }
            }
            catch (const std::out_of_range& e) {
                EV << NOW << " LteMaxDatarate::prepareSchedule failed." << std::endl;
            }
        }
    }
    catch (const std::out_of_range& e) {
        EV << NOW << " LteMaxDatarate::prepareSchedule failed." << std::endl;
    }
}

void LteRandom::commitSchedule()
{
    activeConnectionSet_ = activeConnectionTempSet_;
}

void LteRandom::updateSchedulingInfo()
{
}

void LteRandom::notifyActiveConnection(MacCid cid)
{
    EV << NOW << "LteRandom::notify CID notified " << cid << endl;
    activeConnectionSet_.insert(cid);
}

void LteRandom::removeActiveConnection(MacCid cid)
{
    EV << NOW << "LteRandom::remove CID removed " << cid << endl;
    activeConnectionSet_.erase(cid);
}

int LteRandom::getRandomInt(int limit)
{
    static int randomSchedulerSeed_ = mac_->par("randomSchedulerSeed"); // obtain a random number from hardware
    static std::mt19937 eng(randomSchedulerSeed_); // seed the generator
    std::uniform_int_distribution<> distr(0, limit); //
    return distr(eng);
}
