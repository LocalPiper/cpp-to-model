#pragma once
#include "drawer.h"
#include "../model.h"

namespace cpp2model {

class AstDrawer final : public Drawer<TreeModel> {
public:
    std::string render(const TreeModel& tree) const override;
    const char* format() const override { return "svg"; }
};

}  // namespace cpp2model