#pragma once

#include <string>

#include "../cfg/cfg_builder.h"

namespace cpp2model {

class CfgDrawer {
public:
    std::string render(const Cfg& cfg) const;
    const char* format() const { return "svg"; }
};

}  // namespace cpp2model
