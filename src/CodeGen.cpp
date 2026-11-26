#include "CodeGen.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <cerrno>
#include <cstdlib>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace stobx;

namespace {

std::string currentModulePrefix;
std::unordered_map<std::string, std::string> importedAliases;
std::unordered_set<std::string> importedSymbols;

std::string applyModulePrefix(const std::string& name) {
    if (name.empty() || currentModulePrefix.empty()) {
        return name;
    }
    const std::string prefixWithSep = currentModulePrefix + "__";
    if (name.rfind(prefixWithSep, 0) == 0) {
        return name;
    }
    return prefixWithSep + name;
}

std::string resolveImportedOrPrefixedName(const std::string& name) {
    const auto aliasIt = importedAliases.find(name);
    if (aliasIt != importedAliases.end()) {
        return aliasIt->second;
    }
    if (!currentModulePrefix.empty()) {
        return applyModulePrefix(name);
    }
    return name;
}

bool hasLiveReturnInstruction(llvm::Function* fn) {
    if (!fn) {
        return false;
    }
    std::set<llvm::BasicBlock*> reachable;
    std::vector<llvm::BasicBlock*> stack;
    llvm::BasicBlock* entry = &fn->getEntryBlock();
    reachable.insert(entry);
    stack.push_back(entry);

    while (!stack.empty()) {
        llvm::BasicBlock* bb = stack.back();
        stack.pop_back();
        for (llvm::Instruction& inst : *bb) {
            if (llvm::isa<llvm::ReturnInst>(&inst)) {
                return true;
            }
        }
        if (const auto* term = bb->getTerminator()) {
            if (llvm::isa<llvm::UnreachableInst>(term)) {
                continue;
            }
            for (unsigned i = 0; i < term->getNumSuccessors(); ++i) {
                llvm::BasicBlock* succ = term->getSuccessor(i);
                if (succ && reachable.insert(succ).second) {
                    stack.push_back(succ);
                }
            }
        }
    }
    return false;
}

bool allReachablePathsReturn(llvm::Function* fn) {
    if (!fn) {
        return false;
    }
    std::set<llvm::BasicBlock*> visited;
    std::vector<llvm::BasicBlock*> stack;
    llvm::BasicBlock* entry = &fn->getEntryBlock();
    visited.insert(entry);
    stack.push_back(entry);

    bool sawReturnPath = false;

    while (!stack.empty()) {
        llvm::BasicBlock* bb = stack.back();
        stack.pop_back();

        const auto* term = bb->getTerminator();
        if (!term) {
            return false;
        }
        if (llvm::isa<llvm::UnreachableInst>(term)) {
            if (bb->getName().starts_with("after.break") ||
                bb->getName().starts_with("after.continue") ||
                bb->getName().starts_with("after.return")) {
                continue;
            }
            return false;
        }
        if (llvm::isa<llvm::ReturnInst>(term)) {
            sawReturnPath = true;
            continue;
        }
        if (term->getNumSuccessors() == 0) {
            return false;
        }
        for (unsigned i = 0; i < term->getNumSuccessors(); ++i) {
            llvm::BasicBlock* succ = term->getSuccessor(i);
            if (succ && visited.insert(succ).second) {
                stack.push_back(succ);
            }
        }
    }

    return sawReturnPath;
}

} // namespace

CodeGen::CodeGen() {
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("StobxModule", *context);
    builder = std::make_unique<llvm::IRBuilder<>>(*context);
}

void CodeGen::initPrintHelpers() {
    printfFunction = getPrintfFunction();
    intFormat = builder->CreateGlobalStringPtr("%d\n", "intFormat", 0, module.get());
    uintFormat = builder->CreateGlobalStringPtr("%u\n", "uintFormat", 0, module.get());
    i64Format = builder->CreateGlobalStringPtr("%lld\n", "i64Format", 0, module.get());
    u64Format = builder->CreateGlobalStringPtr("%llu\n", "u64Format", 0, module.get());
    floatFormat = builder->CreateGlobalStringPtr("%f\n", "floatFormat", 0, module.get());
    ptrFormat = builder->CreateGlobalStringPtr("%p\n", "ptrFormat", 0, module.get());
    stringFormat = builder->CreateGlobalStringPtr("%s\n", "stringFormat", 0, module.get());
    nullString = builder->CreateGlobalStringPtr("(null)", "nullString", 0, module.get());
    trueString = builder->CreateGlobalStringPtr("true", "trueString", 0, module.get());
    falseString = builder->CreateGlobalStringPtr("false", "falseString", 0, module.get());
}

void CodeGen::pushScope() {
    scopes.emplace_back();
}

void CodeGen::popScope() {
    if (!scopes.empty()) {
        scopes.pop_back();
    }
}

bool CodeGen::declareInCurrentScope(const std::string& name, VariableInfo info) {
    if (scopes.empty()) {
        pushScope();
    }
    auto& current = scopes.back();
    if (current.count(name)) {
        return false;
    }
    current.emplace(name, std::move(info));
    return true;
}

CodeGen::VariableInfo* CodeGen::findVariable(const std::string& name) noexcept {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &found->second;
        }
    }
    if (!currentModulePrefix.empty()) {
        const std::string prefixed = applyModulePrefix(name);
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(prefixed);
            if (found != it->end()) {
                return &found->second;
            }
        }
    }
    return nullptr;
}

const CodeGen::VariableInfo* CodeGen::findVariable(const std::string& name) const noexcept {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &found->second;
        }
    }
    if (!currentModulePrefix.empty()) {
        const std::string prefixed = applyModulePrefix(name);
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(prefixed);
            if (found != it->end()) {
                return &found->second;
            }
        }
    }
    return nullptr;
}

void CodeGen::createDeadContinuationBlock(llvm::Function* fn, const char* name) {
    if (!fn) {
        return;
    }
    llvm::BasicBlock* deadBlock = llvm::BasicBlock::Create(*context, name, fn);
    builder->SetInsertPoint(deadBlock);
}

void CodeGen::terminateUnfinishedBlocks(llvm::Function* fn) {
    if (!fn) {
        return;
    }

    const auto savedIP = builder->saveIP();
    for (llvm::BasicBlock& bb : *fn) {
        if (!bb.getTerminator()) {
            builder->SetInsertPoint(&bb);
            builder->CreateUnreachable();
        }
    }
    if (savedIP.isSet()) {
        builder->restoreIP(savedIP);
    }
}

void CodeGen::verifyCurrentModule() {
    std::string verifyErr;
    llvm::raw_string_ostream verifyStream(verifyErr);
    if (llvm::verifyModule(*module, &verifyStream)) {
        throw std::runtime_error("LLVM module verification failed:\n" + verifyErr);
    }
}

bool CodeGen::isFloatTypeName(const std::string& typeName) noexcept {
    return typeName == "f32" || typeName == "f64";
}

bool CodeGen::isSignedIntTypeName(const std::string& typeName) noexcept {
    return typeName == "i1" ||
           typeName == "i8" ||
           typeName == "i16" ||
           typeName == "i32" ||
           typeName == "i64";
}

bool CodeGen::isUnsignedIntTypeName(const std::string& typeName) noexcept {
    return typeName == "u1" ||
           typeName == "u8" ||
           typeName == "u16" ||
           typeName == "u32" ||
           typeName == "u64";
}

bool CodeGen::isStringTypeName(const std::string& typeName) noexcept {
    return typeName == "string" || typeName == "str";
}

bool CodeGen::isPointerTypeName(const std::string& typeName) noexcept {
    return !typeName.empty() && typeName.front() == '*';
}

std::string CodeGen::stripPointerPrefix(const std::string& typeName, int* outDepth) {
    int depth = 0;
    while (depth < static_cast<int>(typeName.size()) && typeName[depth] == '*') {
        ++depth;
    }
    if (outDepth) {
        *outDepth = depth;
    }
    return typeName.substr(static_cast<std::size_t>(depth));
}

std::string CodeGen::inferPointeeTypeName(const std::string& typeName) {
    if (isPointerTypeName(typeName)) {
        return typeName.substr(1);
    }
    if (isStringTypeName(typeName)) {
        return "char";
    }
    if (!typeName.empty() && typeName.front() == '[' && typeName.back() == ']') {
        return typeName.substr(1, typeName.size() - 2);
    }
    return "i64";
}

std::string CodeGen::resolveTypeName(const TypeSpec& spec, std::string_view fallback) {
    std::string base = !spec.name.empty()
        ? spec.name
        : (!fallback.empty() ? std::string(fallback) : std::string("i64"));

    const auto aliasIt = importedAliases.find(base);
    if (aliasIt != importedAliases.end()) {
        base = aliasIt->second;
    } else if (!currentModulePrefix.empty() &&
               base != "void" && base != "bool" && base != "char" &&
               base != "string" && base != "str" &&
               base != "f32" && base != "f64" &&
               !isSignedIntTypeName(base) && !isUnsignedIntTypeName(base)) {
        base = applyModulePrefix(base);
    }

    if (spec.elementType) {
        const std::string elem = resolveTypeName(*spec.elementType, "i64");
        base = "[" + elem + "]";
    }

    if (spec.pointerDepth > 0) {
        base = std::string(spec.pointerDepth, '*') + base;
    }

    return base;
}

std::string CodeGen::inferTypeNameFromLLVMType(
    llvm::Type* ty,
    bool preferUnsigned,
    bool isString
) {
    if (!ty) {
        return "i64";
    }
    if (ty->isVoidTy()) {
        return "void";
    }
    if (ty->isFloatTy()) {
        return "f32";
    }
    if (ty->isDoubleTy()) {
        return "f64";
    }
    if (ty->isPointerTy()) {
        return isString ? "string" : "*i64";
    }
    if (ty->isIntegerTy()) {
        const unsigned bits = ty->getIntegerBitWidth();
        if (bits == 1 && !preferUnsigned) {
            return "bool";
        }
        const char prefix = preferUnsigned ? 'u' : 'i';
        return std::string(1, prefix) + std::to_string(bits);
    }
    if (auto* stTy = llvm::dyn_cast<llvm::StructType>(ty)) {
        if (!stTy->getName().empty()) {
            return stTy->getName().str();
        }
    }
    return "i64";
}

