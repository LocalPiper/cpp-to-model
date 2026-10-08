#pragma once

#include <string>

#include "../model.h"

namespace cpp2model {

class Drawer {
public:
    virtual ~Drawer() = default;
    virtual std::string render(const TreeModel& tree) const = 0;
    virtual const char* format() const = 0;
};

}  // namespace cpp2model