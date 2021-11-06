
#pragma once
#include <set>
#include <string>
#include "mongo/util/string_map.h"
namespace mongo {
namespace fts {

  void loadStopWordMap( StringMap< std::set< std::string > >* m );
}
}
