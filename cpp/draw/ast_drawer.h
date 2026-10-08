#pragma once

#include <string>

#include "drawer.h"

namespace cpp2model {

class AstDrawer final : public Drawer {
public:
    std::string render(const TreeModel& tree) const override;
    const char* format() const override { return "svg"; }
};

}  // namespace cpp2model