llvm::Type* CodeGen::getTypeFromName(const std::string& typeName) {
    if (isPointerTypeName(typeName)) {
        return builder->getPtrTy();
    }
    if (!typeName.empty() && typeName.front() == '[' && typeName.back() == ']') {
        return builder->getPtrTy();
    }
    if (typeName == "void") {
        return builder->getVoidTy();
    }
    if (typeName == "u1" || typeName == "i1" || typeName == "bool") {
        return builder->getInt1Ty();
    }
    if (typeName == "u8" || typeName == "i8" || typeName == "char") {
        return builder->getInt8Ty();
    }
    if (typeName == "u16" || typeName == "i16") {
        return builder->getInt16Ty();
    }
    if (typeName == "u32" || typeName == "i32") {
        return builder->getInt32Ty();
    }
    if (typeName == "u64" || typeName == "i64") {
        return builder->getInt64Ty();
    }
    if (typeName == "f32") {
        return builder->getFloatTy();
    }
    if (typeName == "f64") {
        return builder->getDoubleTy();
    }
    if (isStringTypeName(typeName)) {
        return builder->getPtrTy();
    }

    const auto stIt = structTable.find(typeName);
    if (stIt != structTable.end() && stIt->second.llvmType) {
        return stIt->second.llvmType;
    }

    const std::string prefixedName = applyModulePrefix(typeName);
    const auto prefIt = structTable.find(prefixedName);
    if (prefIt != structTable.end() && prefIt->second.llvmType) {
        return prefIt->second.llvmType;
    }

    if (auto* existingStruct = llvm::StructType::getTypeByName(*context, typeName)) {
        return existingStruct;
    }
    if (auto* prefixedStruct = llvm::StructType::getTypeByName(*context, prefixedName)) {
        return prefixedStruct;
    }

    throw std::runtime_error("Unknown data type: " + typeName);
}
llvm::Type* CodeGen::getTypeFromSpec(const TypeSpec& spec, std::string_view fallback) {
    if (spec.isFunctionType) {
        return builder->getPtrTy();
    }
    if (spec.pointerDepth > 0) {
        return builder->getPtrTy();
    }
    if (spec.elementType) {
        llvm::Type* elemTy = getTypeFromSpec(*spec.elementType);
        if (spec.hasArraySize) {
            return llvm::ArrayType::get(elemTy, spec.arraySize);
        }
        return builder->getPtrTy();
    }
    const std::string base = !spec.name.empty()
        ? spec.name
        : (!fallback.empty() ? std::string(fallback) : std::string("i64"));
    return getTypeFromName(base);
}

llvm::Function* CodeGen::getPrintfFunction() {
    llvm::Function* fn = module->getFunction("printf");
    if (!fn) {
        llvm::FunctionType* printfType = llvm::FunctionType::get(
            builder->getInt32Ty(),
            builder->getPtrTy(),
            true
        );
        fn = llvm::Function::Create(
            printfType,
            llvm::Function::ExternalLinkage,
            "printf",
            module.get()
        );
    }
    return fn;
}

llvm::Value* CodeGen::castValueToType(
    llvm::Value* value,
    const std::string& targetTypeName,
    llvm::Type* targetType
) {
    if (!value || !targetType) {
        throw std::runtime_error("Invalid value or target type for cast");
    }

    llvm::Type* srcType = value->getType();
    if (srcType == targetType) {
        return value;
    }

    if (srcType->isPointerTy() && targetType->isPointerTy()) {
        return value;
    }

    if (srcType->isPointerTy() && targetType->isIntegerTy()) {
        return builder->CreatePtrToInt(value, targetType, "ptrtointtmp");
    }

    if (srcType->isIntegerTy() && targetType->isPointerTy()) {
        return builder->CreateIntToPtr(value, targetType, "inttoptrtmp");
    }

    if (srcType->isIntegerTy() && targetType->isIntegerTy()) {
        const bool isSigned = isSignedIntTypeName(targetTypeName);
        return builder->CreateIntCast(value, targetType, isSigned, "casttmp");
    }

    if (srcType->isFloatingPointTy() && targetType->isFloatingPointTy()) {
        if (srcType->getPrimitiveSizeInBits() < targetType->getPrimitiveSizeInBits()) {
            return builder->CreateFPExt(value, targetType, "fpexttmp");
        }
        return builder->CreateFPTrunc(value, targetType, "fptrunctmp");
    }

    if (srcType->isIntegerTy() && targetType->isFloatingPointTy()) {
        if (isUnsignedIntTypeName(targetTypeName)) {
            return builder->CreateUIToFP(value, targetType, "uitofptmp");
        }
        return builder->CreateSIToFP(value, targetType, "sitofptmp");
    }

    if (srcType->isFloatingPointTy() && targetType->isIntegerTy()) {
        if (isUnsignedIntTypeName(targetTypeName)) {
            return builder->CreateFPToUI(value, targetType, "fptouitmp");
        }
        return builder->CreateFPToSI(value, targetType, "fptositmp");
    }

    throw std::runtime_error("Incompatible type conversion");
}

std::string CodeGen::inferExpressionTypeName(ASTNode* node, llvm::Value* val) {
    if (!node) {
        return inferTypeNameFromLLVMType(val ? val->getType() : nullptr);
    }

    if (auto* castExpr = dynamic_cast<CastASTNode*>(node)) {
        return resolveTypeName(castExpr->targetType, "i64");
    }

    if (auto* varExpr = dynamic_cast<VariableASTNode*>(node)) {
        const VariableInfo* info = findVariable(varExpr->name);
        if (info && !info->typeName.empty()) {
            return info->typeName;
        }
        return inferTypeNameFromLLVMType(
            val ? val->getType() : nullptr,
            false,
            info ? info->isString : false
        );
    }

    if (auto* callExpr = dynamic_cast<FunctionCallASTNode*>(node)) {
        const std::string resolvedCallName = resolveImportedOrPrefixedName(callExpr->name);
        auto it = functionTable.find(resolvedCallName);
        if (it == functionTable.end()) {
            it = functionTable.find(callExpr->name);
        }
        if (it != functionTable.end() && !it->second.returnTypeName.empty()) {
            return it->second.returnTypeName;
        }
        return inferTypeNameFromLLVMType(val ? val->getType() : nullptr);
    }

    if (auto* numExpr = dynamic_cast<NumberASTNode*>(node)) {
        if (numExpr->literalKind == TokenType::FLOAT_LITERAL ||
            numExpr->value.find('.') != std::string::npos) {
            return "f64";
        }
        return "i64";
    }

    if (dynamic_cast<StringASTNode*>(node)) {
        return "string";
    }

    if (dynamic_cast<CharLiteralASTNode*>(node)) {
        return "char";
    }

    if (dynamic_cast<BoolLiteralASTNode*>(node)) {
        return "bool";
    }

    if (auto* unaryExpr = dynamic_cast<UnaryASTNode*>(node)) {
        if (unaryExpr->op == "&") {
            std::string innerType = inferExpressionTypeName(unaryExpr->operand.get(), nullptr);
            return "*" + innerType;
        }
        if (unaryExpr->op == "*") {
            std::string innerType = inferExpressionTypeName(unaryExpr->operand.get(), nullptr);
            return inferPointeeTypeName(innerType);
        }
        if (unaryExpr->op == "not") {
            return "bool";
        }
        return inferExpressionTypeName(unaryExpr->operand.get(), val);
    }

    if (auto* binaryExpr = dynamic_cast<BinaryASTNode*>(node)) {
        const std::string& op = binaryExpr->op;
        if (op == "and" || op == "or" ||
            op == "==" || op == "!=" ||
            op == "<" || op == "<=" ||
            op == ">" || op == ">=") {
            return "bool";
        }
        if (op == "<<" || op == ">>") {
            return inferExpressionTypeName(binaryExpr->left.get(), val);
        }
        const std::string leftType = inferExpressionTypeName(binaryExpr->left.get(), nullptr);
        const std::string rightType = inferExpressionTypeName(binaryExpr->right.get(), nullptr);
        if (leftType == "f64" || rightType == "f64") {
            return "f64";
        }
        if (leftType == "f32" || rightType == "f32") {
            return "f32";
        }
        if (isUnsignedIntTypeName(leftType) || isUnsignedIntTypeName(rightType)) {
            if (val && val->getType()->isIntegerTy()) {
                return "u" + std::to_string(val->getType()->getIntegerBitWidth());
            }
            return isUnsignedIntTypeName(leftType) ? leftType : rightType;
        }
        return inferTypeNameFromLLVMType(val ? val->getType() : nullptr);
    }

    return inferTypeNameFromLLVMType(val ? val->getType() : nullptr);
}

