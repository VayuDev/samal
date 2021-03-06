#pragma once
#include "AstHelpers.hpp"
#include "Datatype.hpp"
#include "samal_lib/Forward.hpp"
#include <optional>
#include <string>

namespace samal {

struct SourceCodeRef {
    const char* start = nullptr;
    size_t len = 0;
    size_t line = 0, column = 0;
};

struct Parameter {
    up<IdentifierNode> name;
    Datatype type;
};

class ASTNode {
public:
    explicit ASTNode(SourceCodeRef source);
    virtual ~ASTNode() = default;
    virtual void findUsedVariables(VariableSearcher&) const = 0;
    [[nodiscard]] virtual std::string dump(unsigned indent) const;
    [[nodiscard]] virtual inline const char* getClassName() const { return "ASTNode"; }
    void throwException(const std::string& msg) const;
    [[nodiscard]] const SourceCodeRef& getSourceCodeRef() const {
        return mSourceCodeRef;
    }

private:
    SourceCodeRef mSourceCodeRef;
};

class CompilableASTNode : public ASTNode {
public:
    CompilableASTNode(SourceCodeRef source);
    virtual Datatype compile(Compiler& comp) const {
        return Datatype{};
    };
    [[nodiscard]] virtual inline const char* getClassName() const { return "CompilableASTNode"; }
};

class StatementNode : public CompilableASTNode {
public:
    explicit StatementNode(SourceCodeRef source);
    [[nodiscard]] inline const char* getClassName() const override { return "StatementNode"; }

private:
};

class ExpressionNode : public StatementNode {
public:
    explicit ExpressionNode(SourceCodeRef source);
    [[nodiscard]] inline const char* getClassName() const override { return "ExpressionNode"; }

private:
};

class TailCallSelfStatementNode : public StatementNode {
public:
    TailCallSelfStatementNode(SourceCodeRef source, std::vector<up<ExpressionNode>> params);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] inline const auto& getParams() const {
        return mParams;
    }
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "TailCallSelfStatementNode"; }

private:
    std::vector<up<ExpressionNode>> mParams;
};

class AssignmentExpression : public ExpressionNode {
public:
    AssignmentExpression(SourceCodeRef source, up<IdentifierNode> left, up<ExpressionNode> right);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] const up<IdentifierNode>& getLeft() const;
    [[nodiscard]] const up<ExpressionNode>& getRight() const;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "AssignmentExpression"; }

private:
    up<IdentifierNode> mLeft;
    up<ExpressionNode> mRight;
};

class BinaryExpressionNode : public ExpressionNode {
public:
    enum class BinaryOperator {
        PLUS,
        MINUS,
        MULTIPLY,
        DIVIDE,
        MODULO,
        LOGICAL_AND,
        LOGICAL_OR,
        LOGICAL_EQUALS,
        LOGICAL_NOT_EQUALS,
        COMPARISON_LESS_THAN,
        COMPARISON_LESS_EQUAL_THAN,
        COMPARISON_MORE_THAN,
        COMPARISON_MORE_EQUAL_THAN,

    };
    BinaryExpressionNode(SourceCodeRef source, up<ExpressionNode> left, BinaryOperator op, up<ExpressionNode> right);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] inline const auto& getLeft() const {
        return mLeft;
    }
    [[nodiscard]] inline auto getOperator() const {
        return mOperator;
    }
    [[nodiscard]] inline const auto& getRight() const {
        return mRight;
    }
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "BinaryExpressionNode"; }

private:
    up<ExpressionNode> mLeft;
    BinaryOperator mOperator;
    up<ExpressionNode> mRight;
};

class LiteralNode : public ExpressionNode {
public:
    explicit LiteralNode(SourceCodeRef source);
    [[nodiscard]] inline const char* getClassName() const override { return "LiteralNode"; }

private:
};

class LiteralInt32Node : public LiteralNode {
public:
    explicit LiteralInt32Node(SourceCodeRef source, int32_t val);
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "LiteralInt32Node"; }

private:
    int32_t mValue;
};

class LiteralInt64Node : public LiteralNode {
public:
    explicit LiteralInt64Node(SourceCodeRef source, int64_t val);
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "LiteralInt64Node"; }

