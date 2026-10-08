#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace cpp2model {

class WrappingStrategy {
public:
    virtual ~WrappingStrategy() = default;
    virtual std::string wrap(std::string_view raw) const = 0;
};

class PassthroughStrategy final : public WrappingStrategy {
public:
    std::string wrap(std::string_view raw) const override;
};

class InputFacade {
public:
    explicit InputFacade(std::unique_ptr<WrappingStrategy> strategy);

    std::string wrap(std::string_view raw) const;

private:
    std::unique_ptr<WrappingStrategy> m_strategy;
};

}  // namespace cpp2model