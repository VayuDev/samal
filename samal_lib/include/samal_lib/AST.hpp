#pragma once
#include "Datatype.hpp"
#include <string>
#include "samal_lib/Forward.hpp"

namespace samal {

class ASTNode {
 public:
  virtual ~ASTNode() = default;
  virtual void completeDatatype(DatatypeCompleter &declList) {};
  [[nodiscard]] virtual std::string dump(unsigned indent) const;
  [[nodiscard]] virtual inline const char* getClassName() const { return "ASTNode"; }
 private:
};

class ParameterListNode : public ASTNode {
 public:
  struct Parameter {
    up<IdentifierNode> name;
    Datatype type;
  };
  explicit ParameterListNode(std::vector<Parameter> params);
  void completeDatatype(DatatypeCompleter &declList) override;
  [[nodiscard]] const std::vector<Parameter>& getParams();
  [[nodiscard]] std::string dump(unsigned indent) const override;
  [[nodiscard]] inline const char* getClassName() const override { return "ParameterListNode"; }
 private:
  std::vector<Parameter> mParams;
};

class ParameterListNodeWithoutDatatypes : public ASTNode {
 public:
  explicit ParameterListNodeWithoutDatatypes(std::vector<up<ExpressionNode>> params);
  void completeDatatype(DatatypeCompleter &declList) override;
  [[nodiscard]] std::string dump(unsigned indent) const override;
  [[nodiscard]] inline const char* getClassName() const override { return "ParameterListNodeWithoutDatatypes"; }
 private:
  std::vector<up<ExpressionNode>> mParams;
};


class ExpressionNode : public ASTNode {
 public:
  void completeDatatype(DatatypeCompleter &declList) override = 0;
  [[nodiscard]] virtual std::optional<Datatype> getDatatype() const = 0;
  [[nodiscard]] inline const char* getClassName() const override { return "ExpressionNode"; }
 private:
};

class AssignmentExpression : public ExpressionNode {
 public:
  AssignmentExpression(up<IdentifierNode> left, up<ExpressionNode> right);
  void completeDatatype(DatatypeCompleter &declList) override;
  [[nodiscard]] std::optional<Datatype> getDatatype() const override;
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
    LOGICAL_AND,
    LOGICAL_OR,
    LOGICAL_EQUALS,
    LOGICAL_NOT_EQUALS,
    COMPARISON_LESS_THAN,
    COMPARISON_LESS_EQUAL_THAN,
    COMPARISON_MORE_THAN,
    COMPARISON_MORE_EQUAL_THAN,

  };
  BinaryExpressionNode(up<ExpressionNode> left, BinaryOperator op, up<ExpressionNode> right);
  void completeDatatype(DatatypeCompleter &declList) override;
  [[nodiscard]] std::optional<Datatype> getDatatype() const override;
  [[nodiscard]] std::string dump(unsigned indent) const override;
  [[nodiscard]] inline const char* getClassName() const override { return "BinaryExpressionNode"; }
 private:
  up<ExpressionNode> mLeft;
  BinaryOperator mOperator;
  up<ExpressionNode> mRight;
};

class LiteralNode : public ExpressionNode {
 public:

  [[nodiscard]] inline const char* getClassName() const override { return "LiteralNode"; }
 private:
};

class LiteralInt32Node : public LiteralNode {
 public:
  explicit LiteralInt32Node(int32_t val);
  void completeDatatype(DatatypeCompleter &declList) override;
  [[nodiscard]] std::optional<Datatype> getDatatype() const override;
  [[nodiscard]] std::string dump(unsigned indent) const override;
  [[nodiscard]] inline const char* getClassName() const override { return "LiteralIntNode"; }
 private:
  int32_t mValue;
};

class IdentifierNode : public ExpressionNode {
 public:
  explicit IdentifierNode(std::string name);
  void completeDatatype(DatatypeCompleter &declList) override;
  [[nodiscard]] const std::string& getName() const;
  [[nodiscard]] std::optional<Datatype> getDatatype() const override;
  [[nodiscard]] std::string dump(unsigned indent) const override;
  [[nodiscard]] inline const char* getClassName() const override { return "IdentifierNode"; }
 private:
  std::string mName;
  std::optional<std::pair<Datatype, int32_t>> mDatatype;
};

class ScopeNode : public ExpressionNode {
 public:
  explicit ScopeNode(std::vector<up<ExpressionNode>> expressions);
  void completeDatatype(DatatypeCompleter &declList) override;
  [[nodiscard]] virtual std::optional<Datatype> getDatatype() const;
  [[nodiscard]] std::string dump(unsigned indent) const override;
  [[nodiscard]] inline const char* getClassName() const override { return "ScopeNode"; }
 private:
  std::vector<up<ExpressionNode>> mExpressions;
};

using IfExpressionChildList = std::vector<std::pair<up<ExpressionNode>, up<ScopeNode>>>;
class IfExpressionNode : public ExpressionNode {
 public:
  IfExpressionNode(IfExpressionChildList children, up<ScopeNode> elseBody);
  void completeDatatype(DatatypeCompleter &declList) override;
  [[nodiscard]] std::optional<Datatype> getDatatype() const override;
  [[nodiscard]] std::string dump(unsigned indent) const override;
  [[nodiscard]] inline const char* getClassName() const override { return "IfExpressionNode"; }
 private:
  IfExpressionChildList mChildren;
  up<ScopeNode> mElseBody;
};

class FunctionCallExpressionNode : public ExpressionNode {
 public:
  FunctionCallExpressionNode(up<ExpressionNode> name, up<ParameterListNodeWithoutDatatypes> params);
  [[nodiscard]] std::optional<Datatype> getDatatype() const override;
  void completeDatatype(DatatypeCompleter &declList) override;
  [[nodiscard]] std::string dump(unsigned indent) const override;
  [[nodiscard]] inline const char* getClassName() const override { return "FunctionCallExpressionNode"; }
 private:
  up<ExpressionNode> mName;
  up<ParameterListNodeWithoutDatatypes> mParams;
};


class DeclarationNode : public ASTNode {
 public:
  virtual void declareShallow(DatatypeCompleter& completer) const = 0;
  [[nodiscard]] inline const char* getClassName() const override { return "DeclarationNode"; }
 private:
};

class ModuleRootNode : public ASTNode {
 public:
  explicit ModuleRootNode(std::vector<up<DeclarationNode>>&& declarations);
  void completeDatatype(DatatypeCompleter &declList) override;
  void declareShallow(DatatypeCompleter& completer) const;
  [[nodiscard]] std::string dump(unsigned indent) const override;
  [[nodiscard]] inline const char* getClassName() const override { return "ModuleRootNode"; }
  std::vector<DeclarationNode*> createDeclarationList();
 private:
  std::vector<up<DeclarationNode>> mDeclarations;
};

class FunctionDeclarationNode : public DeclarationNode {
 public:
  FunctionDeclarationNode(up<IdentifierNode> name, up<ParameterListNode> params, Datatype returnType, up<ScopeNode> body);
  void completeDatatype(DatatypeCompleter &declList) override;
  void declareShallow(DatatypeCompleter& completer) const;
  [[nodiscard]] std::string dump(unsigned indent) const override;
  [[nodiscard]] inline const char* getClassName() const override { return "FunctionDeclarationNode"; }
 private:
  up<IdentifierNode> mName;
  up<ParameterListNode> mParameters;
  Datatype mReturnType;
  up<ScopeNode> mBody;
};

}