llvm::Value* CodeGen::getAddressOfExpression(
    ASTNode* node,
    std::string* outTypeName,
    std::string* outPointeeTypeName,
    bool* outIsString
) {
    if (!node) {
        throw std::runtime_error("Invalid target expression");
    }

    if (auto* varNode = dynamic_cast<VariableASTNode*>(node)) {
        const VariableInfo* info = findVariable(varNode->name);
        if (!info || !info->allocation) {
            throw std::runtime_error("Unknown variable: " + varNode->name);
        }
        if (outTypeName) {
            *outTypeName = info->typeName;
        }
        if (outPointeeTypeName) {
            *outPointeeTypeName = !info->pointeeTypeName.empty()
                ? info->pointeeTypeName
                : inferPointeeTypeName(info->typeName);
        }
        if (outIsString) {
            *outIsString = info->isString;
        }
        return info->allocation;
    }

    if (auto* fieldNode = dynamic_cast<FieldAccessASTNode*>(node)) {
        if (!fieldNode->object) {
            throw std::runtime_error("Invalid object in field access");
        }

        llvm::Value* basePtr = nullptr;
        llvm::StructType* structTy = nullptr;
        std::string structName;

        if (auto* objVar = dynamic_cast<VariableASTNode*>(fieldNode->object.get())) {
            const VariableInfo* info = findVariable(objVar->name);
            if (!info || !info->allocation) {
                throw std::runtime_error("Unknown struct variable: " + objVar->name);
            }
            llvm::Type* allocTy = info->allocation->getAllocatedType();
            if (auto* directStruct = llvm::dyn_cast<llvm::StructType>(allocTy)) {
                basePtr = info->allocation;
                structName = !info->typeName.empty()
                    ? stripPointerPrefix(info->typeName)
                    : directStruct->getName().str();
                structTy = directStruct;
            } else if (allocTy->isPointerTy()) {
                basePtr = builder->CreateLoad(allocTy, info->allocation, objVar->name + ".deref");
                structName = !info->pointeeTypeName.empty()
                    ? stripPointerPrefix(info->pointeeTypeName)
                    : stripPointerPrefix(info->typeName);
            }
        } else {
            std::string objTypeName;
            llvm::Value* objVal = generateExpression(fieldNode->object.get(), &objTypeName);
            if (objVal->getType()->isPointerTy()) {
                basePtr = objVal;
                structName = stripPointerPrefix(objTypeName);
            } else {
                llvm::AllocaInst* tmp = builder->CreateAlloca(objVal->getType(), nullptr, "fieldtmp");
                builder->CreateStore(objVal, tmp);
                basePtr = tmp;
                structTy = llvm::dyn_cast<llvm::StructType>(objVal->getType());
                structName = !objTypeName.empty()
                    ? stripPointerPrefix(objTypeName)
                    : (structTy ? structTy->getName().str() : std::string());
            }
        }

        if (!structTy && !structName.empty()) {
            const auto stIt = structTable.find(structName);
            if (stIt != structTable.end()) {
                structTy = stIt->second.llvmType;
            } else {
                const std::string prefixed = applyModulePrefix(structName);
                const auto prefIt = structTable.find(prefixed);
                if (prefIt != structTable.end()) {
                    structName = prefixed;
                    structTy = prefIt->second.llvmType;
                } else {
                    structTy = llvm::StructType::getTypeByName(*context, structName);
                    if (!structTy) {
                        structTy = llvm::StructType::getTypeByName(*context, prefixed);
                        if (structTy) {
                            structName = prefixed;
                        }
                    }
                }
            }
        }

        if (!structTy) {
            throw std::runtime_error("Field access on non-struct type");
        }

        const std::string lookupName = (!structName.empty() && structTable.count(structName))
            ? structName
            : structTy->getName().str();
        const auto stIt = structTable.find(lookupName);
        if (stIt == structTable.end()) {
            throw std::runtime_error("Unregistered struct type: " + lookupName);
        }

        const auto fIt = stIt->second.fieldIndices.find(fieldNode->field);
        if (fIt == stIt->second.fieldIndices.end()) {
            throw std::runtime_error("Unknown field '" + fieldNode->field + "' in struct");
        }

        const std::string fieldTypeName = (fIt->second < stIt->second.fieldTypeNames.size())
            ? stIt->second.fieldTypeNames[fIt->second]
            : std::string("i64");

        if (outTypeName) {
            *outTypeName = fieldTypeName;
        }
        if (outPointeeTypeName) {
            *outPointeeTypeName = inferPointeeTypeName(fieldTypeName);
        }
        if (outIsString) {
            *outIsString = isStringTypeName(fieldTypeName);
        }

        const unsigned gepIdx = stIt->second.isUnion ? 0u : fIt->second;
        llvm::Value* rawSlotPtr = builder->CreateStructGEP(
            structTy,
            basePtr,
            gepIdx,
            fieldNode->field + ".ptr"
        );

        if (stIt->second.isUnion && rawSlotPtr->getType() != builder->getPtrTy()) {
            rawSlotPtr = builder->CreateBitCast(rawSlotPtr, builder->getPtrTy(), "union.slot.ptr");
        }

        return rawSlotPtr;
    }

    if (auto* idxNode = dynamic_cast<IndexASTNode*>(node)) {
        llvm::Value* idxVal = generateExpression(idxNode->index.get());
        if (!idxVal->getType()->isIntegerTy()) {
            throw std::runtime_error("Array index must be an integer");
        }

        std::string targetTypeName;
        std::string targetPointeeTypeName;
        bool targetIsString = false;

        if (auto* targetVar = dynamic_cast<VariableASTNode*>(idxNode->target.get())) {
            const VariableInfo* info = findVariable(targetVar->name);
            if (!info || !info->allocation) {
                throw std::runtime_error("Unknown array variable: " + targetVar->name);
            }
            targetTypeName = info->typeName;
            targetPointeeTypeName = !info->pointeeTypeName.empty()
                ? info->pointeeTypeName
                : inferPointeeTypeName(info->typeName);
            targetIsString = info->isString;

            llvm::Type* allocatedTy = info->allocation->getAllocatedType();
            if (auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(allocatedTy)) {
                const std::string elemTypeName = targetPointeeTypeName.empty()
                    ? inferTypeNameFromLLVMType(arrTy->getElementType())
                    : targetPointeeTypeName;
                if (outTypeName) {
                    *outTypeName = elemTypeName;
                }
                if (outPointeeTypeName) {
                    *outPointeeTypeName = inferPointeeTypeName(elemTypeName);
                }
                if (outIsString) {
                    *outIsString = isStringTypeName(elemTypeName);
                }
                return builder->CreateGEP(
                    arrTy,
                    info->allocation,
                    {builder->getInt64(0), idxVal},
                    "arridx.ptr"
                );
            }

            if (allocatedTy->isPointerTy()) {
                llvm::Value* ptrVal = builder->CreateLoad(allocatedTy, info->allocation, "ptrload");
                llvm::Type* elemTy = getTypeFromName(targetPointeeTypeName);
                if (elemTy->isVoidTy()) {
                    elemTy = builder->getInt8Ty();
                    targetPointeeTypeName = "i8";
                }
                if (outTypeName) {
                    *outTypeName = targetPointeeTypeName;
                }
                if (outPointeeTypeName) {
                    *outPointeeTypeName = inferPointeeTypeName(targetPointeeTypeName);
                }
                if (outIsString) {
                    *outIsString = isStringTypeName(targetPointeeTypeName);
                }
                return builder->CreateGEP(elemTy, ptrVal, idxVal, "ptridx.ptr");
            }
        }

        llvm::Value* targetVal = generateExpression(
            idxNode->target.get(),
            &targetTypeName,
            &targetIsString
        );
        if (targetVal->getType()->isPointerTy()) {
            const std::string elemTypeName = targetIsString
                ? std::string("char")
                : inferPointeeTypeName(targetTypeName);
            llvm::Type* elemTy = getTypeFromName(elemTypeName);
            if (elemTy->isVoidTy()) {
                elemTy = builder->getInt8Ty();
            }
            if (outTypeName) {
                *outTypeName = elemTypeName;
            }
            if (outPointeeTypeName) {
                *outPointeeTypeName = inferPointeeTypeName(elemTypeName);
            }
            if (outIsString) {
                *outIsString = isStringTypeName(elemTypeName);
            }
            return builder->CreateGEP(elemTy, targetVal, idxVal, "idx.ptr");
        }
    }

    throw std::runtime_error("Expression does not have an assignable memory address");
}

llvm::Value* CodeGen::generateExpression(
    ASTNode* node,
    std::string* outTypeName,
    bool* outIsString
) {
    if (!node) {
        throw std::runtime_error("Invalid expression");
    }
    lastValue = nullptr;
    lastTypeName.clear();
    lastIsString = false;

    node->accept(*this);
    if (!lastValue) {
        throw std::runtime_error("Expression did not produce a value");
    }
    if (lastTypeName.empty()) {
        lastTypeName = inferExpressionTypeName(node, lastValue);
    }
    if (outTypeName) {
        *outTypeName = lastTypeName;
    }
    if (outIsString) {
        *outIsString = lastIsString || isStringTypeName(lastTypeName);
    }
    return lastValue;
}

void CodeGen::generateStatement(ASTNode* node) {
    if (!node) {
        return;
    }
    llvm::BasicBlock* currentBlock = builder->GetInsertBlock();
    if (!currentBlock || currentBlock->getTerminator()) {
        return;
    }
    lastValue = nullptr;
    lastTypeName.clear();
    lastIsString = false;
    node->accept(*this);
}
void CodeGen::visit(NumberASTNode& node) {
    const char* str = node.value.c_str();
    char* endPtr = nullptr;
    errno = 0;

    if (node.literalKind == TokenType::FLOAT_LITERAL ||
        node.value.find('.') != std::string::npos) {
        const double val = std::strtod(str, &endPtr);
        if (errno == ERANGE || endPtr == str || *endPtr != '\0') {
            throw std::runtime_error("Invalid floating-point literal: " + node.value);
        }
        lastValue = llvm::ConstantFP::get(builder->getDoubleTy(), val);
        lastTypeName = "f64";
        lastIsString = false;
        return;
    }

    int base = 10;
    std::string_view sv(node.value);
    if (node.literalKind == TokenType::HEX_LITERAL ||
        (sv.size() > 2 && sv[0] == '0' && (sv[1] == 'x' || sv[1] == 'X'))) {
        base = 16;
        if (sv.size() > 2 && sv[0] == '0' && (sv[1] == 'x' || sv[1] == 'X')) {
            sv.remove_prefix(2);
        }
    } else if (node.literalKind == TokenType::BINARY_LITERAL ||
               (sv.size() > 2 && sv[0] == '0' && (sv[1] == 'b' || sv[1] == 'B'))) {
        base = 2;
        if (sv.size() > 2 && sv[0] == '0' && (sv[1] == 'b' || sv[1] == 'B')) {
            sv.remove_prefix(2);
        }
    }

    const std::string cleanStr(sv);
    const unsigned long long val = std::strtoull(cleanStr.c_str(), &endPtr, base);
    if (errno == ERANGE || endPtr == cleanStr.c_str() || *endPtr != '\0') {
        throw std::runtime_error("Invalid integer literal: " + node.value);
    }
    lastValue = builder->getInt64(static_cast<uint64_t>(val));
    lastTypeName = "i64";
    lastIsString = false;
}

void CodeGen::visit(StringASTNode& node) {
    std::string_view raw(node.value);
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
        raw.remove_prefix(1);
        raw.remove_suffix(1);
    }
    lastValue = builder->CreateGlobalStringPtr(std::string(raw));
    lastTypeName = "string";
    lastIsString = true;
}

void CodeGen::visit(CharLiteralASTNode& node) {
    std::string_view raw(node.value);
    if (raw.size() >= 2 && raw.front() == '\'' && raw.back() == '\'') {
        raw.remove_prefix(1);
        raw.remove_suffix(1);
    }
    unsigned char ch = 0;
    if (!raw.empty()) {
        if (raw[0] == '\\' && raw.size() >= 2) {
            switch (raw[1]) {
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                case '0': ch = '\0'; break;
                case '\\': ch = '\\'; break;
                case '\'': ch = '\''; break;
                case '"': ch = '"'; break;
                default: ch = static_cast<unsigned char>(raw[1]); break;
            }
        } else {
            ch = static_cast<unsigned char>(raw[0]);
        }
    }
    lastValue = builder->getInt8(ch);
    lastTypeName = "char";
    lastIsString = false;
}

void CodeGen::visit(BoolLiteralASTNode& node) {
    lastValue = builder->getInt1(node.value);
    lastTypeName = "bool";
    lastIsString = false;
}

void CodeGen::visit(NullLiteralASTNode&) {
    lastValue = llvm::ConstantPointerNull::get(builder->getPtrTy());
    lastTypeName = "*void";
    lastIsString = false;
}

void CodeGen::visit(VariableASTNode& node) {
    const VariableInfo* info = findVariable(node.name);
    if (!info || !info->allocation) {
        throw std::runtime_error("Unknown variable: " + node.name);
    }
    lastValue = builder->CreateLoad(
        info->allocation->getAllocatedType(),
        info->allocation,
        node.name
    );
    lastTypeName = info->typeName;
    lastIsString = info->isString || isStringTypeName(info->typeName);
}

