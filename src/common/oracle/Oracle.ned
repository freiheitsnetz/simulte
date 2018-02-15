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

package lte.common.oracle;

simple Oracle {            	    
    parameters:
        // Used for CQI computation. Copy over from your channel.xml.
        double targetBler = default(0.001);
        double lambdaMinTh = default(0.02);
        double lambdaMaxTh = default(0.2);
        double lambdaRatioTh = default(20);
        // The simulation time at which final configuration takes place.
        double configTimepoint = default(0.005); 
        // The entity will take a snapshot of the some network statistics every x seconds.
        double updateInterval = default(0.05); 
        // 180kHz is LTE's bandwidth per resource block as per its specification.
        double resourceBlockBandwidth @unit(kHz) = default(180kHz);
        bool recordBandAllocation = default(false);
        string historyFilename = default("scheduling_history");
        // The LteSchedulerEnb::scheduleGrant sets TERMINATE=true and returns if
        // a codeword is already assigned. This means band reassignment is not supported right now.
        // If this is set to 'false', then this checking is disabled in the function.
        bool considerTerminateFlag = default(true);
        
        // Whether the REASSIGNMENT scheduler should go through the connections 'forward' or 'backward'.
        string reassignmentSchedulingDirection = default("forward");
        
        // D2D Penalty value for Transferable Utility scheduler.
        double d2dPenalty = default(1.0);
        
        // D2D maximum/minimum transmission powers used by the Stackelberg Game scheduler.
        double stackelberg_d2dPowerLimit_max = default(398.0); // in mW, equals 26dBm
        double stackelberg_d2dPowerLimit_min = default(100.0); // 20 dBm
        double stackelberg_beta = default(2.0);
        double stackelberg_delta = default(0.04);
        string stackelberg_scheduleLeaders = default("RR"); // Which scheduler to use to schedule leader UEs: RR=RoundRobin
        bool stackelberg_setTxPower = default(false); // Whether to set follower's transmission power to what's been computed while playing the game.
    @display("i=block/table");
    @class(Oracle);    
}
