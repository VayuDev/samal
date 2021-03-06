#include "samal_lib/VM.hpp"
#include "samal_lib/ExternalVMValue.hpp"
#include "samal_lib/Instruction.hpp"
#include "samal_lib/StackInformationTree.hpp"
#include "samal_lib/Util.hpp"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace samal {

#ifdef SAMAL_ENABLE_JIT
struct JitReturn {
    int32_t ip;                   // lower 4 bytes of rax
    int32_t stackSize;            // upper 4 bytes of rax
    int32_t nativeFunctionToCall; // lower 4 bytes of rdx
};

class JitCode : public Xbyak::CodeGenerator {
public:
    explicit JitCode(const std::vector<uint8_t>& instructions)
    : Xbyak::CodeGenerator(4096 * 4, Xbyak::AutoGrow) {
        setDefaultJmpNEAR(true);
        // prelude
        push(rbx);
        push(rbp);
        push(r10);
        push(r11);
        push(r12);
        push(r13);
        push(r14);
        push(r15);
        mov(r15, rsp);

        // gc pointer
        const auto& gcPointer = r14;
        mov(gcPointer, rcx);

        // GC::requestCollection pointer
        const auto& gcRequestCollectionPointer = r8;
        mov(gcRequestCollectionPointer, r8);

        // ip
        const auto& ip = r11;
        [[maybe_unused]] const auto& ip32 = r11d;
        mov(ip, rdi);
        // stack ptr
        const auto& originalStackPointer = r12;
        mov(originalStackPointer, rsi);
        // stack size
        const auto& stackSize = r13;
        mov(stackSize, rdx);

        const auto& tableRegister = r9;
        mov(tableRegister, "JumpTable");

        const auto& nativeFunctionIdRegister = r10;
        mov(nativeFunctionIdRegister, 0);

        mov(rsp, originalStackPointer);

        std::vector<std::pair<int32_t, Xbyak::Label>> instructionLocationLabels;
        std::vector<std::pair<up<Xbyak::Label>, int32_t>> directJumpLocationLabels;
        auto jumpWithIp = [&] {
            jmp(ptr[tableRegister + ip * sizeof(void*)]);
        };
        auto isInstructionAtIpJittable = [&](int32_t ip) -> bool {
            auto ins = static_cast<Instruction>(instructions.at(ip));
            switch(ins) {
            case Instruction::PUSH_8:
            case Instruction::POP_N_BELOW:
            case Instruction::ADD_I32:
            case Instruction::SUB_I32:
            case Instruction::MUL_I32:
            case Instruction::DIV_I32:
            case Instruction::MODULO_I32:
            case Instruction::COMPARE_LESS_THAN_I32:
            case Instruction::COMPARE_MORE_THAN_I32:
            case Instruction::COMPARE_LESS_EQUAL_THAN_I32:
            case Instruction::COMPARE_MORE_EQUAL_THAN_I32:
            case Instruction::COMPARE_EQUALS_I32:
            case Instruction::COMPARE_NOT_EQUALS_I32:
            case Instruction::ADD_I64:
            case Instruction::SUB_I64:
            case Instruction::MUL_I64:
            case Instruction::DIV_I64:
            case Instruction::MODULO_I64:
            case Instruction::COMPARE_LESS_THAN_I64:
            case Instruction::COMPARE_MORE_THAN_I64:
            case Instruction::COMPARE_LESS_EQUAL_THAN_I64:
            case Instruction::COMPARE_MORE_EQUAL_THAN_I64:
            case Instruction::COMPARE_EQUALS_I64:
            case Instruction::COMPARE_NOT_EQUALS_I64:
            case Instruction::LOGICAL_OR:
            case Instruction::LOGICAL_NOT:
            case Instruction::LOGICAL_AND:
            case Instruction::JUMP_IF_FALSE:
            case Instruction::JUMP:
            case Instruction::REPUSH_FROM_N:
            case Instruction::RETURN:
            case Instruction::CALL:
            case Instruction::LOAD_FROM_PTR:
            case Instruction::LIST_GET_TAIL:
            case Instruction::IS_LIST_EMPTY:
            case Instruction::RUN_GC:
            case Instruction::NOOP:
                    return true;
            }
            return false;
        };
        jumpWithIp();

        // Start executing some Code!
        for(int32_t i = 0; i < static_cast<int32_t>(instructions.size());) {
            auto ins = static_cast<Instruction>(instructions.at(i));
            if(!isInstructionAtIpJittable(i)) {
                // we hit an instruction that we don't know, so exit the jit
                jmp("AfterJumpTable");
                i += instructionToWidth(ins);
                continue;
            }
            auto nextInstruction = [&] {
                return static_cast<Instruction>(instructions.at(i + instructionToWidth(ins)));
            };
            auto nextInstructionWidth = [&] {
                return instructionToWidth(nextInstruction());
            };
            bool shouldAutoIncrement = true;

            for(auto it = directJumpLocationLabels.begin(); it != directJumpLocationLabels.end();) {
                if(it->second == i) {
                    L(*it->first);
                    directJumpLocationLabels.erase(it);
                } else {
                    it++;
                }
            }
            instructionLocationLabels.emplace_back(std::make_pair((uint32_t)i, L()));
            switch(ins) {
            case Instruction::PUSH_8: {
                auto amount = *(int64_t*)&instructions.at(i + 1);
                if(nextInstruction() == Instruction::SUB_I32) {
                    sub(qword[rsp], amount);
                    add(ip, instructionToWidth(ins) + nextInstructionWidth());
                    i += instructionToWidth(ins) + nextInstructionWidth();
                    shouldAutoIncrement = false;
                } else if(nextInstruction() == Instruction::ADD_I32) {
                    add(qword[rsp], amount);
                    add(ip, instructionToWidth(ins) + nextInstructionWidth());
                    i += instructionToWidth(ins) + nextInstructionWidth();
                    shouldAutoIncrement = false;
                } else if(nextInstruction() == Instruction::DIV_I32) {
                    pop(rax);
                    mov(rbx, amount);
                    cdq();
                    idiv(ebx);
                    push(rax);
                    add(ip, instructionToWidth(ins) + nextInstructionWidth());
                    i += instructionToWidth(ins) + nextInstructionWidth();
                    shouldAutoIncrement = false;
                    // TODO optimize MUL_I32, DIV_I64 and MUL_I64 as well
                } else {
                    mov(rax, *(uint64_t*)&instructions.at(i + 1));
                    push(rax);
                }
                break;
            }
            case Instruction::ADD_I32:
                pop(rax);
                add(dword[rsp], eax);
                break;
            case Instruction::SUB_I32:
                pop(rax);
                sub(dword[rsp], eax);
                break;
            case Instruction::MUL_I32:
                pop(rbx);
                imul(ebx, qword[rsp]);
                mov(qword[rsp], rbx);
                break;
            case Instruction::DIV_I32:
                pop(rbx);
                pop(rax);
                cdq();
                idiv(ebx);
                push(rax);
                break;
            case Instruction::MODULO_I32:
                pop(rbx);
                pop(rax);
                cdq();
                idiv(ebx);
                push(rdx);
                break;
            case Instruction::COMPARE_LESS_THAN_I32:
                pop(rax);
                pop(rbx);
                cmp(ebx, eax);
                mov(rax, 0);
                setl(al);
                push(rax);
                break;
            case Instruction::COMPARE_LESS_EQUAL_THAN_I32:
                pop(rax);
                pop(rbx);
                cmp(ebx, eax);
                mov(rax, 0);
                setle(al);
                push(rax);
                break;
            case Instruction::COMPARE_MORE_THAN_I32:
                pop(rax);
                pop(rbx);
                cmp(ebx, eax);
                mov(rax, 0);
                setg(al);
                push(rax);
                break;
            case Instruction::COMPARE_MORE_EQUAL_THAN_I32:
                pop(rax);
                pop(rbx);
                cmp(ebx, eax);
                mov(rax, 0);
                setge(al);
                push(rax);
                break;
            case Instruction::COMPARE_EQUALS_I32:
                pop(rax);
                pop(rbx);
                cmp(ebx, eax);
                mov(rax, 0);
                sete(al);
                push(rax);
                break;
            case Instruction::COMPARE_NOT_EQUALS_I32:
                pop(rax);
                pop(rbx);
                cmp(ebx, eax);
                mov(rax, 0);
                setne(al);
                push(rax);
                break;
            case Instruction::ADD_I64:
                pop(rax);
                add(qword[rsp], rax);
                break;
            case Instruction::SUB_I64:
                pop(rax);
                sub(qword[rsp], rax);
                break;
            case Instruction::MUL_I64:
                pop(rbx);
                imul(rbx, qword[rsp]);
                mov(qword[rsp], rbx);
                break;
            case Instruction::DIV_I64:
                pop(rbx);
                pop(rax);
                cqo();
                idiv(rbx);
                push(rax);
                break;
            case Instruction::MODULO_I64:
                pop(rbx);
                pop(rax);
                cqo();
                idiv(rbx);
                push(rdx);
                break;
            case Instruction::COMPARE_LESS_THAN_I64:
                pop(rax);
                pop(rbx);
                cmp(rbx, rax);
                mov(rax, 0);
                setl(al);
                push(rax);
                break;
            case Instruction::COMPARE_LESS_EQUAL_THAN_I64:
                pop(rax);
                pop(rbx);
                cmp(rbx, rax);
                mov(rax, 0);
                setle(al);
                push(rax);
                break;
            case Instruction::COMPARE_MORE_THAN_I64:
                pop(rax);
                pop(rbx);
                cmp(rbx, rax);
                mov(rax, 0);
                setg(al);
                push(rax);
                break;
            case Instruction::COMPARE_MORE_EQUAL_THAN_I64:
                pop(rax);
                pop(rbx);
                cmp(rbx, rax);
                mov(rax, 0);
                setge(al);
                push(rax);
                break;
            case Instruction::COMPARE_EQUALS_I64:
                pop(rax);
                pop(rbx);
                cmp(rbx, rax);
                mov(rax, 0);
                sete(al);
                push(rax);
                break;
            case Instruction::COMPARE_NOT_EQUALS_I64:
                pop(rax);
                pop(rbx);
                cmp(rbx, rax);
                mov(rax, 0);
                setne(al);
                push(rax);
                break;
            case Instruction::REPUSH_FROM_N: {
                int32_t repushLen = *(int32_t*)&instructions.at(i + 1);
                int32_t repushOffset = *(int32_t*)&instructions.at(i + 5);
                assert(repushLen % 8 == 0);
                for(int j = 0; j < repushLen / 8; ++j) {
                    mov(rax, qword[rsp + (repushOffset + repushLen - 8)]);
                    push(rax);
                }
                break;
            }
            case Instruction::POP_N_BELOW: {
                int32_t popLen = *(int32_t*)&instructions.at(i + 1);
                int32_t popOffset = *(int32_t*)&instructions.at(i + 5);
                assert(popOffset % 8 == 0);
                for(int j = popOffset / 8 - 1; j >= 0; --j) {
                    mov(rax, ptr[rsp + (j * 8)]);
                    mov(ptr[rsp + (j * 8 + popLen)], rax);
                }
                add(rsp, popLen);
                break;
            }
            case Instruction::JUMP: {
                auto newIp = *(int32_t*)&instructions.at(i + 1);
                mov(ip, newIp);
                if(isInstructionAtIpJittable(newIp)) {
                    // try to find the label in already compiled code
                    bool found = false;
                    for(auto& instructionJumpLabel: instructionLocationLabels) {
                        if(instructionJumpLabel.first == newIp) {
                            jmp(instructionJumpLabel.second);
                            found = true;
                            break;
                        }
                    }
                    if(found) {
                        break;
                    }

                    // create a new label and assign it as soon as we compile the instruction
                    Xbyak::Label label;
                    jmp(label);
                    directJumpLocationLabels.emplace_back(std::make_pair(std::make_unique<Xbyak::Label>(std::move(label)), newIp));
                } else {
                    jmp("AfterJumpTable");
                }
                break;
            }
            case Instruction::JUMP_IF_FALSE: {
                auto newIp = *(int32_t*)&instructions.at(i + 1);
                pop(rax);
                mov(rbx, newIp);
                test(rax, rax);
                cmovz(ip, rbx);
                // If the instruction at the desired ip is jittable, then we can just jump to it directly without using the jump table.
                // If it is not jittable, it's even easier as we can just pass execution back to the interpreter
                if(isInstructionAtIpJittable(newIp)) {
                    // try to find the label in already compiled code
                    bool found = false;
                    for(auto& instructionJumpLabel: instructionLocationLabels) {
                        if(instructionJumpLabel.first == newIp) {
                            jz(instructionJumpLabel.second);
                            found = true;
                            break;
                        }
                    }
                    if(found) {
                        break;
                    }

                    // create a new label and assign it as soon as we compile the code
                    Xbyak::Label label;
                    jz(label);
                    directJumpLocationLabels.emplace_back(std::make_pair(std::make_unique<Xbyak::Label>(std::move(label)), newIp));
                } else {
                    jz("AfterJumpTable");
                }
                break;
            }
            case Instruction::CALL: {
                auto callInfoOffset = *(int32_t*)&instructions.at(i + 1);
                mov(rax, rsp);
                add(rax, callInfoOffset);
                // rax points to the location of the function id/ptr
                mov(rbx, qword[rax]);
                // rbx contains the new ip if it's a normal function and a ptr if it's a lambda
                test(bl, 1);
                Xbyak::Label normalOrNativeFunctionLocation;
                Xbyak::Label end;
                jnz(normalOrNativeFunctionLocation);
                // lambda
                // setup rsi
                mov(rsi, rbx);
                add(rsi, 16);
                // rsi points to the start of the buffer in ram
                // extract length & ip
                mov(ecx, dword[rbx]);
                // rcx contains the length of the lambda data
                mov(ebx, dword[rbx + 4]);
                // rbx contains the ip we should jump to
                sub(rsp, rcx);
                mov(rdi, rsp);
                // rdi points to the end of destination on the stack
                cld();
                rep();
                movsb();

                jmp(end);

                L(normalOrNativeFunctionLocation);
                // normal function or native function
                Xbyak::Label normalFunctionLocation;
                cmp(bl, 3);
                jne(normalFunctionLocation);
                // native function
                sar(rbx, 32);
                mov(nativeFunctionIdRegister, rbx);
                inc(nativeFunctionIdRegister);
                add(ip, instructionToWidth(ins));
                jmp("AfterJumpTable");
                // note: we never actually execute the code below because of the jmp
                L(normalFunctionLocation);
                // normal/default function
                sar(rbx, 32);
                L(end);
                add(ip, instructionToWidth(ins));
                mov(dword[rax + 4], ip32);

                mov(ip, rbx);
                jumpWithIp();
                break;
            }
            case Instruction::RETURN: {
                int32_t returnInfoOffset = *(int32_t*)&instructions.at(i + 1);
                mov(ip, qword[rsp + returnInfoOffset]);
                sar(ip, 32);

                // pop below
                {
                    int32_t popLen = 8;
                    int32_t popOffset = returnInfoOffset;
                    assert(popOffset % 8 == 0);
                    for(int j = popOffset / 8 - 1; j >= 0; --j) {
                        mov(rax, ptr[rsp + (j * 8)]);
                        mov(ptr[rsp + (j * 8 + popLen)], rax);
                    }
                    add(rsp, popLen);
                }

                jumpWithIp();
                break;
            }
            case Instruction::LIST_GET_TAIL: {
                cmp(qword[rsp], 0);
                Xbyak::Label after;
                je(after);
                mov(rax, qword[rsp]);
                mov(rax, qword[rax]);
                mov(qword[rsp], rax);
                L(after);
                break;
            }
            case Instruction::LOAD_FROM_PTR: {
                int32_t sizeOfElement;
                memcpy(&sizeOfElement, &instructions.at(i + 1), 4);
                int32_t offset;
                memcpy(&offset, &instructions.at(i + 5), 4);

                cmp(qword[rsp], 0);
                je("AfterJumpTable");

                pop(rax);
                assert(sizeOfElement % 8 == 0);
                for(int32_t i = sizeOfElement / 8 - 1; i >= 0; --i) {
                    mov(rbx, qword[rax + (8 * i + offset)]);
                    push(rbx);
                }
                break;
            }
            case Instruction::LOGICAL_OR:
                pop(rax);
                or_(qword[rsp], rax);
                break;
            case Instruction::LOGICAL_AND: {
                pop(rax);
                and_(qword[rsp], rax);
                break;
            }
            case Instruction::LOGICAL_NOT:
            case Instruction::IS_LIST_EMPTY: {
                mov(rbx, 0);
                cmp(qword[rsp], 0);
                sete(bl);
                mov(qword[rsp], rbx);
                break;
            }
            case Instruction::RUN_GC:
                mov(rax, originalStackPointer);
                add(rax, stackSize);
                // rax now points to the upper end of the original stack

                // calculate new stack size
                sub(rax, rsp);

                push(r8);
                push(r9);
                push(r10);
                push(r11);
                mov(rdi, gcPointer);
                mov(rsi, rax);
                mov(rdx, ip);
                call(gcRequestCollectionPointer);
                pop(r11);
                pop(r10);
                pop(r9);
                pop(r8);
                break;
            case Instruction::NOOP:
                break;
            default:
                assert(false);
                break;
            }
            if(shouldAutoIncrement) {
                i += instructionToWidth(ins);
                add(ip, instructionToWidth(ins));
            }
        }
        assert(directJumpLocationLabels.empty());

        jmp("AfterJumpTable");

        L("JumpTable");
        for(size_t i = 0; i <= instructions.size(); ++i) {
            bool labelExists = false;
            for(auto& label : instructionLocationLabels) {
                if(label.first == i) {
                    labelExists = true;
                    putL(label.second);
                    break;
                }
            }
            if(!labelExists) {
                putL("AfterJumpTable");
            }
        }
        L("AfterJumpTable");

        assert(!hasUndefinedLabel());

        mov(r14, originalStackPointer);
        add(r14, stackSize);
        // r14 now points to the upper end of stack

        // calculate new stack size
        sub(r14, rsp);
        mov(stackSize, r14);
        mov(rsp, r15);

        // do some magic to put but stackSize and ip in the rax register
        mov(rax, stackSize);
        sal(rax, 32);
        mov(rbx, ip);
        mov(ebx, ebx);
        or_(rax, rbx);

        // put native function id in the rdx register
        mov(rdx, nativeFunctionIdRegister);

        // restore registers & stack
        mov(rsp, r15);
        pop(r15);
        pop(r14);
        pop(r13);
        pop(r12);
        pop(r11);
        pop(r10);
        pop(rbp);
        pop(rbx);
        ret();

        readyRE();
    }
};
#else
class JitCode {
};
#endif