private:
    int64_t mValue;
};

class LiteralBoolNode : public LiteralNode {
public:
    explicit LiteralBoolNode(SourceCodeRef source, bool val);
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "LiteralBoolNode"; }

private:
    bool mValue;
};

class LiteralCharNode : public LiteralNode {
public:
    explicit LiteralCharNode(SourceCodeRef source, int32_t val);
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "LiteralCharNode"; }

private:
    int32_t mValue;
};

class LiteralByteNode : public LiteralNode {
public:
    explicit LiteralByteNode(SourceCodeRef source, uint8_t val);
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "LiteralByteNode"; }

private:
    uint8_t mValue;
};

class IdentifierNode : public ExpressionNode {
public:
    explicit IdentifierNode(SourceCodeRef source, std::vector<std::string> name, std::vector<Datatype> templateParameters);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] std::string getName() const;
    [[nodiscard]] std::vector<std::string> getNameSplit() const;
    [[nodiscard]] const std::vector<Datatype>& getTemplateParameters() const;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "IdentifierNode"; }

private:
    std::vector<std::string> mName;
    std::vector<Datatype> mTemplateParameters;
};

class TupleCreationNode : public ExpressionNode {
public:
    explicit TupleCreationNode(SourceCodeRef source, std::vector<up<ExpressionNode>> params);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] const auto& getParams() const {
        return mParams;
    }
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "TupleCreationNode"; }

private:
    std::vector<up<ExpressionNode>> mParams;
};

class ListCreationNode : public ExpressionNode {
public:
    explicit ListCreationNode(SourceCodeRef source, std::vector<up<ExpressionNode>> params);
    explicit ListCreationNode(SourceCodeRef source, Datatype baseType);
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;

    [[nodiscard]] const auto& getBaseType() const {
        return mBaseType;
    }
    [[nodiscard]] const auto& getParams() const {
        return mParams;
    }

    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "ListCreationNode"; }

private:
    std::optional<Datatype> mBaseType;
    std::vector<up<ExpressionNode>> mParams;
};

class StructCreationNode : public ExpressionNode {
public:
    struct StructCreationParameter {
        std::string name;
        up<ExpressionNode> value;
        inline StructCreationParameter(std::string name, up<ExpressionNode> value)
        : name(std::move(name)), value(std::move(value)) {
        }
    };
    explicit StructCreationNode(SourceCodeRef source, Datatype structType, std::vector<StructCreationParameter> params);
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;

    [[nodiscard]] const auto& getStructType() const {
        return mStructType;
    }
    [[nodiscard]] const auto& getParams() const {
        return mParams;
    }

    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "StructCreationNode"; }

private:
    Datatype mStructType;
    std::vector<StructCreationParameter> mParams;
};

class EnumCreationNode : public ExpressionNode {
public:
    explicit EnumCreationNode(SourceCodeRef source, Datatype enumType, std::string fieldName, std::vector<up<ExpressionNode>> params);
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;

    [[nodiscard]] const auto& getEnumType() const {
        return mEnumType;
    }
    [[nodiscard]] const auto& getFieldName() const {
        return mFieldName;
    }
    [[nodiscard]] const auto& getParams() const {
        return mParams;
    }

    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "EnumCreationNode"; }

private:
    Datatype mEnumType;
    std::string mFieldName;
    std::vector<up<ExpressionNode>> mParams;
};

class LambdaCreationNode : public ExpressionNode {
public:
    LambdaCreationNode(SourceCodeRef source, std::vector<Parameter> parameters, Datatype returnType, up<ScopeNode> body);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] inline const auto& getReturnType() const {
        return mReturnType;
    }
    [[nodiscard]] inline const auto& getParameters() const {
        return mParameters;
    }
    [[nodiscard]] inline const auto& getBody() const {
        return mBody;
    }
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "LambdaCreationNode"; }

private:
    Datatype mReturnType;
    std::vector<Parameter> mParameters;
    up<ScopeNode> mBody;
};

class ScopeNode : public ExpressionNode {
public:
    explicit ScopeNode(SourceCodeRef source, std::vector<up<StatementNode>> expressions);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] inline const std::vector<up<StatementNode>>& getExpressions() const {
        return mExpressions;
    }
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "ScopeNode"; }