void CodeGen::visit(ScopeResolutionASTNode& node) {
    if (node.path.empty()) {
        throw std::runtime_error("Empty scope resolution path");
    }

    std::string mangled;
    for (std::size_t i = 0; i < node.path.size(); ++i) {
        if (i > 0) {
            mangled += "__";
        }
        mangled += node.path[i];
    }

    const auto enumIt = enumConstants.find(mangled);
    if (enumIt != enumConstants.end()) {
        lastValue = builder->getInt32(static_cast<uint32_t>(enumIt->second));
        lastTypeName = "i32";
        lastIsString = false;
        return;
    }

    if (const VariableInfo* info = findVariable(mangled)) {
        lastValue = builder->CreateLoad(
            info->allocation->getAllocatedType(),
            info->allocation,
            mangled
        );
        lastTypeName = info->typeName;
        lastIsString = info->isString || isStringTypeName(info->typeName);
        return;
    }

    if (llvm::Function* fn = module->getFunction(mangled)) {
        lastValue = fn;
        lastTypeName = "*void";
        lastIsString = false;
        return;
    }

    throw std::runtime_error("Unknown scoped symbol: " + mangled);
}

void CodeGen::visit(FieldAccessASTNode& node) {
    std::string fieldTypeName;
    bool fieldIsString = false;
    llvm::Value* fieldPtr = getAddressOfExpression(&node, &fieldTypeName, nullptr, &fieldIsString);
    llvm::Type* fieldTy = getTypeFromName(fieldTypeName);
    lastValue = builder->CreateLoad(fieldTy, fieldPtr, node.field);
    lastTypeName = fieldTypeName;
    lastIsString = fieldIsString || isStringTypeName(fieldTypeName);
}

void CodeGen::visit(IndexASTNode& node) {
    std::string elemTypeName;
    bool elemIsString = false;
    llvm::Value* elemPtr = getAddressOfExpression(&node, &elemTypeName, nullptr, &elemIsString);
    llvm::Type* elemTy = getTypeFromName(elemTypeName);
    lastValue = builder->CreateLoad(elemTy, elemPtr, "idx.val");
    lastTypeName = elemTypeName;
    lastIsString = elemIsString || isStringTypeName(elemTypeName);
}

void CodeGen::visit(TryASTNode& node) {
    std::string opTypeName;
    bool opIsString = false;
    llvm::Value* val = generateExpression(node.operand.get(), &opTypeName, &opIsString);

    if (val->getType()->isPointerTy()) {
        llvm::Function* fn = builder->GetInsertBlock()->getParent();
        llvm::Value* isNull = builder->CreateIsNull(val, "try.isnull");
        llvm::BasicBlock* nullBlock = llvm::BasicBlock::Create(*context, "try.null", fn);
        llvm::BasicBlock* okBlock = llvm::BasicBlock::Create(*context, "try.ok", fn);

        builder->CreateCondBr(isNull, nullBlock, okBlock);

        builder->SetInsertPoint(nullBlock);
        if (fn->getReturnType()->isVoidTy()) {
            builder->CreateRetVoid();
        } else if (fn->getReturnType()->isPointerTy()) {
            builder->CreateRet(llvm::ConstantPointerNull::get(builder->getPtrTy()));
        } else if (fn->getReturnType()->isIntegerTy()) {
            builder->CreateRet(llvm::ConstantInt::get(fn->getReturnType(), 0));
        } else if (fn->getReturnType()->isFloatingPointTy()) {
            builder->CreateRet(llvm::ConstantFP::get(fn->getReturnType(), 0.0));
        } else {
            builder->CreateUnreachable();
        }

        builder->SetInsertPoint(okBlock);
    }

    lastValue = val;
    lastTypeName = opTypeName;
    lastIsString = opIsString;
}

void CodeGen::visit(FunctionCallASTNode& node) {
    std::string targetName = node.name;
    if (targetName.empty() && node.callee) {
        if (auto* sc = dynamic_cast<ScopeResolutionASTNode*>(node.callee.get())) {
            for (std::size_t i = 0; i < sc->path.size(); ++i) {
                if (i > 0) {
                    targetName += "__";
                }
                targetName += sc->path[i];
            }
        }
    }

    targetName = resolveImportedOrPrefixedName(targetName);

    llvm::Function* function = module->getFunction(targetName);
    if (!function && !node.name.empty()) {
        function = module->getFunction(node.name);
        if (function) {
            targetName = node.name;
        }
    }
    if (!function) {
        throw std::runtime_error("Unknown function: " + targetName);
    }

    if (!function->isVarArg() && function->arg_size() != node.arguments.size()) {
        throw std::runtime_error(
            "Incorrect number of arguments in function call: " + targetName
        );
    }

    const auto fnIt = functionTable.find(targetName);
    const FunctionInfo* fnInfo =
        (fnIt != functionTable.end()) ? &fnIt->second : nullptr;

    std::vector<llvm::Value*> arguments;
    arguments.reserve(node.arguments.size());

    for (std::size_t i = 0; i < node.arguments.size(); ++i) {
        llvm::Value* argument = generateExpression(node.arguments[i].get());
        if (i < function->arg_size()) {
            llvm::Type* expectedType = function->getFunctionType()->getParamType(i);
            const std::string expectedParamTypeName =
                (fnInfo && i < fnInfo->paramTypeNames.size() && !fnInfo->paramTypeNames[i].empty())
                    ? fnInfo->paramTypeNames[i]
                    : inferTypeNameFromLLVMType(expectedType);

            if (argument->getType() != expectedType) {
                argument = castValueToType(argument, expectedParamTypeName, expectedType);
            }
        }
        arguments.push_back(argument);
    }

    if (function->getReturnType()->isVoidTy()) {
        builder->CreateCall(function, arguments);
        lastValue = llvm::ConstantInt::get(builder->getInt64Ty(), 0);
        lastTypeName = "i64";
        lastIsString = false;
        return;
    }

    lastValue = builder->CreateCall(function, arguments, "calltmp");
    lastTypeName = (fnInfo && !fnInfo->returnTypeName.empty())
        ? fnInfo->returnTypeName
        : inferTypeNameFromLLVMType(lastValue->getType(), false, fnInfo && fnInfo->returnIsString);
    lastIsString = (fnInfo && fnInfo->returnIsString) || isStringTypeName(lastTypeName);
}

void CodeGen::visit(UnaryASTNode& node) {
    if (node.op == "&") {
        std::string addrType;
        bool addrIsString = false;
        lastValue = getAddressOfExpression(node.operand.get(), &addrType, nullptr, &addrIsString);
        lastTypeName = addrIsString ? "string" : ("*" + addrType);
        lastIsString = addrIsString;
        return;
    }

    if (node.op == "*") {
        std::string operandTypeName;
        bool operandIsString = false;
        llvm::Value* ptrVal = generateExpression(
            node.operand.get(),
            &operandTypeName,
            &operandIsString
        );
        if (!ptrVal->getType()->isPointerTy()) {
            throw std::runtime_error("Dereference '*' requires a pointer operand");
        }
        std::string pointeeTypeName = operandIsString
            ? std::string("char")
            : inferPointeeTypeName(operandTypeName);
        if (auto* varExpr = dynamic_cast<VariableASTNode*>(node.operand.get())) {
            if (const VariableInfo* vInfo = findVariable(varExpr->name)) {
                if (!vInfo->pointeeTypeName.empty()) {
                    pointeeTypeName = vInfo->pointeeTypeName;
                }
            }
        }
        llvm::Type* pointeeTy = getTypeFromName(pointeeTypeName);
        if (pointeeTy->isVoidTy()) {
            pointeeTy = builder->getInt8Ty();
            pointeeTypeName = "i8";
        }
        lastValue = builder->CreateLoad(pointeeTy, ptrVal, "dereftmp");
        lastTypeName = pointeeTypeName;
        lastIsString = isStringTypeName(pointeeTypeName);
        return;
    }

    std::string operandTypeName;
    bool operandIsString = false;
    llvm::Value* operand = generateExpression(
        node.operand.get(),
        &operandTypeName,
        &operandIsString
    );

    if (node.op == "not") {
        if (!operand->getType()->isIntegerTy(1)) {
            throw std::runtime_error(
                "'not' requires a comparison or logical expression"
            );
        }
        lastValue = builder->CreateNot(operand, "nottmp");
        lastTypeName = "bool";
        lastIsString = false;
        return;
    }

    if (node.op == "~") {
        if (!operand->getType()->isIntegerTy()) {
            throw std::runtime_error("Bitwise '~' requires an integer operand");
        }
        lastValue = builder->CreateNot(operand, "bitnottmp");
        lastTypeName = operandTypeName;
        lastIsString = false;
        return;
    }

    if (node.op == "-") {
        if (operand->getType()->isFloatingPointTy()) {
            lastValue = builder->CreateFNeg(operand, "fnegtmp");
            lastTypeName = operandTypeName;
            lastIsString = false;
            return;
        }
        if (operand->getType()->isIntegerTy()) {
            lastValue = builder->CreateNeg(operand, "negtmp");
            lastTypeName = operandTypeName;
            lastIsString = false;
            return;
        }
        throw std::runtime_error("Unary '-' requires a numeric operand");
    }

    if (node.op == "+") {
        if (operand->getType()->isFloatingPointTy() || operand->getType()->isIntegerTy()) {
            lastValue = operand;
            lastTypeName = operandTypeName;
            lastIsString = false;
            return;
        }
        throw std::runtime_error("Unary '+' requires a numeric operand");
    }

    throw std::runtime_error("Unknown unary operator: " + node.op);
}

void CodeGen::visit(CastASTNode& node) {
    llvm::Value* val = generateExpression(node.expression.get());
    const std::string targetName = resolveTypeName(node.targetType, "i64");
    llvm::Type* targetTy = getTypeFromSpec(node.targetType, "i64");
    lastValue = castValueToType(val, targetName, targetTy);
    lastTypeName = targetName;
    lastIsString = isStringTypeName(targetName);
}

void CodeGen::visit(SizeOfASTNode& node) {
    llvm::Type* targetTy = nullptr;
    if (node.isTypeTarget) {
        targetTy = getTypeFromSpec(node.targetType, "i64");
    } else if (node.targetExpr) {
        llvm::Value* v = generateExpression(node.targetExpr.get());
        targetTy = v->getType();
    }
    if (!targetTy) {
        throw std::runtime_error("Invalid sizeof target");
    }
    const llvm::DataLayout dl(module.get());
    const uint64_t sizeBytes = dl.getTypeAllocSize(targetTy);
    lastValue = builder->getInt64(sizeBytes);
    lastTypeName = "u64";
    lastIsString = false;
}

