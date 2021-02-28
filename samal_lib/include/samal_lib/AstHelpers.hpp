#pragma once
#include "Datatype.hpp"
#include <cstdint>
#include <stdexcept>
#include "Forward.hpp"
#include "Util.hpp"
#include <map>

namespace samal {

struct IdentifierId {
    int32_t variableId = 0;
    int32_t templateId = 0;
};

struct EnumField {
    std::string name;
    std::vector<Datatype> params;
    inline bool operator==(const EnumField& other) const {
        return name == other.name && params == other.params;
    }
    inline bool operator!=(const EnumField& other) const {
        return !operator==(other);
    }
};

static inline bool operator<(const IdentifierId& a, const IdentifierId& b) {
    if(a.variableId < b.variableId)
        return true;
    if(a.templateId < b.templateId)
        return true;
    return false;
}

using TemplateInstantiationInfo = std::vector<std::vector<Datatype>>;
// create a map that maps e.g. T => i32 if you call fib<i32>,
// which is then used to determine the return & param types of fib<i32>
// which could be fib<T> -> T, so it becomes i32
static inline std::map<std::string, Datatype> createTemplateParamMap(const std::vector<std::string>& identifierTemplateInfo /*<T>*/, const std::vector<Datatype>& templateParameterInfo /*<i32>*/) {
    if(identifierTemplateInfo.size() != templateParameterInfo.size()) {
        throw std::runtime_error{ "Template parameter sizes don't match" };
    }
    std::map<std::string, Datatype> ret;
    size_t i = 0;
    for(auto& identifierTemplateParameter : identifierTemplateInfo) {
        if(i >= templateParameterInfo.size()) {
            return ret;
        }
        ret.emplace(identifierTemplateParameter, templateParameterInfo.at(i));
        ++i;
    }
    return ret;
};

}