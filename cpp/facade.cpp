#include "facade.h"

#include <string>
#include <utility>

namespace cpp2model {

std::string PassthroughStrategy::wrap(std::string_view raw) const {
    return std::string(raw);
}

InputFacade::InputFacade(std::unique_ptr<WrappingStrategy> strategy)
    : m_strategy(std::move(strategy)) {}

std::string InputFacade::wrap(std::string_view raw) const {
    return m_strategy ? m_strategy->wrap(raw) : std::string(raw);
}

}  // namespace cpp2model