void CodeGen::visit(BinaryASTNode& node) {
    std::string leftTypeName;
    std::string rightTypeName;
    llvm::Value* leftVal = generateExpression(node.left.get(), &leftTypeName);
    llvm::Value* rightVal = generateExpression(node.right.get(), &rightTypeName);

    const bool isFloat = leftVal->getType()->isFloatingPointTy() ||
                         rightVal->getType()->isFloatingPointTy();

    if (isFloat) {
        if (leftVal->getType() != rightVal->getType()) {
            llvm::Type* targetFP =
                (leftVal->getType()->isDoubleTy() || rightVal->getType()->isDoubleTy())
                    ? builder->getDoubleTy()
                    : builder->getFloatTy();
            const std::string targetFPName = targetFP->isDoubleTy() ? "f64" : "f32";
            leftVal = castValueToType(leftVal, targetFPName, targetFP);
            rightVal = castValueToType(rightVal, targetFPName, targetFP);
        }

        lastIsString = false;
        if (node.op == "+") {
            lastValue = builder->CreateFAdd(leftVal, rightVal, "faddtmp");
            lastTypeName = leftVal->getType()->isDoubleTy() ? "f64" : "f32";
            return;
        }
        if (node.op == "-") {
            lastValue = builder->CreateFSub(leftVal, rightVal, "fsubtmp");
            lastTypeName = leftVal->getType()->isDoubleTy() ? "f64" : "f32";
            return;
        }
        if (node.op == "*") {
            lastValue = builder->CreateFMul(leftVal, rightVal, "fmultmp");
            lastTypeName = leftVal->getType()->isDoubleTy() ? "f64" : "f32";
            return;
        }
        if (node.op == "/") {
            lastValue = builder->CreateFDiv(leftVal, rightVal, "fdivtmp");
            lastTypeName = leftVal->getType()->isDoubleTy() ? "f64" : "f32";
            return;
        }
        if (node.op == ">") {
            lastValue = builder->CreateFCmpOGT(leftVal, rightVal, "fcmptmp");
            lastTypeName = "bool";
            return;
        }
        if (node.op == "<") {
            lastValue = builder->CreateFCmpOLT(leftVal, rightVal, "fcmptmp");
            lastTypeName = "bool";
            return;
        }
        if (node.op == ">=") {
            lastValue = builder->CreateFCmpOGE(leftVal, rightVal, "fcmptmp");
            lastTypeName = "bool";
            return;
        }
        if (node.op == "<=") {
            lastValue = builder->CreateFCmpOLE(leftVal, rightVal, "fcmptmp");
            lastTypeName = "bool";
            return;
        }
        if (node.op == "==") {
            lastValue = builder->CreateFCmpOEQ(leftVal, rightVal, "fcmptmp");
            lastTypeName = "bool";
            return;
        }
        if (node.op == "!=") {
            lastValue = builder->CreateFCmpONE(leftVal, rightVal, "fcmptmp");
            lastTypeName = "bool";
            return;
        }

        throw std::runtime_error(
            "Unsupported operator for floating-point values: " + node.op
        );
    }

    lastIsString = false;
    if (node.op == "and") {
        if (!leftVal->getType()->isIntegerTy(1) || !rightVal->getType()->isIntegerTy(1)) {
            throw std::runtime_error(
                "'and' requires comparison or logical expressions"
            );
        }
        lastValue = builder->CreateAnd(leftVal, rightVal, "andtmp");
        lastTypeName = "bool";
        return;
    }

    if (node.op == "or") {
        if (!leftVal->getType()->isIntegerTy(1) || !rightVal->getType()->isIntegerTy(1)) {
            throw std::runtime_error(
                "'or' requires comparison or logical expressions"
            );
        }
        lastValue = builder->CreateOr(leftVal, rightVal, "ortmp");
        lastTypeName = "bool";
        return;
    }

    if (node.op == "<<" || node.op == ">>") {
        if (!leftVal->getType()->isIntegerTy() || !rightVal->getType()->isIntegerTy()) {
            throw std::runtime_error("Shift operators require integer operands");
        }

        llvm::Value* shiftAmount = builder->CreateZExtOrTrunc(
            rightVal,
            leftVal->getType(),
            "shiftamount"
        );

        if (node.op == "<<") {
            lastValue = builder->CreateShl(leftVal, shiftAmount, "shltmp");
            lastTypeName = leftTypeName;
            return;
        }

        if (isUnsignedIntTypeName(leftTypeName)) {
            lastValue = builder->CreateLShr(leftVal, shiftAmount, "lshrtmp");
            lastTypeName = leftTypeName;
            return;
        }
        lastValue = builder->CreateAShr(leftVal, shiftAmount, "ashrtmp");
        lastTypeName = leftTypeName;
        return;
    }

    std::string promotedTypeName = leftTypeName;
    if (leftVal->getType() != rightVal->getType()) {
        const unsigned leftBits = leftVal->getType()->getIntegerBitWidth();
        const unsigned rightBits = rightVal->getType()->getIntegerBitWidth();
        if (leftBits >= rightBits) {
            promotedTypeName = leftTypeName;
            rightVal = castValueToType(rightVal, leftTypeName, leftVal->getType());
        } else {
            promotedTypeName = rightTypeName;
            leftVal = castValueToType(leftVal, rightTypeName, rightVal->getType());
        }
    } else if (isUnsignedIntTypeName(leftTypeName) != isUnsignedIntTypeName(rightTypeName)) {
        promotedTypeName = isUnsignedIntTypeName(leftTypeName) ? leftTypeName : rightTypeName;
    }

    const bool isUnsigned = isUnsignedIntTypeName(promotedTypeName);

    if (node.op == "+") {
        lastValue = builder->CreateAdd(leftVal, rightVal, "addtmp");
        lastTypeName = promotedTypeName;
        return;
    }
    if (node.op == "-") {
        lastValue = builder->CreateSub(leftVal, rightVal, "subtmp");
        lastTypeName = promotedTypeName;
        return;
    }
    if (node.op == "*") {
        lastValue = builder->CreateMul(leftVal, rightVal, "multmp");
        lastTypeName = promotedTypeName;
        return;
    }
    if (node.op == "/") {
        lastValue = isUnsigned
            ? builder->CreateUDiv(leftVal, rightVal, "udivtmp")
            : builder->CreateSDiv(leftVal, rightVal, "sdivtmp");
        lastTypeName = promotedTypeName;
        return;
    }
    if (node.op == "%") {
        lastValue = isUnsigned
            ? builder->CreateURem(leftVal, rightVal, "uremtmp")
            : builder->CreateSRem(leftVal, rightVal, "sremtmp");
        lastTypeName = promotedTypeName;
        return;
    }
    if (node.op == ">") {
        lastValue = isUnsigned
            ? builder->CreateICmpUGT(leftVal, rightVal, "cmptmp")
            : builder->CreateICmpSGT(leftVal, rightVal, "cmptmp");
        lastTypeName = "bool";
        return;
    }
    if (node.op == "<") {
        lastValue = isUnsigned
            ? builder->CreateICmpULT(leftVal, rightVal, "cmptmp")
            : builder->CreateICmpSLT(leftVal, rightVal, "cmptmp");
        lastTypeName = "bool";
        return;
    }
    if (node.op == ">=") {
        lastValue = isUnsigned
            ? builder->CreateICmpUGE(leftVal, rightVal, "cmptmp")
            : builder->CreateICmpSGE(leftVal, rightVal, "cmptmp");
        lastTypeName = "bool";
        return;
    }
    if (node.op == "<=") {
        lastValue = isUnsigned
            ? builder->CreateICmpULE(leftVal, rightVal, "cmptmp")
            : builder->CreateICmpSLE(leftVal, rightVal, "cmptmp");
        lastTypeName = "bool";
        return;
    }
    if (node.op == "==") {
        lastValue = builder->CreateICmpEQ(leftVal, rightVal, "cmptmp");
        lastTypeName = "bool";
        return;
    }
    if (node.op == "!=") {
        lastValue = builder->CreateICmpNE(leftVal, rightVal, "cmptmp");
        lastTypeName = "bool";
        return;
    }
    if (node.op == "&") {
        lastValue = builder->CreateAnd(leftVal, rightVal, "bitandtmp");
        lastTypeName = promotedTypeName;
        return;
    }
    if (node.op == "|") {
        lastValue = builder->CreateOr(leftVal, rightVal, "bitortmp");
        lastTypeName = promotedTypeName;
        return;
    }
    if (node.op == "^") {
        lastValue = builder->CreateXor(leftVal, rightVal, "bitxortmp");
        lastTypeName = promotedTypeName;
        return;
    }

    throw std::runtime_error("Unknown binary operator: " + node.op);
}
void CodeGen::visit(PrintASTNode& node) {
    ASTNode* expression = node.expression.get();
    std::string exprTypeName;
    bool exprIsString = false;
    llvm::Value* value = generateExpression(expression, &exprTypeName, &exprIsString);

    if (dynamic_cast<StringASTNode*>(expression) ||
        exprIsString ||
        isStringTypeName(exprTypeName)) {
        llvm::Function* fn = builder->GetInsertBlock()->getParent();
        llvm::Value* isNull = builder->CreateIsNull(value, "str.isnull");
        llvm::BasicBlock* nullStrBlock = llvm::BasicBlock::Create(*context, "str.null", fn);
        llvm::BasicBlock* nonNullStrBlock = llvm::BasicBlock::Create(*context, "str.nonnull", fn);
        llvm::BasicBlock* afterStrBlock = llvm::BasicBlock::Create(*context, "str.after", fn);

        builder->CreateCondBr(isNull, nullStrBlock, nonNullStrBlock);

        builder->SetInsertPoint(nullStrBlock);
        builder->CreateCall(printfFunction, {stringFormat, nullString});
        builder->CreateBr(afterStrBlock);

        builder->SetInsertPoint(nonNullStrBlock);
        builder->CreateCall(printfFunction, {stringFormat, value});
        builder->CreateBr(afterStrBlock);

        builder->SetInsertPoint(afterStrBlock);
    } else if (value->getType()->isPointerTy()) {
        builder->CreateCall(printfFunction, {ptrFormat, value});
    } else if (value->getType()->isIntegerTy(1)) {
        llvm::Value* selectedString = builder->CreateSelect(
            value,
            trueString,
            falseString,
            "boolString"
        );
        builder->CreateCall(printfFunction, {stringFormat, selectedString});
    } else if (value->getType()->isFloatingPointTy()) {
        llvm::Value* doubleVal = value->getType()->isDoubleTy()
            ? value
            : builder->CreateFPExt(value, builder->getDoubleTy(), "fpexttmp");
        builder->CreateCall(printfFunction, {floatFormat, doubleVal});
    } else if (value->getType()->isIntegerTy()) {
        bool isUnsigned = isUnsignedIntTypeName(exprTypeName);
        if (!isUnsigned) {
            if (auto* varExpr = dynamic_cast<VariableASTNode*>(expression)) {
                const VariableInfo* info = findVariable(varExpr->name);
                if (info && !info->typeName.empty()) {
                    isUnsigned = isUnsignedIntTypeName(info->typeName);
                }
            } else if (auto* callExpr = dynamic_cast<FunctionCallASTNode*>(expression)) {
                const auto it = functionTable.find(callExpr->name);
                if (it != functionTable.end() && !it->second.returnTypeName.empty()) {
                    isUnsigned = isUnsignedIntTypeName(it->second.returnTypeName);
                }
            }
        }

        const unsigned bits = value->getType()->getIntegerBitWidth();
        if (bits > 32) {
            llvm::Value* ext64 = isUnsigned
                ? builder->CreateZExtOrTrunc(value, builder->getInt64Ty(), "u64ext")
                : builder->CreateSExtOrTrunc(value, builder->getInt64Ty(), "i64ext");
            builder->CreateCall(printfFunction, {isUnsigned ? u64Format : i64Format, ext64});
        } else if (isUnsigned) {
            llvm::Value* ext32 = builder->CreateZExtOrTrunc(value, builder->getInt32Ty(), "u32ext");
            builder->CreateCall(printfFunction, {uintFormat, ext32});
        } else {
            llvm::Value* ext32 = builder->CreateSExtOrTrunc(value, builder->getInt32Ty(), "i32ext");
            builder->CreateCall(printfFunction, {intFormat, ext32});
        }
    } else {
        throw std::runtime_error("Unsupported type in print statement: " + exprTypeName);
    }
}

