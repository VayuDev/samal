#include "samal_lib/GC.hpp"
#include "samal_lib/Program.hpp"
#include "samal_lib/VM.hpp"
#include <algorithm>
#include <sys/mman.h>

#undef NDEBUG
#include <cassert>
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

namespace samal {

GC::Region::Region(size_t len) {
    if(len > 0) {
        base = new uint8_t[len];
        assert(base);
        size = len;
    }
    offset = 0;
}
GC::Region::~Region() {
    if(base) {
        delete[] base;
        base = nullptr;
    }
}
GC::Region::Region(GC::Region&& other) noexcept {
    operator=(std::move(other));
}
GC::Region& GC::Region::operator=(GC::Region&& other) noexcept {
    if(base)
        free(base);
    base = other.base;
    size = other.size;
    offset = other.offset;
    other.offset = 0;
    other.size = 0;
    other.base = nullptr;
    return *this;
}

GC::GC(VM& vm, const VMParameters& params)
: mVM(vm) {
    mConfigFunctionsCallsPerGCRun = params.functionsCallsPerGCRun;
    mRegions[0] = Region{ static_cast<size_t>(params.initialHeapSize) };
    mRegions[1] = Region{ static_cast<size_t>(params.initialHeapSize) };
}
uint8_t* GC::alloc(int32_t size, size_t heapIndex) {
#ifdef x86_64_BIT_MODE
    assert(size % 8 == 0);
#endif
    // ensure that every memory allocation is divisible by 2 (used for pointer tagging for lambdas/functions)
    if(size % 2 == 1) {
        size += 1;
    }
    if(mRegions[heapIndex].offset + size >= mRegions[heapIndex].size) {
        // not enough memory, add to temporary allocations
        auto ptr = (uint8_t*)calloc(size, 1);
        assert(ptr);
        mTemporaryAllocations.emplace_back(TemporaryAllocation{{ptr, ArrayDeleter{}}, (size_t)size});
        assert((uintptr_t)mTemporaryAllocations.back().data.get() % 8 == 0);
        return ptr;
    }
    auto ptr = mRegions[heapIndex].top();
    mRegions[heapIndex].offset += size;

#ifdef x86_64_BIT_MODE
    assert((uintptr_t)ptr % 8 == 0);
#endif
    return ptr;
}
uint8_t* GC::alloc(int32_t size) {
    return alloc(size, mActiveRegion);
}
void GC::performGarbageCollection() {
    //Stopwatch stopwatch{"GC"};
    //printf("Before size: %i\n", (int)mRegions[mActiveRegion].offset);
    puts("Running GC");
    getOtherRegion().offset = 0;
    if(!mTemporaryAllocations.empty() || getOtherRegion().size < getActiveRegion().size) {
        // our other region that we're copying into might be too small, so we resize it to prevent any potential problems
        size_t totalTemporarySize = 0;
        for(auto& alloc: mTemporaryAllocations) {
            totalTemporarySize += alloc.len;
        }
        //printf("Resizing heap to %i\n", (int)(getActiveRegion().size + totalTemporarySize));
        getOtherRegion() = Region{getActiveRegion().size + totalTemporarySize};
        //printf("Resizing heap due to temporary allocations to %zu\n", getOtherRegion().size);
    }
    mVM.generateStacktrace([this](const uint8_t* ptr, const Datatype& type, const std::string& name) {
#ifdef x86_64_BIT_MODE
        assert((uintptr_t)ptr % 8 == 0);
#endif
        searchForPtrs((uint8_t*)ptr, type, ScanningHeapOrStack::Stack);
    }, {});
    mActiveRegion = !mActiveRegion;
    mTemporaryAllocations.clear();
    //printf("After size: %i\n", (int)mRegions[mActiveRegion].offset);
}
uint8_t* GC::copyToOther(uint8_t** ptr, size_t len) {
    assert(ptr);
    uint8_t* newPtr = getOtherRegion().top();
    assert(getOtherRegion().size >= getOtherRegion().offset + len);
    memcpy(newPtr, *ptr, len);
    getOtherRegion().offset += len;
    return newPtr;
}

void GC::searchForPtrs(uint8_t* ptr, const Datatype& type, ScanningHeapOrStack scanningHeapOrStack) {
    switch(type.getCategory()) {
    case DatatypeCategory::bool_:
    case DatatypeCategory::i32:
    case DatatypeCategory::i64:
    case DatatypeCategory::f64:
    case DatatypeCategory::char_:
    case DatatypeCategory::byte:
        break;
    case DatatypeCategory::tuple: {
        int32_t offset = type.getSizeOnStack();
        for(auto& element : type.getTupleInfo()) {
            offset -= element.getSizeOnStack();
            searchForPtrs(ptr + offset, element, scanningHeapOrStack);
        }
        break;
    }
    case DatatypeCategory::list: {
        auto** ptrToCurrent = (uint8_t**)ptr;
        while(true) {
#ifdef x86_64_BIT_MODE
            assert((uintptr_t)*ptrToCurrent % 8 == 0);
#endif
            if(*ptrToCurrent == nullptr)
                break;
            if(isInOtherRegion(*ptrToCurrent)) {
                break;
            }
            if(isInOtherRegion(**(uint8_t***)ptrToCurrent)) {
                memcpy(ptrToCurrent, *ptrToCurrent, 8);
                break;
            }
            auto containedTypeSize = type.getListContainedType().getSizeOnStack();
            searchForPtrs(*ptrToCurrent + 8, type.getListContainedType(), ScanningHeapOrStack::Heap);
            auto newPtr = copyToOther(ptrToCurrent, containedTypeSize + 8);
            auto oldPtrToCurrent = *ptrToCurrent;
            memcpy(ptrToCurrent, &newPtr, 8);

            ptrToCurrent = *(uint8_t***)ptrToCurrent;
            memcpy(oldPtrToCurrent, &newPtr, 8);
        }
        break;
    }
    case DatatypeCategory::function: {
        int32_t firstHalf;
        memcpy(&firstHalf, ptr, 4);
        if(firstHalf % 2 != 0) {
            // not lambda
            break;
        }
        // it's a lambda
        auto lambdaPtr = *(uint8_t**)ptr;
        assert(lambdaPtr);

        if(isInOtherRegion(*(uint8_t**)ptr)) {
            break;
        }
        if(isInOtherRegion(**(uint8_t***)ptr)) {
            memcpy(ptr, *(uint8_t**)ptr, 8);
            break;
        }

        int32_t sizeOfLambda = -1;
        memcpy(&sizeOfLambda, lambdaPtr, 4);
        assert(sizeOfLambda >= 0);
        sizeOfLambda += 16;

        int32_t capturedLambdaTypesId;
        memcpy(&capturedLambdaTypesId, lambdaPtr + 8, 4);

        size_t offset = sizeOfLambda;
        const auto& helperTuple = mVM.getProgram().auxiliaryDatatypes.at(capturedLambdaTypesId).getTupleInfo();
        for(size_t i = 0; i < helperTuple.size(); ++i) {
            offset -= helperTuple.at(i).getSizeOnStack();
            searchForPtrs(lambdaPtr + offset, helperTuple.at(i), ScanningHeapOrStack::Heap);
        }
        auto newPtr = copyToOther((uint8_t**)ptr, sizeOfLambda);
        memcpy(*(uint8_t**)ptr, &newPtr, 8);
        memcpy(ptr, &newPtr, 8);
        break;
    }
    case DatatypeCategory::struct_: {
        int32_t offset = type.getSizeOnStack();
        for(auto& field : type.getStructInfo().fields) {
            auto fieldType = field.type.completeWithSavedTemplateParameters();
            offset -= fieldType.getSizeOnStack();
            searchForPtrs(ptr + offset, fieldType, scanningHeapOrStack);
        }
        break;
    }
    case DatatypeCategory::enum_: {
#ifdef x86_64_BIT_MODE
        int64_t selectedIndex;
        memcpy(&selectedIndex, ptr, 8);
#else
        int32_t selectedIndex;
        memcpy(&selectedIndex, ptr, 4);
#endif
        auto &selectedField = type.getEnumInfo().fields.at(selectedIndex);
        int32_t offset = type.getEnumInfo().getLargestFieldSizePlusIndex();
        for(auto& element : selectedField.params) {
            auto elementType = element.completeWithSavedTemplateParameters();
            offset -= elementType.getSizeOnStack();
            searchForPtrs(ptr + offset, elementType, scanningHeapOrStack);
        }
        break;
    }
    case DatatypeCategory::pointer: {
        if(isInOtherRegion(*(uint8_t**)ptr)) {
            break;
        }
        if(isInOtherRegion(**(uint8_t***)ptr)) {
            memcpy(ptr, *(uint8_t**)ptr, 8);
            break;
        }
        searchForPtrs(*(uint8_t**)ptr, type.getPointerBaseType(), ScanningHeapOrStack::Heap);
        auto newPtr = copyToOther((uint8_t**)ptr, type.getPointerBaseType().getSizeOnStack());
        memcpy(*(uint8_t**)ptr, &newPtr, 8);
        memcpy(ptr, &newPtr, 8);
        break;
    }
    case DatatypeCategory::undetermined_identifier:
        assert(false);
        return searchForPtrs(ptr, completeTypeUntilNoLongerUndefined(type), scanningHeapOrStack);
    default:
        assert(false);
    }
}
void GC::requestCollection() {
    mFunctionCallsSinceLastRun++;
    if(mFunctionCallsSinceLastRun > mConfigFunctionsCallsPerGCRun) {
        performGarbageCollection();
        mFunctionCallsSinceLastRun = 0;
    }
}
GC::Region& GC::getActiveRegion() {
    return mRegions[mActiveRegion];
}
GC::Region& GC::getOtherRegion() {
    return mRegions[!mActiveRegion];
}
bool GC::isInOtherRegion(uint8_t* ptr) {
    return ptr >= getOtherRegion().base && ptr < getOtherRegion().top() && (uintptr_t)ptr % 2 == 0;
}
}
