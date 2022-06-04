#ifndef ENCODER_UTILS_H
#define ENCODER_UTILS_H

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <vector>
#include <map>
#include <set>

#include <z3++.h>

typedef std::set<const llvm::BasicBlock*> ConstBlockSet;

void wrapEdgeInfo(const llvm::Function&,std::map<const llvm::BasicBlock*,ConstBlockSet>&,std::map<const llvm::BasicBlock*,ConstBlockSet>&);

std::vector<const llvm::BasicBlock*> BBTopOrderSort(const llvm::Function&);

int readConstInt( const llvm::ConstantInt* CI);

unsigned getSuccessorIndex( const llvm::BasicBlock* BB, const llvm::BasicBlock* succ );

z3::expr createFreshExprForInst(z3::context& ctx,const llvm::Instruction*);
z3::expr createFreshBool(z3::context& ctx, std::string name);
z3::expr createFreshInt(z3::context& ctx, std::string name);
z3::expr createConstTerm(z3::context& ctx , const llvm::Constant*);
z3::expr And(z3::context& ctx,const z3::expr_vector& exprs);
z3::expr Or(z3::context& ctx,const z3::expr_vector& exprs);

#endif