void CodeGen::visit(AssignmentASTNode& node) {
    std::string rhsTypeName;
    bool rhsIsString = false;
    llvm::Value* rhsVal = generateExpression(node.value.get(), &rhsTypeName, &rhsIsString);

    std::string targetTypeName;
    bool targetIsString = false;
    llvm::Value* targetPtr = nullptr;
    llvm::Type* targetElemType = nullptr;

    if (node.target) {
        targetPtr = getAddressOfExpression(node.target.get(), &targetTypeName, nullptr, &targetIsString);
        targetElemType = getTypeFromName(targetTypeName);
    } else {
        VariableInfo* info = findVariable(node.name);
        if (!info || !info->allocation) {
            throw std::runtime_error("Assignment to undeclared variable: " + node.name);
        }
        targetPtr = info->allocation;
        targetTypeName = info->typeName;
        targetIsString = info->isString;
        targetElemType = info->allocation->getAllocatedType();
    }

    llvm::Value* convertedRhs = !targetTypeName.empty()
        ? castValueToType(rhsVal, targetTypeName, targetElemType)
        : rhsVal;

    if (node.op == "=") {
        builder->CreateStore(convertedRhs, targetPtr);
        lastValue = convertedRhs;
        lastTypeName = targetTypeName;
        lastIsString = targetIsString;
        return;
    }

    llvm::Value* curVal = builder->CreateLoad(targetElemType, targetPtr, "compound.cur");
    llvm::Value* updated = nullptr;
    const bool isFloat = targetElemType->isFloatingPointTy();
    const bool isUnsigned = isUnsignedIntTypeName(targetTypeName);

    if (node.op == "+=") {
        updated = isFloat
            ? builder->CreateFAdd(curVal, convertedRhs, "faddassign")
            : builder->CreateAdd(curVal, convertedRhs, "addassign");
    } else if (node.op == "-=") {
        updated = isFloat
            ? builder->CreateFSub(curVal, convertedRhs, "fsubassign")
            : builder->CreateSub(curVal, convertedRhs, "subassign");
    } else if (node.op == "*=") {
        updated = isFloat
            ? builder->CreateFMul(curVal, convertedRhs, "fmulassign")
            : builder->CreateMul(curVal, convertedRhs, "mulassign");
    } else if (node.op == "/=") {
        if (isFloat) {
            updated = builder->CreateFDiv(curVal, convertedRhs, "fdivassign");
        } else {
            updated = isUnsigned
                ? builder->CreateUDiv(curVal, convertedRhs, "udivassign")
                : builder->CreateSDiv(curVal, convertedRhs, "sdivassign");
        }
    } else if (node.op == "%=") {
        updated = isUnsigned
            ? builder->CreateURem(curVal, convertedRhs, "uremassign")
            : builder->CreateSRem(curVal, convertedRhs, "sremassign");
    } else if (node.op == "&=") {
        updated = builder->CreateAnd(curVal, convertedRhs, "andassign");
    } else if (node.op == "|=") {
        updated = builder->CreateOr(curVal, convertedRhs, "orassign");
    } else if (node.op == "^=") {
        updated = builder->CreateXor(curVal, convertedRhs, "xorassign");
    } else if (node.op == "<<=") {
        llvm::Value* shiftAmt = builder->CreateZExtOrTrunc(convertedRhs, targetElemType, "shlamt");
        updated = builder->CreateShl(curVal, shiftAmt, "shlassign");
    } else if (node.op == ">>=") {
        llvm::Value* shiftAmt = builder->CreateZExtOrTrunc(convertedRhs, targetElemType, "shramt");
        updated = isUnsigned
            ? builder->CreateLShr(curVal, shiftAmt, "lshrassign")
            : builder->CreateAShr(curVal, shiftAmt, "ashrassign");
    } else {
        throw std::runtime_error("Unsupported assignment operator: " + node.op);
    }

    builder->CreateStore(updated, targetPtr);
    lastValue = updated;
    lastTypeName = targetTypeName;
    lastIsString = targetIsString;
}

void CodeGen::visit(ModuleDeclASTNode& node) {
    std::string modName;
    for (std::size_t i = 0; i < node.path.size(); ++i) {
        if (i > 0) {
            modName += "__";
        }
        modName += node.path[i];
    }
    currentModulePrefix = modName;
}

void CodeGen::visit(ImportDeclASTNode& node) {
    if (node.path.empty()) {
        return;
    }

    std::string sourcePrefix;
    for (std::size_t i = 0; i < node.path.size(); ++i) {
        if (i > 0) {
            sourcePrefix += "__";
        }
        sourcePrefix += node.path[i];
    }

    const std::string& leaf = node.path.back();
    importedAliases[leaf] = sourcePrefix;
    importedSymbols.insert(leaf);
    importedSymbols.insert(sourcePrefix);
}

void CodeGen::visit(ParameterASTNode&) {}

void CodeGen::visit(StructDeclASTNode& node) {
    const std::string structName = applyModulePrefix(node.name);
    std::vector<llvm::Type*> fieldTypes;
    StructInfo info;
    fieldTypes.reserve(node.fields.size());
    info.fieldTypeNames.reserve(node.fields.size());
    info.isUnion = false;

    for (unsigned i = 0; i < static_cast<unsigned>(node.fields.size()); ++i) {
        const auto& f = node.fields[i];
        const std::string fTypeName = resolveTypeName(f.type, "i64");
        fieldTypes.push_back(getTypeFromSpec(f.type, "i64"));
        info.fieldIndices[f.name] = i;
        info.fieldTypeNames.push_back(fTypeName);
    }

    llvm::StructType* stTy = llvm::StructType::create(*context, fieldTypes, structName);
    info.llvmType = stTy;
    structTable[structName] = std::move(info);
    if (structName != node.name && !structTable.count(node.name)) {
        structTable[node.name] = structTable[structName];
    }
}

void CodeGen::visit(UnionDeclASTNode& node) {
    if (node.fields.empty()) {
        return;
    }

    const std::string unionName = applyModulePrefix(node.name);
    const llvm::DataLayout dl(module.get());
    uint64_t maxSize = 0;
    unsigned maxAlign = 1;
    llvm::Type* bestType = nullptr;
    std::string bestTypeName = "i64";

    for (const auto& f : node.fields) {
        llvm::Type* fTy = getTypeFromSpec(f.type, "i64");
        const uint64_t sz = dl.getTypeAllocSize(fTy);
        const unsigned al = dl.getABITypeAlign(fTy).value();
        if (sz > maxSize || (sz == maxSize && al > maxAlign)) {
            maxSize = sz;
            maxAlign = al;
            bestType = fTy;
            bestTypeName = resolveTypeName(f.type, "i64");
        }
    }

    if (!bestType) {
        bestType = builder->getInt64Ty();
    }

    StructInfo info;
    llvm::StructType* stTy = llvm::StructType::create(*context, {bestType}, unionName);
    info.llvmType = stTy;
    info.isUnion = true;
    for (const auto& f : node.fields) {
        info.fieldIndices[f.name] = 0;
        info.fieldTypeNames.push_back(resolveTypeName(f.type, bestTypeName));
    }
    structTable[unionName] = std::move(info);
    if (unionName != node.name && !structTable.count(node.name)) {
        structTable[node.name] = structTable[unionName];
    }
}

void CodeGen::visit(EnumDeclASTNode& node) {
    const std::string enumName = applyModulePrefix(node.name);
    for (std::size_t i = 0; i < node.variants.size(); ++i) {
        const std::string qualified = enumName + "__" + node.variants[i];
        enumConstants[qualified] = static_cast<long long>(i);
        if (enumName != node.name) {
            enumConstants[node.name + "__" + node.variants[i]] = static_cast<long long>(i);
        }
    }
}

void CodeGen::visit(WildcardPatternASTNode&) {
    lastValue = builder->getInt1(true);
    lastTypeName = "bool";
    lastIsString = false;
}

void CodeGen::visit(LiteralPatternASTNode& node) {
    if (!node.literal || !currentMatchScrutinee) {
        lastValue = builder->getInt1(false);
        lastTypeName = "bool";
        lastIsString = false;
        return;
    }

    llvm::Value* patVal = generateExpression(node.literal.get());
    llvm::Type* scrTy = currentMatchScrutinee->getType();

    if (scrTy != patVal->getType()) {
        if (scrTy->isIntegerTy() && patVal->getType()->isIntegerTy()) {
            patVal = castValueToType(patVal, currentMatchScrutineeTypeName, scrTy);
        } else if (scrTy->isFloatingPointTy() && patVal->getType()->isFloatingPointTy()) {
            patVal = castValueToType(patVal, currentMatchScrutineeTypeName, scrTy);
        } else if (scrTy->isPointerTy() && patVal->getType()->isPointerTy()) {
            patVal = builder->CreatePointerCast(patVal, scrTy, "pat.ptrcast");
        } else {
            lastValue = builder->getInt1(false);
            lastTypeName = "bool";
            lastIsString = false;
            return;
        }
    }

    if (scrTy->isFloatingPointTy()) {
        lastValue = builder->CreateFCmpOEQ(currentMatchScrutinee, patVal, "pat.fcmp");
    } else {
        lastValue = builder->CreateICmpEQ(currentMatchScrutinee, patVal, "pat.icmp");
    }
    lastTypeName = "bool";
    lastIsString = false;
}

void CodeGen::visit(IdentifierPatternASTNode&) {
    lastValue = builder->getInt1(true);
    lastTypeName = "bool";
    lastIsString = false;
}

void CodeGen::visit(PathPatternASTNode& node) {
    if (!currentMatchScrutinee || node.path.empty()) {
        lastValue = builder->getInt1(false);
        lastTypeName = "bool";
        lastIsString = false;
        return;
    }

    std::string mangled;
    for (std::size_t i = 0; i < node.path.size(); ++i) {
        if (i > 0) {
            mangled += "__";
        }
        mangled += node.path[i];
    }

    const auto enumIt = enumConstants.find(mangled);
    if (enumIt != enumConstants.end() && currentMatchScrutinee->getType()->isIntegerTy()) {
        llvm::Value* enumVal = llvm::ConstantInt::get(
            currentMatchScrutinee->getType(),
            static_cast<uint64_t>(enumIt->second),
            true
        );
        lastValue = builder->CreateICmpEQ(currentMatchScrutinee, enumVal, "pat.enumcmp");
        lastTypeName = "bool";
        lastIsString = false;
        return;
    }

    lastValue = builder->getInt1(false);
    lastTypeName = "bool";
    lastIsString = false;
}

