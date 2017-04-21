#include <MaxDatarateSorter.h>

MaxDatarateSorter::MaxDatarateSorter(size_t numBands) : mNumBands(numBands) {
  for (size_t i = 0; i < numBands; i++)
    mBandToIdRate.push_back(std::vector<IdRatePair>());
}

void MaxDatarateSorter::put(const Band &band, const IdRatePair& idRatePair) {
  // Grab the band's vector.
  std::vector<IdRatePair>& list = mBandToIdRate.at(band);
  // Go through list.
  for (size_t i = 0; i < list.size(); i++) {
    IdRatePair current = list.at(i);
    // Is the given rate better than the one at this position?
    if (current < idRatePair) {
      // Then insert this one before it.
      list.insert(list.begin() + i, idRatePair);
      return;
    }
  }
  // The new value must have a worse rate than every list member, so put it at the back.
  list.push_back(idRatePair);
}

const std::vector<IdRatePair>& MaxDatarateSorter::at(const Band& band) const {
    return mBandToIdRate.at(band);
}

const std::vector<IdRatePair> MaxDatarateSorter::at(const Band &band, const Direction &dir) const {
  std::vector<IdRatePair> allPairs = mBandToIdRate.at(band);
  for (std::vector<IdRatePair>::iterator it = allPairs.begin(); it < allPairs.end(); it++) {
    IdRatePair& pair = *it;
    if (pair.dir != dir) {
      allPairs.erase(it);
      // Size has decreased by one, so decrease 'it' in order to target next element correctly.
      it--;
    }
  }
  return allPairs;
}

const std::vector<IdRatePair> MaxDatarateSorter::at_nonD2D(const Band &band) const {
  std::vector<IdRatePair> allPairs = mBandToIdRate.at(band);
  for (std::vector<IdRatePair>::iterator it = allPairs.begin(); it < allPairs.end(); it++) {
    IdRatePair& pair = *it;
    if (pair.dir == Direction::D2D) {
      allPairs.erase(it);
      // Size has decreased by one, so decrease 'it' in order to target next element correctly.
      it--;
    }
  }
  return allPairs;
}

const IdRatePair& MaxDatarateSorter::get(const Band& band, const size_t& position) const {
    return at(band).at(position);
}

size_t MaxDatarateSorter::size() const {
    return mBandToIdRate.size();
}

std::string MaxDatarateSorter::toString() const {
  std::string descr = "";
  for (size_t i = 0; i < mBandToIdRate.size(); i++) {
    descr += "Band " + std::to_string(i) + ":\n";
    for (size_t j = 0; j < mBandToIdRate.at(i).size(); j++) {
      const IdRatePair& pair = mBandToIdRate.at(i).at(j);
      descr += "\t" + std::to_string(pair.from) + " -" + dirToA(pair.dir) + "-> "
              + std::to_string(pair.to) + " @" + std::to_string(pair.txPower) + "dB"
              + " with throughput " + std::to_string(pair.rate) + "\n";
    }
  }
  return descr;
}

void MaxDatarateSorter::remove(const MacNodeId id) {
  for (size_t i = 0; i < mBandToIdRate.size(); i++) {
    std::vector<IdRatePair>& currentBandVec = mBandToIdRate.at(i);
    for (size_t j = 0; j < currentBandVec.size(); j++) {
      if (currentBandVec.at(j).from == id)
        currentBandVec.erase(currentBandVec.begin() + j);
    }
  }
}

void MaxDatarateSorter::markBand(const Band &band, const bool reassigned) {
  // Grab all <id, rate> pairs.
  std::vector<IdRatePair>& ratesVec = mBandToIdRate.at(band);
  for (size_t i = 0; i < ratesVec.size(); i++) {
    IdRatePair& item = ratesVec.at(i);
    item.reassigned = reassigned;
  }
}

const Band MaxDatarateSorter::getBestBand(const MacNodeId& id) const {
  Band bestBand(0);
  double bestRate = -1;
  // Go through all bands.
  for (Band band(0); band < mNumBands; band++) {
    // Grab all <id, rate> pairs.
    const std::vector<IdRatePair>& ratesVec = at(band);
    // Find the right pair.
    for (size_t i = 0; i < ratesVec.size(); i++) {
      const IdRatePair& item = ratesVec.at(i);
      // Ignore already reassigned bands.
      if (item.reassigned)
        continue;
      if (item.from == id) {
        // Is this the best rate yet?
        if (item.rate > bestRate) {
          bestBand = band;
          bestRate = item.rate;
        }
        // Stop looking for this band.
        break;
      }
    }
  }
  if (bestRate == -1)
    throw std::runtime_error("MaxDatarateSorter::getBestBand called but no bands available.");
  return bestBand;
}
