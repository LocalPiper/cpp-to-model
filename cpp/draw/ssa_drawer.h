#pragma once
#include "../ssa/ssa_builder.h"
#include "drawer.h"

namespace cpp2model {

class SsaDrawer final : public Drawer<SsaCfg> {
public:
  std::string render(const SsaCfg &ssa) const override;
  const char *format() const override { return "svg"; }
};

} // namespace cpp2model