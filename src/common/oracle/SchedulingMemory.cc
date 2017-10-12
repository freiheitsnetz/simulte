#include <stdexcept>
#include <iostream>
#include "common/oracle/SchedulingMemory.h"

using namespace std;

SchedulingMemory::SchedulingMemory() {}

SchedulingMemory::SchedulingMemory(const SchedulingMemory &other)
  : _memory(other._memory) {}

void SchedulingMemory::put(const MacNodeId id, const Band band, const bool isReassigned) {
  // Is there an item for 'id' already?
  try {
    MemoryItem &item = get(id);
    item.putBand(band, isReassigned);
  // If not, create it.
  } catch (const exception& e) {
    _memory.push_back(MemoryItem(id));
    MemoryItem &item = _memory.at(_memory.size() - 1);
    item.putBand(band, isReassigned);
  }
}

const SchedulingMemory::MemoryItem& SchedulingMemory::get(const MacNodeId &id) const {
  for (size_t i = 0; i < _memory.size(); i++) {
    if (_memory.at(i).getId() == id)
      return _memory.at(i);
  }
  throw invalid_argument("SchedulingMemory::get(invalid 'id') was called with id=" + std::to_string(id));
}

SchedulingMemory::MemoryItem& SchedulingMemory::get(const MacNodeId &id) {
  // Apparently Scott Meyers recommends doing this. Looks complicated,
  // but it just casts away the const.
  return const_cast<MemoryItem&>(static_cast<const SchedulingMemory&>(*this).get(id));
}

std::size_t SchedulingMemory::getNumberAssignedBands(const MacNodeId &id) const {
  return get(id).getNumberOfAssignedBands();
}

const std::vector<Band>& SchedulingMemory::getBands(const MacNodeId &id) const {
  return get(id).getBands();
}
const std::vector<bool>& SchedulingMemory::getReassignments(const MacNodeId &id) const {
  return get(id).getReassignments();
}

void SchedulingMemory::put(const MacNodeId id, const Direction dir) {
  // Is there an item for 'id' already?
  try {
    MemoryItem &item = get(id);
    item.setDir(dir);
    // If not, create it.
  } catch (const exception& e) {
    _memory.push_back(MemoryItem(id));
    MemoryItem &item = _memory.at(_memory.size() - 1);
    item.setDir(dir);
  }
}

const Direction &SchedulingMemory::getDirection(const MacNodeId &id) const {
  return get(id).getDir();
}