VM::VM(Program program, VMParameters params)
: mProgram(std::move(program)), mGC(*this, params) {
#ifdef SAMAL_ENABLE_JIT
    mCompiledCode = std::make_unique<JitCode>(mProgram.code);
#endif
}
ExternalVMValue VM::run(const std::string& functionName, std::vector<uint8_t> initialStack) {
    mStack.clear();
    mStack.push(initialStack);
    Program::Function* function{ nullptr };
    for(auto& func : mProgram.functions) {
        if(func.name == functionName) {
            function = &func;
        }
    }
    if(!function) {
        throw std::runtime_error{ "Function " + functionName + " not found!" };
    }
    mIp = function->offset;
    auto& returnType = function->type.getFunctionTypeInfo().first;
#ifdef _DEBUG
    auto dump = mStack.dump();
    printf("Dump:\n%s\n", dump.c_str());
#endif
    while(true) {
#ifdef SAMAL_ENABLE_JIT
        // first try to jit as many instructions as possible
        {
#    ifdef _DEBUG
            printf("Executing jit...\n");
#    endif
            JitReturn ret = mCompiledCode->getCode<JitReturn (*)(int32_t, uint8_t*, int32_t, VM*, void(VM::*)(int64_t, int64_t))>()(mIp, mStack.getTopPtr(), mStack.getSize(), this, &VM::jitRequestGCCollection);
            mStack.setSize(ret.stackSize);
            mIp = ret.ip;
            if(mIp == static_cast<int32_t>(mProgram.code.size())) {
#    ifdef _DEBUG
                auto dump = mStack.dump();
                printf("Dump:\n%s\n", dump.c_str());
#    endif
                return ExternalVMValue::wrapStackedValue(returnType, *this, 0);
            }
#    ifdef _DEBUG
            auto dump = mStack.dump();
            printf("New ip: %u\n", mIp);
            printf("Dump:\n%s\n", dump.c_str());
#    endif
            if(ret.nativeFunctionToCall) {
                auto newIp = mIp;
                mIp -= instructionToWidth(Instruction::CALL);
                execNativeFunction(ret.nativeFunctionToCall - 1);
                mIp = newIp;
                continue;
            }
        }
#endif
        // then run one through the interpreter
        // TODO run multiple instructions at once if multiple instructions in a row can't be jitted
        auto ret = interpretInstruction();
#ifdef _DEBUG
        auto stackDump = mStack.dump();
        printf("Stack:\n%s\n", stackDump.c_str());
#endif
        if(!ret) {
            return ExternalVMValue::wrapStackedValue(returnType, *this, 0);
        }
    }
}
bool VM::interpretInstruction() {
    bool incIp = true;
    auto ins = static_cast<Instruction>(mProgram.code.at(mIp));
#ifdef x86_64_BIT_MODE
    constexpr int32_t BOOL_SIZE = 8;
#else
    constexpr int32_t BOOL_SIZE = 1;
#endif
#ifdef _DEBUG
    // dump
    auto varDump = dumpVariablesOnStack();
    printf("%s", varDump.c_str());
    printf("Executing instruction %i: %s\n", static_cast<int>(ins), instructionToString(ins));
#endif
    switch(ins) {
    case Instruction::PUSH_1:
#ifdef x86_64_BIT_MODE
        assert(false);
#endif
        mStack.push(&mProgram.code.at(mIp + 1), 1);
        break;
    case Instruction::PUSH_4:
#ifdef x86_64_BIT_MODE
        assert(false);
#endif
        mStack.push(&mProgram.code.at(mIp + 1), 4);
        break;
    case Instruction::PUSH_8:
        mStack.push(&mProgram.code.at(mIp + 1), 8);
        break;
    case Instruction::REPUSH_FROM_N:
        mStack.repush(*(int32_t*)&mProgram.code.at(mIp + 5), *(int32_t*)&mProgram.code.at(mIp + 1));
        break;
    case Instruction::JUMP_IF_FALSE: {
#ifdef x86_64_BIT_MODE
        auto val = *(bool*)mStack.get(0);
        mStack.pop(8);
        if(!val) {
            mIp = *(int32_t*)&mProgram.code.at(mIp + 1);
            incIp = false;
        }
#else
        auto val = *(bool*)mStack.get(0);
        mStack.pop(1);
        if(!val) {
            mIp = *(int32_t*)&mProgram.code.at(mIp + 1);
            incIp = false;
        }
#endif
        break;
    }
    case Instruction::JUMP: {
        mIp = *(int32_t*)&mProgram.code.at(mIp + 1);
        incIp = false;
        break;
    }
    case Instruction::SUB_I32: {
#ifdef x86_64_BIT_MODE
        auto lhs = *(int32_t*)mStack.get(8);
        auto rhs = *(int32_t*)mStack.get(0);
        int64_t res = lhs - rhs;
        mStack.pop(16);
        mStack.push(&res, 8);
#else
        auto lhs = *(int32_t*)mStack.get(4);
        auto rhs = *(int32_t*)mStack.get(0);
        int32_t res = lhs - rhs;
        mStack.pop(8);
        mStack.push(&res, 4);
#endif
        break;
    }
    case Instruction::ADD_I32: {
#ifdef x86_64_BIT_MODE
        auto lhs = *(int32_t*)mStack.get(8);
        auto rhs = *(int32_t*)mStack.get(0);
        int64_t res = lhs + rhs;
        mStack.pop(16);
        mStack.push(&res, 8);
#else
        auto lhs = *(int32_t*)mStack.get(4);
        auto rhs = *(int32_t*)mStack.get(0);
        int32_t res = lhs + rhs;
        mStack.pop(8);
        mStack.push(&res, 4);
#endif
        break;
    }
    case Instruction::MUL_I32: {
#ifdef x86_64_BIT_MODE
        auto lhs = *(int32_t*)mStack.get(8);
        auto rhs = *(int32_t*)mStack.get(0);
        int64_t res = lhs * rhs;
        mStack.pop(16);
        mStack.push(&res, 8);
#else
        auto lhs = *(int32_t*)mStack.get(4);
        auto rhs = *(int32_t*)mStack.get(0);
        int32_t res = lhs * rhs;
        mStack.pop(8);
        mStack.push(&res, 4);
#endif
        break;
    }
    case Instruction::DIV_I32: {
#ifdef x86_64_BIT_MODE
        auto lhs = *(int32_t*)mStack.get(8);
        auto rhs = *(int32_t*)mStack.get(0);
        int64_t res = lhs / rhs;
        mStack.pop(16);
        mStack.push(&res, 8);
#else
        auto lhs = *(int32_t*)mStack.get(4);
        auto rhs = *(int32_t*)mStack.get(0);
        int32_t res = lhs / rhs;
        mStack.pop(8);
        mStack.push(&res, 4);
#endif
        break;
    }
    case Instruction::MODULO_I32: {
#ifdef x86_64_BIT_MODE
        auto lhs = *(int32_t*)mStack.get(8);
        auto rhs = *(int32_t*)mStack.get(0);
        int64_t res = lhs % rhs;
        mStack.pop(16);
        mStack.push(&res, 8);
#else
        auto lhs = *(int32_t*)mStack.get(4);
        auto rhs = *(int32_t*)mStack.get(0);
        int32_t res = lhs % rhs;
        mStack.pop(8);
        mStack.push(&res, 4);
#endif
        break;
    }
    case Instruction::COMPARE_LESS_THAN_I32: {
#ifdef x86_64_BIT_MODE
        auto lhs = *(int32_t*)mStack.get(8);
        auto rhs = *(int32_t*)mStack.get(0);
        mStack.pop(16);
        int64_t res = lhs < rhs;
        mStack.push(&res, 8);
#else
        auto lhs = *(int32_t*)mStack.get(4);
        auto rhs = *(int32_t*)mStack.get(0);
        mStack.pop(8);
        bool res = lhs < rhs;
        mStack.push(&res, 1);
#endif
        break;
    }
    case Instruction::COMPARE_MORE_THAN_I32: {
#ifdef x86_64_BIT_MODE
        auto lhs = *(int32_t*)mStack.get(8);
        auto rhs = *(int32_t*)mStack.get(0);
        mStack.pop(16);
        int64_t res = lhs > rhs;
        mStack.push(&res, 8);
#else
        auto lhs = *(int32_t*)mStack.get(4);
        auto rhs = *(int32_t*)mStack.get(0);
        mStack.pop(8);
        bool res = lhs > rhs;
        mStack.push(&res, 1);
#endif
        break;
    }
    case Instruction::COMPARE_LESS_EQUAL_THAN_I32: {
#ifdef x86_64_BIT_MODE
        auto lhs = *(int32_t*)mStack.get(8);
        auto rhs = *(int32_t*)mStack.get(0);
        mStack.pop(16);
        int64_t res = lhs <= rhs;
        mStack.push(&res, 8);
#else
        auto lhs = *(int32_t*)mStack.get(4);
        auto rhs = *(int32_t*)mStack.get(0);
        mStack.pop(8);
        bool res = lhs <= rhs;
        mStack.push(&res, 1);
#endif
        break;
    }
    case Instruction::COMPARE_MORE_EQUAL_THAN_I32: {
#ifdef x86_64_BIT_MODE
        auto lhs = *(int32_t*)mStack.get(8);
        auto rhs = *(int32_t*)mStack.get(0);
        mStack.pop(16);
        int64_t res = lhs >= rhs;
        mStack.push(&res, 8);
#else
        auto lhs = *(int32_t*)mStack.get(4);
        auto rhs = *(int32_t*)mStack.get(0);
        mStack.pop(8);
        bool res = lhs >= rhs;
        mStack.push(&res, 1);
#endif
        break;
    }
    case Instruction::COMPARE_EQUALS_I32: {
#ifdef x86_64_BIT_MODE
        auto lhs = *(int32_t*)mStack.get(8);
        auto rhs = *(int32_t*)mStack.get(0);
        mStack.pop(16);
        int64_t res = lhs == rhs;
        mStack.push(&res, 8);
#else
        auto lhs = *(int32_t*)mStack.get(4);
        auto rhs = *(int32_t*)mStack.get(0);
        mStack.pop(8);
        bool res = lhs == rhs;
        mStack.push(&res, 1);
#endif
        break;
    }
    case Instruction::COMPARE_NOT_EQUALS_I32: {
#ifdef x86_64_BIT_MODE
        auto lhs = *(int32_t*)mStack.get(8);
        auto rhs = *(int32_t*)mStack.get(0);
        mStack.pop(16);
        int64_t res = lhs != rhs;
        mStack.push(&res, 8);
#else
        auto lhs = *(int32_t*)mStack.get(4);
        auto rhs = *(int32_t*)mStack.get(0);
        mStack.pop(8);
        bool res = lhs != rhs;
        mStack.push(&res, 1);
#endif
        break;
    }
    case Instruction::SUB_I64: {
        auto lhs = *(int64_t*)mStack.get(8);
        auto rhs = *(int64_t*)mStack.get(0);
        int64_t res = lhs - rhs;
        mStack.pop(16);
        mStack.push(&res, 8);
        break;
    }
    case Instruction::ADD_I64: {
        auto lhs = *(int64_t*)mStack.get(8);
        auto rhs = *(int64_t*)mStack.get(0);
        int64_t res = lhs + rhs;
        mStack.pop(16);
        mStack.push(&res, 8);
        break;
    }
    case Instruction::MUL_I64: {
        auto lhs = *(int64_t*)mStack.get(8);
        auto rhs = *(int64_t*)mStack.get(0);
        int64_t res = lhs * rhs;
        mStack.pop(16);
        mStack.push(&res, 8);
        break;
    }
    case Instruction::DIV_I64: {
        auto lhs = *(int64_t*)mStack.get(8);
        auto rhs = *(int64_t*)mStack.get(0);
        int64_t res = lhs / rhs;
        mStack.pop(16);
        mStack.push(&res, 8);
        break;
    }
    case Instruction::MODULO_I64: {
        auto lhs = *(int64_t*)mStack.get(8);
        auto rhs = *(int64_t*)mStack.get(0);
        int64_t res = lhs % rhs;
        mStack.pop(16);
        mStack.push(&res, 8);
        break;
    }
    case Instruction::COMPARE_LESS_THAN_I64: {
        auto lhs = *(int64_t*)mStack.get(8);
        auto rhs = *(int64_t*)mStack.get(0);
        mStack.pop(16);
        int64_t res = lhs < rhs;
        mStack.push(&res, BOOL_SIZE);
        break;
    }
    case Instruction::COMPARE_MORE_THAN_I64: {
        auto lhs = *(int64_t*)mStack.get(8);
        auto rhs = *(int64_t*)mStack.get(0);
        mStack.pop(16);
        int64_t res = lhs > rhs;
        mStack.push(&res, BOOL_SIZE);
        break;
    }
    case Instruction::COMPARE_LESS_EQUAL_THAN_I64: {
        auto lhs = *(int64_t*)mStack.get(8);
        auto rhs = *(int64_t*)mStack.get(0);
        mStack.pop(16);
        int64_t res = lhs <= rhs;
        mStack.push(&res, BOOL_SIZE);
        break;
    }
    case Instruction::COMPARE_MORE_EQUAL_THAN_I64: {
        auto lhs = *(int64_t*)mStack.get(8);
        auto rhs = *(int64_t*)mStack.get(0);
        mStack.pop(16);
        int64_t res = lhs >= rhs;
        mStack.push(&res, BOOL_SIZE);
        break;
    }
    case Instruction::COMPARE_EQUALS_I64: {
        auto lhs = *(int64_t*)mStack.get(8);
        auto rhs = *(int64_t*)mStack.get(0);
        mStack.pop(16);
        int64_t res = lhs == rhs;
        mStack.push(&res, BOOL_SIZE);
        break;
    }
    case Instruction::COMPARE_NOT_EQUALS_I64: {
        auto lhs = *(int64_t*)mStack.get(8);
        auto rhs = *(int64_t*)mStack.get(0);
        mStack.pop(16);
        int64_t res = lhs != rhs;
        mStack.push(&res, BOOL_SIZE);
        break;
    }
    case Instruction::LOGICAL_OR: {
#ifdef x86_64_BIT_MODE
        auto lhs = *(int64_t*)mStack.get(8);
        auto rhs = *(int64_t*)mStack.get(0);
        mStack.pop(16);
        int64_t res = lhs || rhs;
        mStack.push(&res, 8);
#else
        auto lhs = *(bool*)mStack.get(1);
        auto rhs = *(bool*)mStack.get(0);
        mStack.pop(2);
        bool res = lhs || rhs;
        mStack.push(&res, 1);
#endif
        break;
    }
    case Instruction::LOGICAL_AND: {
#ifdef x86_64_BIT_MODE
        auto lhs = *(int64_t*)mStack.get(8);
        auto rhs = *(int64_t*)mStack.get(0);
        mStack.pop(16);
        int64_t res = lhs && rhs;
        mStack.push(&res, 8);
#else
        auto lhs = *(bool*)mStack.get(1);
        auto rhs = *(bool*)mStack.get(0);
        mStack.pop(2);
        bool res = lhs && rhs;
        mStack.push(&res, 1);
#endif
        break;
    }
    case Instruction::LOGICAL_NOT: {
#ifdef x86_64_BIT_MODE
        auto value = *(int64_t*)mStack.get(0);
        mStack.pop(8);
        int64_t res = !value;
        mStack.push(&res, 8);
#else
        auto value = *(bool*)mStack.get(0);
        mStack.pop(1);
        bool res = !value;
        mStack.push(&res, 1);
#endif
        break;
    }
    case Instruction::POP_N_BELOW: {
        mStack.popBelow(*(int32_t*)&mProgram.code.at(mIp + 5),
            *(int32_t*)&mProgram.code.at(mIp + 1));
        break;
    }
    case Instruction::CALL: {
        auto offset = *(int32_t*)&mProgram.code.at(mIp + 1);
        int32_t newIp{ -1 };
        auto firstHalfOfParam = *(int32_t*)mStack.get(offset);
        if(firstHalfOfParam % 2 == 0) {
            // it's a lambda function call
            // save old values to stack
            auto* lambdaParams = *(uint8_t**)mStack.get(offset);
            auto lambdaParamsLen = ((int32_t*)lambdaParams)[0];
            newIp = ((int32_t*)lambdaParams)[1];

            // save old values to stack
            *(int32_t*)mStack.get(offset + 4) = mIp + instructionToWidth(Instruction::CALL);
            *(int32_t*)mStack.get(offset) = 0;

            assert(lambdaParamsLen >= 0);
            if(lambdaParamsLen > 0) {
                uint8_t buffer[lambdaParamsLen];
                memcpy(buffer, lambdaParams + 16, lambdaParamsLen);
                mStack.push(buffer, lambdaParamsLen);
            }
        } else {
            // it's a default function call or a native function call
            if(firstHalfOfParam == 3) {
                // native

                // save old values
                newIp = mIp + instructionToWidth(Instruction::CALL);
                execNativeFunction(*(int32_t*)mStack.get(offset + 4));
            } else {
                // default
                newIp = *(int32_t*)mStack.get(offset + 4);

                // save old values to stack
                *(int32_t*)mStack.get(offset + 4) = mIp + instructionToWidth(Instruction::CALL);
                *(int32_t*)mStack.get(offset) = 0;
            }
        }

        mIp = newIp;
        incIp = false;
        break;
    }
    case Instruction::RETURN: {
        auto offset = *(int32_t*)&mProgram.code.at(mIp + 1);
        mIp = *(int32_t*)mStack.get(offset + 4);
        mStack.popBelow(offset, 8);
        incIp = false;
        if(mIp == static_cast<int32_t>(mProgram.code.size())) {
            return false;
        }
        break;
    }
    case Instruction::CREATE_LAMBDA: {
        auto functionIpOffset = *(int32_t*)&mProgram.code.at(mIp + 1);
        auto lambdaCapturedTypesId = *(int32_t*)&mProgram.code.at(mIp + 5);
        auto* dataOnHeap = (uint8_t*)mGC.alloc(functionIpOffset + 16);
        // store length of buffer & ip of the function on the stack
        ((int32_t*)dataOnHeap)[0] = functionIpOffset;
        ((int32_t*)dataOnHeap)[1] = *(int32_t*)mStack.get(functionIpOffset);
        ((int32_t*)dataOnHeap)[2] = lambdaCapturedTypesId;
        ((int32_t*)dataOnHeap)[3] = 1;
        memcpy(dataOnHeap + 16, mStack.get(0), functionIpOffset);
        mStack.pop(functionIpOffset + 8);
        mStack.push(&dataOnHeap, 8);
        break;
    }
    case Instruction::CREATE_STRUCT_OR_ENUM: {
        auto sizeOfData = *(int32_t*)&mProgram.code.at(mIp + 1);
        auto* dataOnHeap = (uint8_t*)mGC.alloc(sizeOfData);
        memcpy(dataOnHeap, mStack.get(0), sizeOfData);
        mStack.pop(sizeOfData);
        mStack.push(&dataOnHeap, 8);
        break;
    }
    case Instruction::CREATE_LIST: {
        auto elementSize = *(int32_t*)&mProgram.code.at(mIp + 1);
        auto elementCount = *(int32_t*)&mProgram.code.at(mIp + 5);
        uint8_t* firstPtr = nullptr;
        uint8_t* ptrToPreviousElement = nullptr;
        for(int i = 0; i < elementCount; ++i) {
            int32_t elementOffset = (elementCount - i - 1) * elementSize;
            auto* dataOnHeap = (uint8_t*)mGC.alloc(elementSize + 8);
            if(firstPtr == nullptr)
                firstPtr = dataOnHeap;

            memcpy(dataOnHeap + 8, mStack.get(elementOffset), elementSize);
            if(ptrToPreviousElement) {
                memcpy(ptrToPreviousElement, &dataOnHeap, 8);
            }
            ptrToPreviousElement = dataOnHeap;
        }
        // let the last element point to nullptr
        if(ptrToPreviousElement)
            memset(ptrToPreviousElement, 0, 8);
        mStack.pop(elementSize * elementCount);
        mStack.push(&firstPtr, 8);
        break;
    }
    case Instruction::LIST_GET_TAIL: {
        if(*(void**)mStack.get(0) != nullptr) {
            *(uint8_t**)mStack.get(0) = **(uint8_t***)mStack.get(0);
        }
        break;
    }
    case Instruction::LOAD_FROM_PTR: {
        auto size = *(int32_t*)&mProgram.code.at(mIp + 1);
        auto offset = *(int32_t*)&mProgram.code.at(mIp + 5);
        auto ptr = *(uint8_t**)mStack.get(0);
        if(ptr == nullptr) {
            // TODO throw some kind of language-internal exception
            throw std::runtime_error{ "Trying to access :head of empty list" };
        }
        mStack.pop(8);
        char buffer[size];
        memcpy(buffer, ptr + offset, size);
        mStack.push(buffer, size);
        break;
    }
    case Instruction::COMPARE_COMPLEX_EQUALITY: {
        auto datatypeIndex = *(int32_t*)&mProgram.code.at(mIp + 1);
        auto& datatype = mProgram.auxiliaryDatatypes.at(datatypeIndex);
        auto datatypeSize = datatype.getSizeOnStack();
        std::function<bool(const Datatype&, uint8_t*, uint8_t*)> isEqual = [&](const Datatype& type, uint8_t* a, uint8_t* b) -> bool {
            switch(type.getCategory()) {
            case DatatypeCategory::list: {
                uint8_t* lhsPtr = *(uint8_t**)a;
                uint8_t* rhsPtr = *(uint8_t**)b;
                while(true) {
                    if(!lhsPtr && !rhsPtr) {
                        return true;
                    }
                    if(!lhsPtr && rhsPtr) {
                        return false;
                    }
                    if(lhsPtr && !rhsPtr) {
                        return false;
                    }
                    if(!isEqual(type.getListContainedType(), lhsPtr + 8, rhsPtr + 8)) {
                        return false;
                    }
                    lhsPtr = *(uint8_t**)lhsPtr;
                    rhsPtr = *(uint8_t**)rhsPtr;
                }
            }
            case DatatypeCategory::char_:
            case DatatypeCategory::i32:
                return *(int32_t*)a == *(int32_t*)b;
            default:
                todo();
            }
        };
        int64_t result = isEqual(datatype, (uint8_t*)mStack.get(0), (uint8_t*)mStack.get(datatypeSize));
        mStack.pop(datatypeSize * 2);
        mStack.push(&result, BOOL_SIZE);
        break;
    }
    case Instruction::LIST_PREPEND: {
        auto datatypeLength = *(int32_t*)&mProgram.code.at(mIp + 1);
        uint8_t* allocation = mGC.alloc(datatypeLength + 8);
        memcpy(allocation, mStack.get(0), 8);
        memcpy(allocation + 8, mStack.get(8), datatypeLength);
        mStack.pop(datatypeLength + 8);
        mStack.push(&allocation, 8);
        break;
    }
    case Instruction::IS_LIST_EMPTY: {
        void* list = *(void**)mStack.get(0);
        int64_t result;
        if(list == nullptr) {
            result = 1;
        } else {
            result = 0;
        }
        mStack.pop(8);
        mStack.push(&result, BOOL_SIZE);
        break;
    }
    case Instruction::RUN_GC: {
        mGC.requestCollection();
        break;
    }
    case Instruction::NOOP:
        break;
    case Instruction::INCREASE_STACK_SIZE: {
        int32_t amount;
        memcpy(&amount, &mProgram.code.at(mIp + 1), 4);
        mStack.setSize(mStack.getSize() + amount);
#ifdef _DEBUG
        // Avoid UB in debug mode, because then we're dumping the stack and don't want any uninitialized data on it.
        // As we shouldn't use the data if everything is correct, it doesn't actually matter in release mode.
        // The JIT-Code doesn't initialise the data either.
        memset(mStack.getTopPtr(), 0, amount);
#endif
        break;
    }
    default:
        fprintf(stderr, "Unhandled instruction %i: %s\n", static_cast<int>(ins), instructionToString(ins));
        assert(false);
    }
    mIp += instructionToWidth(ins) * incIp;
    return true;
}
ExternalVMValue VM::run(const std::string& functionName, const std::vector<ExternalVMValue>& params) {
    std::vector<uint8_t> stack(8);
    auto returnValueIP = static_cast<uint32_t>(mProgram.code.size());
    //memset(stack.data(), 0, 8);
    memcpy(stack.data() + 4, &returnValueIP, 4);
    memcpy(stack.data(), &returnValueIP, 4);
    for(auto& param : params) {
        auto stackedValue = param.toStackValue(*this);
        stack.insert(stack.begin(), stackedValue.cbegin(), stackedValue.cend());
    }
    return run(functionName, stack);
}
const Stack& VM::getStack() const {
    return mStack;
}
std::string VM::dumpVariablesOnStack() {
    std::string ret;
    generateStacktrace([&](const uint8_t* ptr, const Datatype& type, const std::string& name) { ret += " " + name + ": " + ExternalVMValue::wrapFromPtr(type, *this, ptr).dump() + "\n"; }, [&](const std::string& functionName) { ret += functionName + "\n"; });
    return ret;
}
void VM::generateStacktrace(const std::function<void(const uint8_t* ptr, const Datatype&, const std::string& name)>& variableCallback, const std::function<void(const std::string&)>& functionCallback) const {
    int32_t ip = mIp;
    int32_t offsetFromTop = 0;
    bool firstIteration = true;
    while(true) {
        const Program::Function* currentFunction = nullptr;
        for(auto& func : mProgram.functions) {
            if(ip >= func.offset && ip < func.offset + func.len) {
                currentFunction = &func;
                break;
            }
        }
        if(!currentFunction) {
            return;
        }
        if(functionCallback)
            functionCallback(currentFunction->name);
        const auto virtualStackSize = currentFunction->stackSizePerIp.at(ip);
        auto stackInfo = currentFunction->stackInformation->getBestNodeForIp(ip);
        assert(stackInfo);

        int32_t nextFunctionOffset = offsetFromTop + virtualStackSize;
        assert(nextFunctionOffset >= 0);
        bool afterPop = false;
        while(stackInfo) {
            if(stackInfo->isAtPopInstruction()) {
                afterPop = true;
            }
            auto& variable = stackInfo->getVarEntry();
            if(variable) {
                // after we hit a pop, we don't dump any variables on the stack anymore as they might have been popped of
                if(!afterPop) {
                    // check that the variable isn't assigned to the return value of the function;
                    // this would not be valid because the function actually hasn't returned yet
                    if(!(!firstIteration && stackInfo->getIp() == ip)) {
                        if(variableCallback)
                            variableCallback(mStack.getTopPtr() + virtualStackSize - stackInfo->getStackSize() + offsetFromTop, variable->datatype, variable->name);
                    }
                }
            }

            if(stackInfo->getPrevSibling()) {
                stackInfo = stackInfo->getPrevSibling();
            } else {
                afterPop = false;
                stackInfo = stackInfo->getParent();
            }
        }
        int32_t prevIp;
        memcpy(&prevIp, mStack.get(offsetFromTop + virtualStackSize + 4), 4);
        if(prevIp == static_cast<int32_t>(mProgram.code.size())) {
            break;
        }
        nextFunctionOffset -= currentFunction->type.getFunctionTypeInfo().first.getSizeOnStack();
        nextFunctionOffset += 8;
        //assert(nextFunctionOffset >= 0);
        ip = prevIp;
        offsetFromTop = nextFunctionOffset;
        firstIteration = false;
    }
}
void VM::execNativeFunction(int32_t nativeFuncId) {
    auto& nativeFunc = mProgram.nativeFunctions.at(nativeFuncId);
    const auto& functionTypeInfo = nativeFunc.functionType.getFunctionTypeInfo();
    auto returnTypeSize = functionTypeInfo.first.getSizeOnStack();

    size_t sumOfParamSizes = 0;
    for(auto& param: functionTypeInfo.second) {
        sumOfParamSizes += param.getSizeOnStack();
    }
    std::vector<ExternalVMValue> params;
    params.reserve(functionTypeInfo.second.size());
    size_t offset = 0;
    for(auto& paramType : functionTypeInfo.second) {
        offset += paramType.getSizeOnStack();
        params.emplace_back(ExternalVMValue::wrapStackedValue(paramType, *this, sumOfParamSizes - offset));
    }

    mStack.pop(offset);
    auto returnValue = nativeFunc.callback(*this, params);
    assert(returnValue.getDatatype() == functionTypeInfo.first);
    auto returnValueBytes = returnValue.toStackValue(*this);
    if(!returnValueBytes.empty())
        mStack.push(returnValueBytes.data(), returnValueBytes.size());
    mStack.popBelow(returnTypeSize, 8);
}
int32_t VM::getIp() const {
    return mIp;
}
const Program& VM::getProgram() const {
    return mProgram;
}
void VM::jitRequestGCCollection(int64_t newStackSize, int64_t newIp) {
    mStack.setSize(newStackSize);
    mIp = newIp;
    mGC.requestCollection();
}
VM::~VM() = default;
void Stack::push(const std::vector<uint8_t>& data) {
    push(data.data(), data.size());
}
void Stack::push(const void* data, size_t len) {
    ensureSpace(len);
    mDataTop -= len;
    memcpy(mDataTop, data, len);
}
void Stack::repush(size_t offset, size_t len) {
    assert(mDataEnd >= mDataTop + offset);
    ensureSpace(len);
    mDataTop -= len;
    memcpy(mDataTop, mDataTop + len + offset, len);
}
void* Stack::get(size_t offset) {
    return mDataTop + offset;
}
const void* Stack::get(size_t offset) const {
    return mDataTop + offset;
}
void Stack::popBelow(size_t offset, size_t len) {
    assert(getSize() >= offset + len);
    memmove(mDataTop + len, mDataTop, offset);
    mDataTop += len;
}
void Stack::pop(size_t len) {
    mDataTop += len;
}
std::string Stack::dump() {
    std::string ret;
    size_t i = 0;
    for(uint8_t* ptr = mDataTop; ptr < mDataEnd; ++ptr) {
        uint8_t val = *ptr;
        ret += std::to_string(val) + " ";
        ++i;
        if(i > 0 && i % 4 == 0) {
            ret += "\n";
        }
    }
    return ret;
}
Stack::Stack() {
    mDataReserved = static_cast<size_t>(1024) * 1024 * 1024 * 2;
    mDataStart = static_cast<uint8_t*>(mmap(nullptr, mDataReserved, PROT_READ | PROT_WRITE | PROT_GROWSDOWN, MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_PRIVATE | MAP_NORESERVE, 0, 0));
    assert(mDataStart);
    mDataEnd = mDataStart + mDataReserved;
    mDataTop = mDataEnd;
}
Stack::~Stack() {
    munmap(mDataStart, mDataReserved);
    mDataStart = nullptr;
    mDataEnd = nullptr;
    mDataTop = nullptr;
}
void Stack::ensureSpace(size_t additionalLen) {
    if(mDataReserved <= getSize() + additionalLen) {
        todo();
        throw std::runtime_error{"No memory left :c"};
    };
}
uint8_t* Stack::getBasePtr() {
    return mDataStart;
}
const uint8_t* Stack::getBasePtr() const {
    return mDataStart;
}
size_t Stack::getSize() const {
    return mDataEnd - mDataTop;
}
void Stack::setSize(size_t val) {
    assert(val <= mDataReserved);
    mDataTop = mDataEnd - val;
}
void Stack::clear() {
    mDataTop = mDataEnd;
}
uint8_t* Stack::getTopPtr() {
    return mDataTop;
}
const uint8_t* Stack::getTopPtr() const {
    return mDataTop;
}

}