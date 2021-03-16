#include "samal_lib/ExternalVMValue.hpp"
#include "samal_lib/VM.hpp"
#include "peg_parser/PegUtil.hpp"
#include <cassert>
#include <sstream>

namespace samal {

ExternalVMValue::~ExternalVMValue() = default;

ExternalVMValue::ExternalVMValue(VM& vm, Datatype type, decltype(mValue) val)
: mVM(&vm), mType(std::move(type)), mValue(std::move(val)) {
}
ExternalVMValue ExternalVMValue::wrapInt32(VM& vm, int32_t val) {
    return ExternalVMValue(vm, Datatype::createSimple(DatatypeCategory::i32), val);
}
ExternalVMValue ExternalVMValue::wrapInt64(VM& vm, int64_t val) {
    return ExternalVMValue(vm, Datatype::createSimple(DatatypeCategory::i64), val);
}
const Datatype& ExternalVMValue::getDatatype() const& {
    return mType;
}
std::vector<uint8_t> ExternalVMValue::toStackValue(VM& vm) const {
    switch(mType.getCategory()) {
    case DatatypeCategory::i32: {
        auto val = std::get<int32_t>(mValue);
#ifdef x86_64_BIT_MODE
        return std::vector<uint8_t>{ (uint8_t)(val >> 0), (uint8_t)(val >> 8), (uint8_t)(val >> 16), (uint8_t)(val >> 24), 0, 0, 0, 0 };
#else
        return std::vector<uint8_t>{ (uint8_t)(val >> 0), (uint8_t)(val >> 8), (uint8_t)(val >> 16), (uint8_t)(val >> 24) };
#endif
    }
    case DatatypeCategory::function:
    case DatatypeCategory::i64: {
        auto val = std::get<int64_t>(mValue);
        return std::vector<uint8_t>{ (uint8_t)(val >> 0), (uint8_t)(val >> 8), (uint8_t)(val >> 16), (uint8_t)(val >> 24), (uint8_t)(val >> 32), (uint8_t)(val >> 40), (uint8_t)(val >> 48), (uint8_t)(val >> 56) };
    }
    case DatatypeCategory::tuple: {
        std::vector<uint8_t> bytes;
        for(auto& child : std::get<std::vector<ExternalVMValue>>(mValue)) {
            auto childBytes = child.toStackValue(vm);
            bytes.insert(bytes.end(), childBytes.begin(), childBytes.end());
        }
        return bytes;
    }
    default:
        assert(false);
    }
}
std::string ExternalVMValue::dump() const {
    std::string ret;
    switch(mType.getCategory()) {
    case DatatypeCategory::char_:
        ret += '\'';
        ret += peg::encodeUTF8Codepoint(std::get<int32_t>(mValue));
        ret += '\'';
        break;
    case DatatypeCategory::i32:
        ret += std::to_string(std::get<int32_t>(mValue));
        break;
    case DatatypeCategory::i64:
        ret += std::to_string(std::get<int64_t>(mValue)) + "i64";
        break;
    case DatatypeCategory::tuple:
        ret += "(";
        for(auto& child : std::get<std::vector<ExternalVMValue>>(mValue)) {
            ret += child.dump();
            if(&child != &std::get<std::vector<ExternalVMValue>>(mValue).back()) {
                ret += ", ";
            }
        }
        ret += ")";
        break;
    case DatatypeCategory::function: {
        std::stringstream ss;
        ss << std::hex << std::get<int64_t>(mValue);
        ret += ss.str();
        break;
    }
    case DatatypeCategory::list: {
        if(isWrappingString()) {
            ret += '"';
            ret += toCPPString();
            ret += '"';
            break;
        }
        ret += "[";
        auto* current = std::get<const uint8_t*>(mValue);
        while(current != nullptr) {
            ret += ExternalVMValue::wrapFromPtr(mType.getListContainedType(), *mVM, current + 8).dump();
            current = *(uint8_t**)current;
            if(current != nullptr)
                ret += ", ";
        }
        ret += "]";
        break;
    }
    case DatatypeCategory::pointer: {
        ret += "$";
        ret += ExternalVMValue::wrapFromPtr(mType.getPointerBaseType(), *mVM, std::get<const uint8_t*>(mValue)).dump();
        break;
    }
    case DatatypeCategory::struct_: {
        const auto& structVal = std::get<StructValue>(mValue);
        ret += structVal.name;
        ret += "{";
        for(auto& field : structVal.fields) {
            ret += field.name + ": " + field.value.dump();
            if(&field != &structVal.fields.back()) {
                ret += ", ";
            }
        }
        ret += "}";
        break;
    }
    case DatatypeCategory::enum_: {
        // TODO dump template params
        const auto& enumVal = std::get<EnumValue>(mValue);
        ret += enumVal.name;
        ret += "::";
        ret += enumVal.selectedFieldName;
        ret += "{";
        for(auto& field: enumVal.elements) {
            ret += field.dump();
            if(&field != &enumVal.elements.back()) {
                ret += ", ";
            }
        }
        ret += "}";
        break;
    }
    default:
        ret += "<unknown>";
    }
    return ret;
}
ExternalVMValue ExternalVMValue::wrapStackedValue(Datatype type, VM& vm, size_t stackOffset) {
    auto stackTop = vm.getStack().getTopPtr();
    return wrapFromPtr(type, vm, stackTop + stackOffset);
}
ExternalVMValue ExternalVMValue::wrapEmptyTuple(VM& vm) {
    return ExternalVMValue(vm, Datatype::createEmptyTuple(), std::vector<ExternalVMValue>{});
}
ExternalVMValue ExternalVMValue::wrapFromPtr(Datatype type, VM& vm, const uint8_t* ptr) {
    switch(type.getCategory()) {
    case DatatypeCategory::i32:
    case DatatypeCategory::char_:
        return ExternalVMValue{ vm, type, *(int32_t*)(ptr) };
    case DatatypeCategory::function:
    case DatatypeCategory::i64:
        return ExternalVMValue{ vm, type, *(int64_t*)(ptr) };
    case DatatypeCategory::tuple: {
        std::vector<ExternalVMValue> children;
        auto reversedTypes = type.getTupleInfo();
        std::reverse(reversedTypes.begin(), reversedTypes.end());

        children.reserve(reversedTypes.size());
        for(auto& childType : reversedTypes) {
            children.emplace_back(ExternalVMValue::wrapFromPtr(childType, vm, ptr));
            ptr += childType.getSizeOnStack();
        }
        std::reverse(children.begin(), children.end());
        return ExternalVMValue{ vm, type, std::move(children) };
    }
    case DatatypeCategory::struct_: {
        StructValue val;
        val.name = type.getStructInfo().name;
        int32_t offset = type.getSizeOnStack();
        for(auto& field: type.getStructInfo().fields) {
            offset -= field.type.completeWithSavedTemplateParameters().getSizeOnStack();
            val.fields.push_back(StructValue::Field{ExternalVMValue::wrapFromPtr(field.type, vm, ptr + offset), field.name});
        }
        return ExternalVMValue{vm, type, std::move(val)};
    }
    case DatatypeCategory::enum_: {
        EnumValue val;
        val.name = type.getEnumInfo().name;
#ifdef x86_64_BIT_MODE
        int64_t index = -1;
        memcpy(&index, ptr, 8);
#else
        int32_t index = -1;
        memcpy(&index, ptr, 4);
#endif
        auto offset = type.getEnumInfo().getLargestFieldSizePlusIndex();
        const auto& selectedField = type.getEnumInfo().fields.at(index);
        val.selectedFieldName = selectedField.name;
        for(auto& element: selectedField.params) {
            offset -= element.completeWithSavedTemplateParameters().getSizeOnStack();
            val.elements.push_back(ExternalVMValue::wrapFromPtr(element, vm, ptr + offset));
        }
        return ExternalVMValue{vm, type, std::move(val)};
    }
    case DatatypeCategory::pointer:
    case DatatypeCategory::list: {
        return ExternalVMValue{ vm, type, *(uint8_t**)(ptr) };
    }
    case DatatypeCategory::undetermined_identifier: {
        auto completedType = type.completeWithSavedTemplateParameters();
        if(completedType.getCategory() == DatatypeCategory::undetermined_identifier) {
            throw std::runtime_error{ "Recursive undetermined identifier " + completedType.getUndeterminedIdentifierString() };
        }
        return wrapFromPtr(completedType, vm, ptr);
    }
    default:
        return ExternalVMValue{ vm, type, std::monostate{} };
    }
}
bool ExternalVMValue::isWrappingString() const {
    return mType == Datatype::createListType(Datatype::createSimple(DatatypeCategory::char_));
}
std::string ExternalVMValue::toCPPString() const {
    if(!isWrappingString()) {
        throw std::runtime_error{"This is not wrapping a string, it's wrapping a " + mType.toString() };
    }
    std::string ret;
    auto* current = std::get<const uint8_t*>(mValue);
    while(current != nullptr) {
        int32_t charValue;
        memcpy(&charValue, current + 8, 4);
        ret += peg::encodeUTF8Codepoint(charValue);
        current = *(uint8_t**)current;
    }
    return ret;
}
}