void CodeGen::visit(MatchArmASTNode& node) {
    if (node.hasBlockBody) {
        for (const auto& stmt : node.bodyStatements) {
            generateStatement(stmt.get());
            if (builder->GetInsertBlock()->getTerminator()) {
                break;
            }
        }
    } else if (node.bodyExpr) {
        std::string exprType;
        bool exprStr = false;
        llvm::Value* val = generateExpression(node.bodyExpr.get(), &exprType, &exprStr);
        lastValue = val;
        lastTypeName = exprType;
        lastIsString = exprStr;
    }
}

void CodeGen::visit(MatchASTNode& node) {
    std::string scrTypeName;
    llvm::Value* scrutineeVal = generateExpression(node.scrutinee.get(), &scrTypeName);
    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* doneBlock = llvm::BasicBlock::Create(*context, "match.done", fn);

    llvm::AllocaInst* resultSlot = nullptr;
    std::string resultTypeName = "i64";
    bool resultIsString = false;
    bool hasAnyResult = false;

    auto emitArmBodyWithBindings = [&](const MatchArmASTNode& arm) {
        pushScope();
        if (auto* idPat = dynamic_cast<IdentifierPatternASTNode*>(arm.pattern.get())) {
            const std::string pTypeName = !scrTypeName.empty()
                ? scrTypeName
                : inferTypeNameFromLLVMType(scrutineeVal->getType());
            llvm::AllocaInst* alloc = builder->CreateAlloca(
                scrutineeVal->getType(),
                nullptr,
                idPat->name
            );
            builder->CreateStore(scrutineeVal, alloc);
            declareInCurrentScope(
                idPat->name,
                VariableInfo{alloc, pTypeName, inferPointeeTypeName(pTypeName), isStringTypeName(pTypeName)}
            );
        }
        const_cast<MatchArmASTNode&>(arm).accept(*this);
        if (!arm.hasBlockBody && lastValue && !lastValue->getType()->isVoidTy()) {
            if (!resultSlot) {
                const auto savedIP = builder->saveIP();
                builder->SetInsertPoint(&fn->getEntryBlock(), fn->getEntryBlock().begin());
                resultSlot = builder->CreateAlloca(lastValue->getType(), nullptr, "match.res");
                builder->restoreIP(savedIP);
                resultTypeName = lastTypeName;
                resultIsString = lastIsString;
            }
            llvm::Value* storedVal = castValueToType(lastValue, resultTypeName, resultSlot->getAllocatedType());
            builder->CreateStore(storedVal, resultSlot);
            hasAnyResult = true;
        }
        popScope();
    };

    for (std::size_t i = 0; i < node.arms.size(); ++i) {
        const auto& arm = node.arms[i];
        const bool isIrrefutable =
            dynamic_cast<WildcardPatternASTNode*>(arm->pattern.get()) != nullptr ||
            dynamic_cast<IdentifierPatternASTNode*>(arm->pattern.get()) != nullptr;

        llvm::BasicBlock* armBodyBlock = llvm::BasicBlock::Create(
            *context,
            "match.arm." + std::to_string(i),
            fn
        );

        if (isIrrefutable) {
            builder->CreateBr(armBodyBlock);
            builder->SetInsertPoint(armBodyBlock);
            emitArmBodyWithBindings(*arm);
            if (!builder->GetInsertBlock()->getTerminator()) {
                builder->CreateBr(doneBlock);
            }
            break;
        }

        llvm::BasicBlock* nextArmBlock = (i + 1 < node.arms.size())
            ? llvm::BasicBlock::Create(*context, "match.next." + std::to_string(i), fn)
            : doneBlock;

        currentMatchScrutinee = scrutineeVal;
        currentMatchScrutineeTypeName = scrTypeName;
        lastValue = nullptr;

        if (arm->pattern) {
            arm->pattern->accept(*this);
        }

        llvm::Value* cond = (lastValue && lastValue->getType()->isIntegerTy(1))
            ? lastValue
            : builder->getInt1(false);
        builder->CreateCondBr(cond, armBodyBlock, nextArmBlock);

        builder->SetInsertPoint(armBodyBlock);
        emitArmBodyWithBindings(*arm);

        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(doneBlock);
        }

        builder->SetInsertPoint(nextArmBlock);
    }

    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(doneBlock);
    }

    builder->SetInsertPoint(doneBlock);
    currentMatchScrutinee = nullptr;
    currentMatchScrutineeTypeName.clear();

    if (hasAnyResult && resultSlot) {
        lastValue = builder->CreateLoad(resultSlot->getAllocatedType(), resultSlot, "match.val");
        lastTypeName = resultTypeName;
        lastIsString = resultIsString;
    } else {
        lastValue = builder->getInt64(0);
        lastTypeName = "i64";
        lastIsString = false;
    }
}

void CodeGen::visit(BreakASTNode&) {
    if (breakTargets.empty()) {
        throw std::runtime_error("'break' can only be used inside a loop");
    }
    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    builder->CreateBr(breakTargets.back());
    createDeadContinuationBlock(fn, "after.break");
}

void CodeGen::visit(ContinueASTNode&) {
    if (continueTargets.empty()) {
        throw std::runtime_error("'continue' can only be used inside a loop");
    }
    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    builder->CreateBr(continueTargets.back());
    createDeadContinuationBlock(fn, "after.continue");
}

void CodeGen::visit(VariableDeclarationASTNode& node) {
    const bool hasExplicitType =
        !node.type.empty() ||
        !node.typeSpec.name.empty() ||
        node.typeSpec.pointerDepth > 0 ||
        node.typeSpec.elementType != nullptr ||
        node.typeSpec.isFunctionType;

    const std::string varTypeName = resolveTypeName(node.typeSpec, node.type);
    llvm::Type* varType = getTypeFromSpec(node.typeSpec, node.type);
    const std::string effectiveVarName =
        (scopes.size() == 1 && !currentModulePrefix.empty())
            ? applyModulePrefix(node.name)
            : node.name;
    llvm::AllocaInst* alloca = builder->CreateAlloca(varType, nullptr, effectiveVarName);

    std::string pointeeName = inferPointeeTypeName(varTypeName);
    bool varIsString = isStringTypeName(varTypeName);

    if (node.value) {
        std::string rhsTypeName;
        bool rhsIsString = false;
        llvm::Value* value = generateExpression(node.value.get(), &rhsTypeName, &rhsIsString);
        if (!hasExplicitType && varType->isPointerTy() && !rhsTypeName.empty()) {
            pointeeName = inferPointeeTypeName(rhsTypeName);
        }
        if (isStringTypeName(varTypeName) || (!hasExplicitType && rhsIsString)) {
            varIsString = true;
        }
        value = castValueToType(value, varTypeName, varType);
        builder->CreateStore(value, alloca);
    }

    if (!declareInCurrentScope(node.name, VariableInfo{alloca, varTypeName, pointeeName, varIsString})) {
        throw std::runtime_error("Variable already declared: " + node.name);
    }
}

void CodeGen::visit(ReturnASTNode& node) {
    llvm::BasicBlock* currentBlock = builder->GetInsertBlock();
    llvm::Function* currentFunction = currentBlock->getParent();

    if (currentFunction->getReturnType()->isVoidTy()) {
        if (node.value) {
            (void)generateExpression(node.value.get());
        }
        builder->CreateRetVoid();
        createDeadContinuationBlock(currentFunction, "after.return");
        return;
    }

    llvm::Value* returnValue = node.value
        ? generateExpression(node.value.get())
        : llvm::ConstantInt::get(currentFunction->getReturnType(), 0);

    if (returnValue->getType() != currentFunction->getReturnType()) {
        const auto fnIt = functionTable.find(currentFunction->getName().str());
        const std::string targetRetName =
            (fnIt != functionTable.end() && !fnIt->second.returnTypeName.empty())
                ? fnIt->second.returnTypeName
                : inferTypeNameFromLLVMType(currentFunction->getReturnType());
        returnValue = castValueToType(
            returnValue,
            targetRetName,
            currentFunction->getReturnType()
        );
    }

    builder->CreateRet(returnValue);
    createDeadContinuationBlock(currentFunction, "after.return");
}

void CodeGen::predeclareFunction(const FunctionASTNode& fnNode, const std::string& effectiveName) {
    if (module->getFunction(effectiveName)) {
        throw std::runtime_error("Function already declared: " + effectiveName);
    }

    const std::string retName = resolveTypeName(fnNode.returnTypeSpec, fnNode.returnType);
    llvm::Type* returnType = getTypeFromSpec(fnNode.returnTypeSpec, fnNode.returnType);

    std::vector<llvm::Type*> parameterTypes;
    std::vector<std::string> paramTypeNames;
    parameterTypes.reserve(fnNode.parameters.size());
    paramTypeNames.reserve(fnNode.parameters.size());

    for (const auto& parameter : fnNode.parameters) {
        const std::string pName = resolveTypeName(parameter->typeSpec, parameter->type);
        parameterTypes.push_back(getTypeFromSpec(parameter->typeSpec, parameter->type));
        paramTypeNames.push_back(pName);
    }

    llvm::FunctionType* functionType = llvm::FunctionType::get(
        returnType,
        parameterTypes,
        false
    );

    llvm::Function::Create(
        functionType,
        llvm::Function::ExternalLinkage,
        effectiveName,
        module.get()
    );

    functionTable[effectiveName] = FunctionInfo{
        retName,
        std::move(paramTypeNames),
        returnType->isVoidTy(),
        isStringTypeName(retName)
    };
}

