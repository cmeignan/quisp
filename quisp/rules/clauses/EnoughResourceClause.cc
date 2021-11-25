#include "EnoughResourceClause.h"

namespace quisp {
namespace rules {
namespace clauses {

bool EnoughResourceClause::check(std::multimap<int, IStationaryQubit*> resource) {
  bool enough = false;
  int num_free = 0;
  for (auto it = resource.begin(); it != resource.end(); ++it) {
    if (it->first == partner) {
      if (!it->second->isLocked()) {  // here must have loop
        num_free++;
      }
      if (num_free >= num_resource_required) {
        enough = true;
      }
    }
  }
  return enough;
}

//CM: why is there is no checkTerminate for this clause ?
bool EnoughResourceClause::checkTerminate(std::multimap<int, IStationaryQubit*> resource) const {
  // There is lot of things to change
  return (true); }
}  // namespace clauses
}  // namespace rules
}  // namespace quisp
