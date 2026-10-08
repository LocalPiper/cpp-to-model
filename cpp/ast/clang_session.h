#pragma once

#include <memory>
#include <string>
#include <vector>

namespace clang {
class FrontendAction;
}

namespace cpp2model {

struct ParseSummary {
    std::vector<std::string> diagnostics;
    unsigned errors = 0;
    unsigned warnings = 0;
};


bool run_tool_on_code(const std::string& code,
                      std::unique_ptr<clang::FrontendAction> action,
                      ParseSummary& out);

}  // namespace cpp2model