#ifndef ENCODER_ENCODER_H
#define ENCODER_ENCODER_H

#include <z3++.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <map>

class InstExprMap{
public:
    void insert(const llvm::Instruction*, z3::expr&);
    z3::expr get(const llvm::Value*) const;
private:
    std::map<const llvm::Instruction*,z3::expr> m_mapInstExpr;
};

z3::expr encode(const llvm::Function&);
z3::expr encode(const llvm::Instruction*, InstExprMap&, 
    const std::map<const llvm::BasicBlock*,z3::expr>&,
    const std::map<const llvm::BasicBlock*,z3::expr_vector>&);
z3::expr encode(const llvm::PHINode*, InstExprMap&,
    const std::map<const llvm::BasicBlock*,z3::expr>&,
    const std::map<const llvm::BasicBlock*,z3::expr_vector>&);

z3::expr encode(const llvm::CmpInst*, InstExprMap&);

#endif