private:
    std::vector<up<StatementNode>> mExpressions;
};

using IfExpressionChildList = std::vector<std::pair<up<ExpressionNode>, up<ScopeNode>>>;
class IfExpressionNode : public ExpressionNode {
public:
    IfExpressionNode(SourceCodeRef source, IfExpressionChildList children, up<ScopeNode> elseBody);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] inline const auto& getChildren() const {
        return mChildren;
    }
    [[nodiscard]] inline const auto& getElseBody() const {
        return mElseBody;
    }
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "IfExpressionNode"; }

private:
    IfExpressionChildList mChildren;
    up<ScopeNode> mElseBody;
};

class FunctionCallExpressionNode : public ExpressionNode {
public:
    FunctionCallExpressionNode(SourceCodeRef source, up<ExpressionNode> name, std::vector<up<ExpressionNode>> params);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] inline const auto& getName() const {
        return mName;
    }
    [[nodiscard]] inline const auto& getParams() const {
        return mParams;
    }
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "FunctionCallExpressionNode"; }

private:
    up<ExpressionNode> mName;
    std::vector<up<ExpressionNode>> mParams;
};

class FunctionChainExpressionNode : public ExpressionNode {
public:
    FunctionChainExpressionNode(SourceCodeRef source, up<ExpressionNode> initialValue, up<FunctionCallExpressionNode> functionCall);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] inline const auto& getInitialValue() const {
        return mInitialValue;
    }
    [[nodiscard]] inline const auto& getFunctionCall() const {
        return mFunctionCall;
    }
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "FunctionChainExpressionNode"; }

private:
    up<ExpressionNode> mInitialValue;
    up<FunctionCallExpressionNode> mFunctionCall;
};

// TODO delete
class ListAccessExpressionNode : public ExpressionNode {
public:
    ListAccessExpressionNode(SourceCodeRef source, up<ExpressionNode> name, up<ExpressionNode> index);
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "ListAccessExpressionNode"; }

private:
    up<ExpressionNode> mName, mIndex;
};

class ListPropertyAccessExpression : public ExpressionNode {
public:
    enum class ListProperty {
        HEAD,
        TAIL
    };
    ListPropertyAccessExpression(SourceCodeRef source, up<ExpressionNode> list, ListProperty property);
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] const auto& getList() const {
        return mList;
    }
    [[nodiscard]] const auto& getProperty() const {
        return mProperty;
    }
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "ListPropertyAccessExpression"; }

private:
    up<ExpressionNode> mList;
    ListProperty mProperty;
};

class TupleAccessExpressionNode : public ExpressionNode {
public:
    TupleAccessExpressionNode(SourceCodeRef source, up<ExpressionNode> name, uint32_t index);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] const auto& getTuple() const {
        return mTuple;
    }
    [[nodiscard]] auto getIndex() const {
        return mIndex;
    }
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "TupleAccessExpressionNode"; }

private:
    up<ExpressionNode> mTuple;
    uint32_t mIndex;
};

class StructFieldAccessExpression : public ExpressionNode {
public:
    StructFieldAccessExpression(SourceCodeRef source, up<ExpressionNode> struct_, std::string fieldName);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] const auto& getStruct() const {
        return mStruct;
    }
    [[nodiscard]] auto getFieldName() const {
        return mFieldName;
    }
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "StructFieldAccessExpression"; }

private:
    up<ExpressionNode> mStruct;
    std::string mFieldName;
};

struct MatchCompileReturn {
    std::vector<int32_t> labelsToInsertJumpToNext;
};

class PrefixExpression : public ExpressionNode {
public:
    enum class Type {
        MOVE_TO_HEAP,
        MOVE_TO_STACK,
        LOGICAL_NOT
    };
    PrefixExpression(SourceCodeRef source, up<ExpressionNode> child, Type type);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] const auto& getChild() const {
        return mChild;
    }
    [[nodiscard]] auto getType() const {
        return mType;
    }
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "PrefixExpression"; }

private:
    up<ExpressionNode> mChild;
    Type mType;
};

