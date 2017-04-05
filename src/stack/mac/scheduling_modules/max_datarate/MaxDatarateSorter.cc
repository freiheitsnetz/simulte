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

const IdRatePair& MaxDatarateSorter::get(const Band& band, const size_t& position) const {
  return mBandToIdRate.at(band).at(position);
}

const std::vector<IdRatePair>& MaxDatarateSorter::at(const Band& band) const {
    return mBandToIdRate.at(band);
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

const Band MaxDatarateSorter::getBestBand(const MacNodeId& id) const {
  Band bestBand(0);
  double bestRate = -1;
  // Go through all bands.
  for (Band band(0); band < mNumBands; band++) {
    // Grab all <id, rate> pairs.
    const std::vector<IdRatePair>& ratesVec = at(band);
    // Find the right pair.
    for (std::size_t i = 0; i < ratesVec.size(); i++) {
      if (ratesVec.at(i).from == id) {
        // Is this the best rate yet?
        if (ratesVec.at(i).rate > bestRate) {
          bestBand = band;
          bestRate = ratesVec.at(i).rate;
        }
        // Stop looking for this band.
        break;
      }
    }
  }
  return bestBand;
}
