#pragma once
#include "drawer.h"
#include "../ssa/ssa_builder.h"

namespace cpp2model {

class SsaDrawer final : public Drawer<SsaCfg> {
public:
    std::string render(const SsaCfg& ssa) const override;
    const char* format() const override { return "svg"; }
};

}  // namespace cpp2model