class MatchCondition : public ASTNode {
public:
    explicit MatchCondition(SourceCodeRef source);
    virtual MatchCompileReturn compileTryMatch(Compiler& comp, const Datatype& toMatch, int32_t offset) = 0;
    [[nodiscard]] inline const char* getClassName() const override { return "MatchCondition"; }

};

class EnumFieldMatchCondition : public MatchCondition {
public:
    EnumFieldMatchCondition(SourceCodeRef source, std::string fieldName, std::vector<up<MatchCondition>> elementsToMatch);
    MatchCompileReturn compileTryMatch(Compiler& comp, const Datatype& toMatch, int32_t offset) override;
    [[nodiscard]] inline const auto& getFieldName() const {
        return mFieldName;
    }
    [[nodiscard]] inline const auto& getChildElementsToMatch() const {
        return mElementsToMatch;
    }
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "EnumFieldMatchCondition"; }

private:
    std::string mFieldName;
    std::vector<up<MatchCondition>> mElementsToMatch;
};

class IdentifierMatchCondition : public MatchCondition {
public:
    IdentifierMatchCondition(SourceCodeRef source, std::string newName);
    MatchCompileReturn compileTryMatch(Compiler& comp, const Datatype& toMatch, int32_t offset) override;
    [[nodiscard]] const auto& getNewName() const {
        return mNewName;
    }
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "IdentifierMatchCondition"; }

private:
    std::string mNewName;
};

struct MatchCase {
    up<MatchCondition> condition;
    up<ExpressionNode> codeToRunOnMatch;
};

class MatchExpression : public ExpressionNode {
public:
    MatchExpression(SourceCodeRef source, up<ExpressionNode> toMatch, std::vector<MatchCase> cases);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] inline const auto& getToMatch() const {
        return mToMatch;
    }
    [[nodiscard]] inline const auto& getMatchCases() const {
        return mCases;
    }
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "MatchExpression"; }

private:
    up<ExpressionNode> mToMatch;
    std::vector<MatchCase> mCases;
};

class ModuleRootNode : public CompilableASTNode {
public:
    explicit ModuleRootNode(SourceCodeRef source, std::vector<up<DeclarationNode>>&& declarations);
    Datatype compile(Compiler& comp) const override;
    [[nodiscard]] const std::vector<up<DeclarationNode>>& getDeclarations() const;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "ModuleRootNode"; }
    std::vector<DeclarationNode*> createDeclarationList();
    void setModuleName(std::string name);
    [[nodiscard]] const std::string& getModuleName() const;

private:
    std::vector<up<DeclarationNode>> mDeclarations;
    std::string mName;
};

class DeclarationNode : public CompilableASTNode {
public:
    explicit DeclarationNode(SourceCodeRef source);
    [[nodiscard]] virtual std::string getDeclaredName() const = 0;
    [[nodiscard]] inline const char* getClassName() const override { return "DeclarationNode"; }
private:
};

class TypeOrCallableDeclarationAstNode : public DeclarationNode {
public:
    explicit TypeOrCallableDeclarationAstNode(SourceCodeRef source);
    [[nodiscard]] std::string getDeclaredName() const override;
    [[nodiscard]] virtual bool hasTemplateParameters() const = 0;
    [[nodiscard]] virtual std::vector<std::string> getTemplateParameterVector() const = 0;
    [[nodiscard]] virtual const IdentifierNode* getIdentifier() const = 0;
    [[nodiscard]] inline const char* getClassName() const override { return "TypeOrCallableDeclarationAstNode"; }
};

class CallableDeclarationNode : public TypeOrCallableDeclarationAstNode {
public:
    explicit CallableDeclarationNode(SourceCodeRef source);
    [[nodiscard]] virtual Datatype getDatatype() const = 0;
    [[nodiscard]] inline const char* getClassName() const override { return "CallableDeclarationNode"; }
};

class FunctionDeclarationNode : public CallableDeclarationNode {
public:
    FunctionDeclarationNode(SourceCodeRef source, up<IdentifierNode> name, std::vector<Parameter> params, Datatype returnType, up<ScopeNode> body);
    [[nodiscard]] bool hasTemplateParameters() const override;
    [[nodiscard]] std::vector<std::string> getTemplateParameterVector() const override;
    [[nodiscard]] const IdentifierNode* getIdentifier() const override;
    [[nodiscard]] inline const std::vector<Parameter>& getParameters() const {
        return mParameters;
    }
    [[nodiscard]] inline const Datatype& getReturnType() const {
        return mReturnType;
    }
    [[nodiscard]] inline const up<ScopeNode>& getBody() const {
        return mBody;
    }
    [[nodiscard]] Datatype getDatatype() const override;
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "FunctionDeclarationNode"; }