void CodeGen::emitFunctionBody(FunctionASTNode& fnNode, const std::string& effectiveName) {
    if (fnNode.isExtern) {
        return;
    }

    llvm::Function* function = module->getFunction(effectiveName);
    if (!function) {
        throw std::runtime_error("Function was not pre-declared: " + effectiveName);
    }

    const auto savedInsertPoint = builder->saveIP();
    const auto savedScopes = scopes;
    scopes.clear();
    pushScope();

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context, "entry", function);
    builder->SetInsertPoint(entry);

    std::size_t index = 0;
    for (auto& argument : function->args()) {
        const auto& parameter = fnNode.parameters[index];
        argument.setName(parameter->name);

        llvm::AllocaInst* alloca = builder->CreateAlloca(
            argument.getType(),
            nullptr,
            parameter->name
        );
        builder->CreateStore(&argument, alloca);

        const std::string pTypeName = resolveTypeName(parameter->typeSpec, parameter->type);
        declareInCurrentScope(
            parameter->name,
            VariableInfo{
                alloca,
                pTypeName,
                inferPointeeTypeName(pTypeName),
                isStringTypeName(pTypeName)
            }
        );
        ++index;
    }

    for (const auto& statement : fnNode.statements) {
        generateStatement(statement.get());
        if (builder->GetInsertBlock()->getTerminator()) {
            break;
        }
    }

    if (function->getReturnType()->isVoidTy()) {
        if (!hasLiveReturnInstruction(function)) {
            if (llvm::BasicBlock* curBB = builder->GetInsertBlock()) {
                if (!curBB->getTerminator()) {
                    builder->CreateRetVoid();
                }
            }
        }
        terminateUnfinishedBlocks(function);
    } else {
        if (!allReachablePathsReturn(function)) {
            terminateUnfinishedBlocks(function);
            popScope();
            scopes = savedScopes;
            if (savedInsertPoint.isSet()) {
                builder->restoreIP(savedInsertPoint);
            }
            throw std::runtime_error(
                "Not all control-flow paths return a value in function: " + effectiveName
            );
        }
        terminateUnfinishedBlocks(function);
    }

    popScope();
    scopes = savedScopes;

    if (savedInsertPoint.isSet()) {
        builder->restoreIP(savedInsertPoint);
    }
}

void CodeGen::visit(FunctionASTNode& node) {
    emitFunctionBody(node, applyModulePrefix(node.name));
}

void CodeGen::visit(ImplBlockASTNode& node) {
    const std::string target = applyModulePrefix(node.targetType);
    for (const auto& method : node.methods) {
        if (method) {
            emitFunctionBody(*method, target + "__" + method->name);
        }
    }
}

void CodeGen::visit(ForASTNode& node) {
    pushScope();
    generateStatement(node.initialization.get());

    llvm::Function* function = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* conditionBlock = llvm::BasicBlock::Create(
        *context,
        "for.condition",
        function
    );
    llvm::BasicBlock* bodyBlock = llvm::BasicBlock::Create(
        *context,
        "for.body",
        function
    );
    llvm::BasicBlock* updateBlock = llvm::BasicBlock::Create(
        *context,
        "for.update",
        function
    );
    llvm::BasicBlock* endBlock = llvm::BasicBlock::Create(
        *context,
        "for.end",
        function
    );

    breakTargets.push_back(endBlock);
    continueTargets.push_back(updateBlock);

    builder->CreateBr(conditionBlock);
    builder->SetInsertPoint(conditionBlock);

    llvm::Value* condition = generateExpression(node.condition.get());
    if (!condition->getType()->isIntegerTy(1)) {
        throw std::runtime_error(
            "The for condition must be a comparison or logical expression"
        );
    }

    builder->CreateCondBr(condition, bodyBlock, endBlock);

    builder->SetInsertPoint(bodyBlock);
    pushScope();
    for (const auto& statement : node.statements) {
        generateStatement(statement.get());
        if (builder->GetInsertBlock()->getTerminator()) {
            break;
        }
    }
    popScope();

    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(updateBlock);
    }

    builder->SetInsertPoint(updateBlock);
    generateStatement(node.update.get());

    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(conditionBlock);
    }

    builder->SetInsertPoint(endBlock);

    continueTargets.pop_back();
    breakTargets.pop_back();
    popScope();
}

void CodeGen::visit(WhileASTNode& node) {
    llvm::Function* function = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* conditionBlock = llvm::BasicBlock::Create(
        *context,
        "while.condition",
        function
    );
    llvm::BasicBlock* bodyBlock = llvm::BasicBlock::Create(
        *context,
        "while.body",
        function
    );
    llvm::BasicBlock* endBlock = llvm::BasicBlock::Create(
        *context,
        "while.end",
        function
    );

    breakTargets.push_back(endBlock);
    continueTargets.push_back(conditionBlock);

    builder->CreateBr(conditionBlock);
    builder->SetInsertPoint(conditionBlock);

    llvm::Value* condition = generateExpression(node.condition.get());
    if (!condition->getType()->isIntegerTy(1)) {
        throw std::runtime_error(
            "The while condition must be a comparison or logical expression"
        );
    }

    builder->CreateCondBr(condition, bodyBlock, endBlock);

    builder->SetInsertPoint(bodyBlock);
    pushScope();
    for (const auto& statement : node.statements) {
        generateStatement(statement.get());
        if (builder->GetInsertBlock()->getTerminator()) {
            break;
        }
    }
    popScope();

    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(conditionBlock);
    }

    builder->SetInsertPoint(endBlock);

    continueTargets.pop_back();
    breakTargets.pop_back();
}

void CodeGen::visit(IfASTNode& node) {
    llvm::Value* condition = generateExpression(node.condition.get());
    if (!condition->getType()->isIntegerTy(1)) {
        throw std::runtime_error(
            "The if condition must be a comparison or logical expression"
        );
    }

    llvm::Function* function = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* thenBlock = llvm::BasicBlock::Create(
        *context,
        "if.then",
        function
    );
    llvm::BasicBlock* elseBlock = llvm::BasicBlock::Create(
        *context,
        "if.else",
        function
    );
    llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(
        *context,
        "if.end",
        function
    );

    builder->CreateCondBr(condition, thenBlock, elseBlock);

    builder->SetInsertPoint(thenBlock);
    pushScope();
    for (const auto& statement : node.statements) {
        generateStatement(statement.get());
        if (builder->GetInsertBlock()->getTerminator()) {
            break;
        }
    }
    popScope();

    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(mergeBlock);
    }

    builder->SetInsertPoint(elseBlock);
    pushScope();
    for (const auto& statement : node.elseStatements) {
        generateStatement(statement.get());
        if (builder->GetInsertBlock()->getTerminator()) {
            break;
        }
    }
    popScope();

    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(mergeBlock);
    }

    builder->SetInsertPoint(mergeBlock);
}

void CodeGen::visit(ProgramASTNode& node) {
    if (module->getFunction("main")) {
        throw std::runtime_error("Code generation has already been performed");
    }

    currentModulePrefix.clear();
    importedAliases.clear();
    importedSymbols.clear();
    scopes.clear();
    pushScope();
    functionTable.clear();
    structTable.clear();
    enumConstants.clear();
    breakTargets.clear();
    continueTargets.clear();

    bool hasUserMain = false;
    bool hasTopLevelNonFunction = false;
    for (const auto& statement : node.statements) {
        if (auto* functionNode = dynamic_cast<FunctionASTNode*>(statement.get())) {
            if (functionNode->name == "main") {
                hasUserMain = true;
            }
        } else if (statement &&
                   !dynamic_cast<StructDeclASTNode*>(statement.get()) &&
                   !dynamic_cast<UnionDeclASTNode*>(statement.get()) &&
                   !dynamic_cast<EnumDeclASTNode*>(statement.get()) &&
                   !dynamic_cast<ImplBlockASTNode*>(statement.get()) &&
                   !dynamic_cast<ModuleDeclASTNode*>(statement.get()) &&
                   !dynamic_cast<ImportDeclASTNode*>(statement.get())) {
            hasTopLevelNonFunction = true;
        }
    }

    if (hasUserMain && hasTopLevelNonFunction) {
        throw std::runtime_error(
            "Top-level statements are not allowed when user defines main"
        );
    }

    for (const auto& statement : node.statements) {
        if (auto* modNode = dynamic_cast<ModuleDeclASTNode*>(statement.get())) {
            modNode->accept(*this);
        } else if (auto* impNode = dynamic_cast<ImportDeclASTNode*>(statement.get())) {
            impNode->accept(*this);
        } else if (auto* structNode = dynamic_cast<StructDeclASTNode*>(statement.get())) {
            structNode->accept(*this);
        } else if (auto* unionNode = dynamic_cast<UnionDeclASTNode*>(statement.get())) {
            unionNode->accept(*this);
        } else if (auto* enumNode = dynamic_cast<EnumDeclASTNode*>(statement.get())) {
            enumNode->accept(*this);
        }
    }

    currentModulePrefix.clear();

    for (const auto& statement : node.statements) {
        if (auto* modNode = dynamic_cast<ModuleDeclASTNode*>(statement.get())) {
            modNode->accept(*this);
        } else if (auto* functionNode = dynamic_cast<FunctionASTNode*>(statement.get())) {
            predeclareFunction(*functionNode, applyModulePrefix(functionNode->name));
        } else if (auto* implNode = dynamic_cast<ImplBlockASTNode*>(statement.get())) {
            const std::string target = applyModulePrefix(implNode->targetType);
            for (const auto& method : implNode->methods) {
                if (method) {
                    predeclareFunction(*method, target + "__" + method->name);
                }
            }
        }
    }

    currentModulePrefix.clear();

    llvm::Function* defaultMainFunction = nullptr;
    if (!hasUserMain) {
        llvm::FunctionType* mainType = llvm::FunctionType::get(
            builder->getInt32Ty(),
            false
        );
        defaultMainFunction = llvm::Function::Create(
            mainType,
            llvm::Function::ExternalLinkage,
            "main",
            module.get()
        );
        llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(
            *context,
            "entry",
            defaultMainFunction
        );
        builder->SetInsertPoint(entryBlock);
    }
    initPrintHelpers();

    for (const auto& statement : node.statements) {
        if (auto* modNode = dynamic_cast<ModuleDeclASTNode*>(statement.get())) {
            modNode->accept(*this);
            continue;
        }
        if (dynamic_cast<StructDeclASTNode*>(statement.get()) ||
            dynamic_cast<UnionDeclASTNode*>(statement.get()) ||
            dynamic_cast<EnumDeclASTNode*>(statement.get())) {
            continue;
        }
        if (auto* functionNode = dynamic_cast<FunctionASTNode*>(statement.get())) {
            functionNode->accept(*this);
            continue;
        }
        if (auto* implNode = dynamic_cast<ImplBlockASTNode*>(statement.get())) {
            implNode->accept(*this);
            continue;
        }
        generateStatement(statement.get());
    }


    currentModulePrefix.clear();

    if (defaultMainFunction) {
        llvm::BasicBlock* finalBlock = builder->GetInsertBlock();
        if (finalBlock && finalBlock->getParent() == defaultMainFunction && !finalBlock->getTerminator()) {
            builder->CreateRet(builder->getInt32(0));
        }
        terminateUnfinishedBlocks(defaultMainFunction);
    }

    verifyCurrentModule();
}

void CodeGen::generate(ProgramASTNode* program) {
    if (!program) {
        throw std::runtime_error("Program is empty");
    }
    program->accept(*this);
}

void CodeGen::printIR() {
    verifyCurrentModule();

    std::error_code error;
    llvm::raw_fd_ostream file("output.ll", error);
    if (error) {
        throw std::runtime_error("Could not open output.ll: " + error.message());
    }
    module->print(file, nullptr);
}
