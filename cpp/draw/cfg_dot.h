#pragma once

#include <string>

#include "../cfg/cfg_builder.h"
#include "dot_common.h"

namespace cpp2model {

CfgColors cfgBlockColors(const Cfg &cfg, int bbIndex);

std::string cfgToDot(const Cfg &cfg);

} // namespace cpp2model