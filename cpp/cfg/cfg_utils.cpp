#include "cfg_utils.h"

namespace cpp2model {

std::string stringify_func_decl(const std::string& func_name, const std::vector<std::string>& func_params) {
    std::string res = func_name + "(";
    for (const std::string& par : func_params) {
        if (par.empty()) continue;
        res += par + ",";
    }
    if (res.back() == ',') res.pop_back();
    res.push_back(')');
    return res;
}

} // namespace cpp2model