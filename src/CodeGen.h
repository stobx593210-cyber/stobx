#ifndef CODEGEN_H
#define CODEGEN_H

#include "Parser.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class CodeGen : public stobx::ASTVisitor {
public:
    CodeGen();
    ~CodeGen() override = default;

    void generate(stobx::ProgramASTNode* program);
    void printIR();

    // ASTVisitor overrides
    void visit(stobx::NumberASTNode& node) override;
    void visit(stobx::StringASTNode& node) override;
    void visit(stobx::CharLiteralASTNode& node) override;
    void visit(stobx::BoolLiteralASTNode& node) override;
    void visit(stobx::NullLiteralASTNode& node) override;
    void visit(stobx::VariableASTNode& node) override;
    void visit(stobx::ScopeResolutionASTNode& node) override;
    void visit(stobx::FieldAccessASTNode& node) override;
    void visit(stobx::IndexASTNode& node) override;
    void visit(stobx::TryASTNode& node) override;
    void visit(stobx::FunctionCallASTNode& node) override;
    void visit(stobx::UnaryASTNode& node) override;
    void visit(stobx::CastASTNode& node) override;
    void visit(stobx::SizeOfASTNode& node) override;
    void visit(stobx::BinaryASTNode& node) override;
    void visit(stobx::PrintASTNode& node) override;
    void visit(stobx::AssignmentASTNode& node) override;
    void visit(stobx::ProgramASTNode& node) override;
    void visit(stobx::ModuleDeclASTNode& node) override;
    void visit(stobx::ImportDeclASTNode& node) override;
    void visit(stobx::StructDeclASTNode& node) override;
    void visit(stobx::UnionDeclASTNode& node) override;
    void visit(stobx::EnumDeclASTNode& node) override;
    void visit(stobx::WildcardPatternASTNode& node) override;
    void visit(stobx::LiteralPatternASTNode& node) override;
    void visit(stobx::IdentifierPatternASTNode& node) override;
    void visit(stobx::PathPatternASTNode& node) override;
    void visit(stobx::MatchArmASTNode& node) override;
    void visit(stobx::MatchASTNode& node) override;
    void visit(stobx::IfASTNode& node) override;
    void visit(stobx::WhileASTNode& node) override;
    void visit(stobx::ForASTNode& node) override;
    void visit(stobx::BreakASTNode& node) override;
    void visit(stobx::ContinueASTNode& node) override;
    void visit(stobx::ParameterASTNode& node) override;
    void visit(stobx::FunctionASTNode& node) override;
    void visit(stobx::ImplBlockASTNode& node) override;
    void visit(stobx::VariableDeclarationASTNode& node) override;
    void visit(stobx::ReturnASTNode& node) override;

private:
    struct VariableInfo {
        llvm::AllocaInst* allocation = nullptr;
        std::string typeName;
        std::string pointeeTypeName;
        bool isString = false;
    };

    struct FunctionInfo {
        std::string returnTypeName;
        std::vector<std::string> paramTypeNames;
        bool isVoid = false;
        bool returnIsString = false;
    };

    struct StructInfo {
        llvm::StructType* llvmType = nullptr;
        std::unordered_map<std::string, unsigned> fieldIndices;
        std::vector<std::string> fieldTypeNames;
        bool isUnion = false;
    };

    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;

    std::vector<std::unordered_map<std::string, VariableInfo>> scopes;
    std::unordered_map<std::string, FunctionInfo> functionTable;
    std::unordered_map<std::string, StructInfo> structTable;
    std::unordered_map<std::string, long long> enumConstants;
    std::vector<llvm::BasicBlock*> breakTargets;
    std::vector<llvm::BasicBlock*> continueTargets;

    llvm::Value* lastValue = nullptr;
    std::string lastTypeName;
    bool lastIsString = false;

    llvm::Value* currentMatchScrutinee = nullptr;
    std::string currentMatchScrutineeTypeName;

    llvm::Function* printfFunction = nullptr;
    llvm::Value* intFormat = nullptr;
    llvm::Value* uintFormat = nullptr;
    llvm::Value* i64Format = nullptr;
    llvm::Value* u64Format = nullptr;
    llvm::Value* floatFormat = nullptr;
    llvm::Value* ptrFormat = nullptr;
    llvm::Value* stringFormat = nullptr;
    llvm::Value* nullString = nullptr;
    llvm::Value* trueString = nullptr;
    llvm::Value* falseString = nullptr;

    void initPrintHelpers();
    void pushScope();
    void popScope();
    bool declareInCurrentScope(const std::string& name, VariableInfo info);
    VariableInfo* findVariable(const std::string& name) noexcept;
    const VariableInfo* findVariable(const std::string& name) const noexcept;
    void terminateUnfinishedBlocks(llvm::Function* fn);
    void createDeadContinuationBlock(llvm::Function* fn, const char* name);
    void verifyCurrentModule();

    static bool isFloatTypeName(const std::string& typeName) noexcept;
    static bool isSignedIntTypeName(const std::string& typeName) noexcept;
    static bool isUnsignedIntTypeName(const std::string& typeName) noexcept;
    static bool isStringTypeName(const std::string& typeName) noexcept;
    static bool isPointerTypeName(const std::string& typeName) noexcept;
    static std::string stripPointerPrefix(const std::string& typeName, int* outDepth = nullptr);
    static std::string inferPointeeTypeName(const std::string& typeName);
    static std::string resolveTypeName(const stobx::TypeSpec& spec, std::string_view fallback);
    static std::string inferTypeNameFromLLVMType(
        llvm::Type* ty,
        bool preferUnsigned = false,
        bool isString = false
    );

    llvm::Type* getTypeFromName(const std::string& typeName);
    llvm::Type* getTypeFromSpec(const stobx::TypeSpec& spec, std::string_view fallback = {});
    llvm::Function* getPrintfFunction();

    void predeclareFunction(const stobx::FunctionASTNode& fnNode, const std::string& effectiveName);
    void emitFunctionBody(stobx::FunctionASTNode& fnNode, const std::string& effectiveName);

    llvm::Value* getAddressOfExpression(
        stobx::ASTNode* node,
        std::string* outTypeName = nullptr,
        std::string* outPointeeTypeName = nullptr,
        bool* outIsString = nullptr
    );

    llvm::Value* castValueToType(
        llvm::Value* value,
        const std::string& targetTypeName,
        llvm::Type* targetType
    );

    std::string inferExpressionTypeName(stobx::ASTNode* node, llvm::Value* val);
    llvm::Value* generateExpression(
        stobx::ASTNode* node,
        std::string* outTypeName = nullptr,
        bool* outIsString = nullptr
    );
    void generateStatement(stobx::ASTNode* node);
};

#endif