private:
    up<IdentifierNode> mName;
    std::vector<Parameter> mParameters;
    Datatype mReturnType;
    up<ScopeNode> mBody;
};

class NativeFunctionDeclarationNode : public CallableDeclarationNode {
public:
    NativeFunctionDeclarationNode(SourceCodeRef source, up<IdentifierNode> name, std::vector<Parameter> params, Datatype returnType);
    [[nodiscard]] bool hasTemplateParameters() const override;
    [[nodiscard]] std::vector<std::string> getTemplateParameterVector() const override;
    [[nodiscard]] const IdentifierNode* getIdentifier() const override;
    [[nodiscard]] inline const std::vector<Parameter>& getParameters() const {
        return mParameters;
    }
    [[nodiscard]] inline const Datatype& getReturnType() const {
        return mReturnType;
    }
    [[nodiscard]] Datatype getDatatype() const override;
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "NativeFunctionDeclarationNode"; }

private:
    up<IdentifierNode> mName;
    std::vector<Parameter> mParameters;
    Datatype mReturnType;
};

class StructDeclarationNode : public TypeOrCallableDeclarationAstNode {
public:
    StructDeclarationNode(SourceCodeRef source, up<IdentifierNode> name, std::vector<Parameter> values);
    [[nodiscard]] bool hasTemplateParameters() const override;
    [[nodiscard]] std::vector<std::string> getTemplateParameterVector() const override;
    [[nodiscard]] const IdentifierNode* getIdentifier() const override;
    [[nodiscard]] inline const std::vector<Parameter>& getFields() const {
        return mValues;
    }
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "StructDeclarationNode"; }

private:
    up<IdentifierNode> mName;
    std::vector<Parameter> mValues;
};

class EnumDeclarationNode : public TypeOrCallableDeclarationAstNode {
public:
    EnumDeclarationNode(SourceCodeRef source, up<IdentifierNode> name, std::vector<EnumField> fields);
    [[nodiscard]] bool hasTemplateParameters() const override;
    [[nodiscard]] std::vector<std::string> getTemplateParameterVector() const override;
    [[nodiscard]] const IdentifierNode* getIdentifier() const override;
    [[nodiscard]] inline const std::vector<EnumField>& getFields() const {
        return mFields;
    }
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] std::string dump(unsigned indent) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "EnumDeclarationNode"; }

private:
    up<IdentifierNode> mName;
    std::vector<EnumField> mFields;
};

class UsingDeclaration : public DeclarationNode {
public:
    UsingDeclaration(SourceCodeRef source, std::string usingModuleName);
    [[nodiscard]] inline std::string getDeclaredName() const override {
        return mUsingModuleName;
    }
    [[nodiscard]] const std::string& getUsingModuleName() const {
        return mUsingModuleName;
    }
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "UsingDeclaration"; }

private:
    std::string mUsingModuleName;
};

class TypedefDeclaration : public DeclarationNode {
public:
    inline TypedefDeclaration(SourceCodeRef source, std::string newTypeName, const Datatype& originalType)
    : DeclarationNode(source), mNewTypeName(std::move(newTypeName)), mOriginalType(originalType) {

    }
    [[nodiscard]] inline std::string getDeclaredName() const override {
        return mNewTypeName;
    }
    [[nodiscard]] const auto& getNewTypeName() const {
        return mNewTypeName;
    }
    [[nodiscard]] const auto& getOriginalType() const {
        return mOriginalType;
    }
    Datatype compile(Compiler& comp) const override;
    void findUsedVariables(VariableSearcher&) const override;
    [[nodiscard]] inline const char* getClassName() const override { return "TypedefDeclaration"; }

private:
    std::string mNewTypeName;
    Datatype mOriginalType;
};

Datatype getFunctionType(const Datatype& returnType, const std::vector<Parameter>& params);

}