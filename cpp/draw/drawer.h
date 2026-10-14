#pragma once
#include <string>

namespace cpp2model {

template <class Model> class Drawer {
public:
  virtual ~Drawer() = default;
  virtual std::string render(const Model &model) const = 0;
  virtual const char *format() const = 0;
};

} // namespace cpp2model