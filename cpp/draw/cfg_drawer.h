#pragma once
#include "../cfg/cfg_builder.h"
#include "drawer.h"

namespace cpp2model {

class CfgDrawer final : public Drawer<Cfg> {
public:
  std::string render(const Cfg &cfg) const override;
  const char *format() const override { return "svg"; }
};

} // namespace cpp2model