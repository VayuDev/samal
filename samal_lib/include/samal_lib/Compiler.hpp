#pragma once
#include "AST.hpp"
#include "Forward.hpp"
#include "Instruction.hpp"
#include "Program.hpp"
#include "StackInformationTree.hpp"
#include <queue>
#include <stack>
#include <unordered_map>

namespace samal {

class VariableSearcher {
public:
    explicit VariableSearcher(std::vector<const IdentifierNode*>& identifiers);
    void identifierFound(const IdentifierNode& identifier);

private:
    std::vector<const IdentifierNode*>& mIdentifiers;
};

class Compiler final {
public:
    explicit Compiler(std::vector<up<ModuleRootNode>>& roots, std::vector<NativeFunction>&& nativeFunctions);
    ~Compiler();
    Program compile();

    void compileFunction(const FunctionDeclarationNode& function);
    Datatype compileLambdaCreationExpression(const LambdaCreationNode&);
    Datatype compileScope(const ScopeNode& scope);
    Datatype compileLiteralI32(int32_t value);
    Datatype compileLiteralI64(int64_t value);
    Datatype compileLiteralBool(bool value);
    Datatype compileLiteralChar(int32_t value);
    Datatype compileLiteralByte(uint8_t value);

    Datatype compileBinaryExpression(const BinaryExpressionNode&);
    Datatype compileIfExpression(const IfExpressionNode&);
    enum class AllowGlobalLoad {
        Yes,
        No
    };
    Datatype compileIdentifierLoad(const IdentifierNode&, AllowGlobalLoad = AllowGlobalLoad::Yes);
    Datatype compileFunctionCall(const FunctionCallExpressionNode&);
    Datatype compileChainedFunctionCall(const FunctionChainExpressionNode&);
    Datatype compileTupleCreationExpression(const TupleCreationNode&);
    Datatype compileTupleAccessExpression(const TupleAccessExpressionNode&);
    Datatype compileAssignmentExpression(const AssignmentExpression&);
    Datatype compileListCreation(const ListCreationNode&);
    Datatype compileListPropertyAccess(const ListPropertyAccessExpression&);
    Datatype compileStructCreation(const StructCreationNode&);
    Datatype compileStructFieldAccess(const StructFieldAccessExpression&);
    Datatype compileTailCallSelf(const TailCallSelfStatementNode&);
    Datatype compilePrefixExpression(const PrefixExpression&);

    Datatype compileEnumCreation(const EnumCreationNode& node);
    Datatype compileMatchExpression(const MatchExpression& node);

    MatchCompileReturn compileTryMatchEnumField(const EnumFieldMatchCondition& condition, const Datatype& datatype, int32_t offsetFromTop);

    MatchCompileReturn compileTryMatchIdentifier(const IdentifierMatchCondition& node, const Datatype& datatype, int32_t offset);


private:
    Program compileInternal();

    Program mProgram;
    std::vector<up<ModuleRootNode>>& mRoots;

    void pushStackFrame();
    void popStackFrame(const Datatype& frameReturnType);

    // Tiny stack frames are used for e.g. parameters in function calls or list creations
    void pushTinyStackFrame();
    void popTinyStackFrame();
    void saveTinyStackFrameVariableLocation(Datatype type);

    void addInstructions(Instruction insn);
    void addInstructions(Instruction insn, int32_t param);
    void addInstructions(Instruction insn, int32_t param1, int32_t param2);
    void addInstructions(Instruction insn, int32_t param1, int32_t param2, int32_t param3);
    void addInstructionOneByteParam(Instruction insn, int8_t param);

    void saveVariableLocation(std::string name, Datatype type, StorageType storageType, int32_t offset = 0);
    void saveCurrentStackSizeToDebugInfo();

    // compiles normal functions and lambdas
    Program::Function& helperCompileFunctionLikeThing(const std::string& name, const Datatype& returnType, const std::vector<std::pair<std::string, Datatype>>& params, const std::vector<std::pair<std::string, Datatype>>& implicitParams, const ScopeNode& body);

    // compiles normal function calls and chained function calls
    Datatype helperCompileFunctionCallLikeThing(const up<ExpressionNode>& functionNameNode, const std::vector<up<ExpressionNode>>& params, const ExpressionNode* chainedInitialParamValue = nullptr);

    int32_t addLabel(Instruction futureInstruction);
    uint8_t* labelToPtr(int32_t label);

    int32_t saveAuxiliaryDatatypeToProgram(Datatype type);

    struct TemplateFunctionToInstantiate {
        FunctionDeclarationNode* function{ nullptr };
        // this is e.g. <i32, i64, (i32, bool)>
        UndeterminedIdentifierReplacementMap replacementMap;
    };
    std::vector<TemplateFunctionToInstantiate> mTemplateFunctionsToInstantiate;

    struct CallableDeclaration {
        CallableDeclarationNode* astNode{ nullptr };
        Datatype type;
    };
    std::unordered_map<std::string, CallableDeclaration> mCallableDeclarations;

    [[nodiscard]] bool addToTemplateFunctionsToInstantiate(CallableDeclaration& node, UndeterminedIdentifierReplacementMap replacementMap, const std::string& fullFunctionName, int32_t labelToInsertId, const std::vector<std::string>& functionsUsingNames);
    std::optional<std::pair<std::string, Compiler::CallableDeclaration&>> findMatchingCallableDeclaration(const std::string& functionName);

    struct Module {
        std::string name;
        std::vector<std::string> usingModuleNames;
    };
    // Maps strings of structs like 'Vec2' (or from external modules like 'Math.Vec3') to the actual datatype. This is later merged with the mTemplateReplacementMap
    UndeterminedIdentifierReplacementMap mCustomUserDatatypeReplacementMap;
    std::vector<Module> mModules;
    Module& findModuleByName(const std::string& name);
    std::unordered_map<DeclarationNode*, size_t> mDeclarationNodeToModuleId;
    std::vector<std::string> mUsingModuleNames;

    UndeterminedIdentifierReplacementMap mCurrentUndeterminedTypeReplacementMap;
    UndeterminedIdentifierReplacementMap mCurrentTemplateTypeReplacementMap;

    int32_t mCurrentFunctionStartingIp{ -1 };
    Datatype mCurrentFunctionType;

    struct VariableOnStack {
        int32_t offsetFromBottom{ 0 };
        Datatype type;
    };
    struct StackFrame {
        std::map<std::string, VariableOnStack> variables;
        int32_t stackFrameSize{ 0 };
    };
    std::stack<StackFrame, std::vector<StackFrame>> mStackFrames;

    int32_t mStackSize{ 0 };

    struct FunctionIdInCodeToInsert {
        int32_t label{ -1 };
        std::string fullFunctionName;
        UndeterminedIdentifierReplacementMap templateParameters;
    };
    std::vector<FunctionIdInCodeToInsert> mLabelsToInsertFunctionIds;

    struct LambdaIdInCodeToInsert {
        int32_t label{ -1 };
        const LambdaCreationNode* lambda{ nullptr };
    };
    std::vector<LambdaIdInCodeToInsert> mLabelsToInsertLambdaIds;

    struct LambdaToCompile {
        const LambdaCreationNode* lambda{ nullptr };
        std::vector<std::pair<std::string, Datatype>> copiedParameters;
    };
    std::queue<LambdaToCompile> mLambdasToCompile;

    up<StackInformationTree> mCurrentStackInfoTree;
    StackInformationTree* mCurrentStackInfoTreeNode{ nullptr };
    std::unordered_map<int32_t, int32_t> mIpToStackSize;

    Program::Function& compileLambda(const LambdaToCompile&);
};

}