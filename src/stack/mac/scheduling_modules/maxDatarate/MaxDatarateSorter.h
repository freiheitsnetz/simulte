/*
 * MaxDatarateSorter.h
 *
 *  Created on: Jul 26, 2017
 *      Author: kunterbunt
 */

#ifndef STACK_MAC_SCHEDULING_MODULES_MAXDATARATE_MAXDATARATESORTER_H_
#define STACK_MAC_SCHEDULING_MODULES_MAXDATARATE_MAXDATARATESORTER_H_

#include <vector>
#include <omnetpp.h>
#include <common/LteCommon.h>

class IdRatePair {
  public:
    IdRatePair(const MacCid& connectionId, const MacNodeId& from, const MacNodeId& to, double txPower, const double rate, const Direction& dir)
        : connectionId(connectionId), from(from), to(to), rate(rate), txPower(txPower), dir(dir) {}

    MacCid connectionId;
    MacNodeId from, to;
    double rate, txPower;
    Direction dir;
    bool reassigned = false;

    bool operator>(const IdRatePair& other) {
      return rate > other.rate;
    }

    bool operator<(const IdRatePair& other) {
      return !((*this) > other);
    }
};

/**
 * This container can be given <node id, throughput> pairs.
 * It always keeps the internal list sorted according to throughput.
 */
class MaxDatarateSorter {
  public:
    MaxDatarateSorter(size_t numBands);

    void put(const Band& band, const IdRatePair& idRatePair);

    /**
     * Removes 'id' from all elements in this container where element.from == 'id'.
     * @param id
     */
    void remove(const MacNodeId id);

    /**
     * Marks 'band' as 'reassigned'. Reassigned bands are not considered when looking for a best band.
     * @param band
     */
    void markBand(const Band &band, const bool reassigned);

    /**
     *
     * @param band
     * @param position
     * @return The xth best node according to throughput.
     */
    const IdRatePair& get(const Band& band, const size_t& position) const;

    /**
     * @param band
     * @return All <id, throughput> pairs for 'band'.
     */
    const std::vector<IdRatePair>& at(const Band &band) const;

    /**
     * @param band
     * @param dir
     * @return All <id, throughput> pairs for 'band' where 'id' wants to transmit in 'dir' direction.
     */
    const std::vector<IdRatePair> at(const Band& band, const Direction& dir) const;

    /**
     * @param band
     * @return All <id, throughput> pairs for 'band' where 'id' wants to transmit in any non-D2D direction.
     */
    const std::vector<IdRatePair> at_nonD2D(const Band& band) const;

    /**
     * @param id
     * @return The best band datarate-wise for 'id'. Bands marked as 'reassigned' are not considered.
     * @throws If no best band can be found.
     */
    const Band getBestBand(const MacNodeId& id) const;

    /**
     * @return The number of bands.
     */
    size_t size() const;

    std::string toString() const;

    std::string toString(std::string prefix) const;


  private:
    /**
     * The outer vector corresponds to the bands.
     * Each inner vector holds an always-sorted list of <id, rate> pairs in descending order rate-wise.
    **/
    std::vector<std::vector<IdRatePair>> mBandToIdRate;
    const size_t mNumBands;
};

#endif /* STACK_MAC_SCHEDULING_MODULES_MAXDATARATE_MAXDATARATESORTER_H_ */
