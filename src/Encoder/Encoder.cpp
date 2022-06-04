#include "Encoder/Encoder.h"
#include "Encoder/Utils.h"

#include <iostream>

#include <llvm/IR/Instructions.h>

using namespace std;
using namespace llvm;
using namespace z3;

z3::context g_Z3Ctx;

void InstExprMap::insert(const Instruction* I, expr& e){
    assert(m_mapInstExpr.insert({I,e}).second);
}

expr InstExprMap::get(const Value* V) const{
    if(auto I = dyn_cast<Instruction>(V)){
        auto it = m_mapInstExpr.find(I);
        assert(it!=m_mapInstExpr.end());
        return it->second;
    }
    else if(auto C = dyn_cast<Constant>(V)){
        return createConstTerm(g_Z3Ctx, C);
    }
    else{
        assert(0);
    }
}



expr encode(const Function& F){
    auto blocks = BBTopOrderSort(F);
    map<const BasicBlock*, set<const BasicBlock*>> inEdges;
    cout<<blocks.size()<<" "<<F.getBasicBlockList().size()<<endl;
    assert(blocks.size() == F.getBasicBlockList().size());

    map<const BasicBlock*,expr> BBEntryBit;
    map<const BasicBlock*,expr_vector> BBExitBits;

    expr ErrEntryBit(g_Z3Ctx);
    
    for(auto BB : blocks){
        expr entryBit = createFreshBool(g_Z3Ctx,"entry");
        expr_vector exitBits(g_Z3Ctx);
        auto term = BB->getTerminator();
        auto numSucc = term ->getNumSuccessors();
        for(unsigned idx = 0;idx<numSucc;idx++){
            auto succ = term->getSuccessor(idx);
            inEdges[succ].insert(BB);
            exitBits.push_back(createFreshBool(g_Z3Ctx,"exit_"+to_string(idx)));
        }
        BBEntryBit.insert({BB, entryBit});
        BBExitBits.insert({BB, exitBits});

        if(BB->getName()=="Err"){
            ErrEntryBit = entryBit;
        }
    }
    assert(ErrEntryBit);

    expr_vector exprs(g_Z3Ctx);
    InstExprMap mapInstExpr;

    exprs.push_back(BBEntryBit.find(blocks[0])->second);
    for(auto BB : blocks){
        auto entryBit = BBEntryBit.find(BB)->second;
        auto exitBits = BBExitBits.find(BB)->second;

        expr_vector incoming(g_Z3Ctx);
        for(auto pred : inEdges[BB]){
            auto predExitBits = BBExitBits.find(pred)->second;
            incoming.push_back(predExitBits[getSuccessorIndex(pred,BB)]);
        }
        if(!incoming.empty()){
            exprs.push_back(Or(g_Z3Ctx,incoming) == entryBit);
        }
        if(!exitBits.empty()){
            exprs.push_back(entryBit == Or(g_Z3Ctx,exitBits));
        }
        for(auto &I : *BB){
            auto e = encode(&I,mapInstExpr,BBEntryBit,BBExitBits);
            if(e){
                exprs.push_back(e);
            }
        }
    }
    exprs.push_back(ErrEntryBit);

    return And(g_Z3Ctx,exprs);

}

expr encode(const Instruction* I, InstExprMap& mapInstExpr,
    const map<const BasicBlock*,expr>& BBEntryBit,
    const map<const BasicBlock*,expr_vector>& BBExitBits){
    expr e(g_Z3Ctx);
    if(auto call = dyn_cast<CallInst>(I)){
        auto callee = call->getCalledFunction()->getName();
        if("nondet_int"==callee){
            auto var = createFreshInt(g_Z3Ctx,"nondet_int");
            mapInstExpr.insert(I,var);
        }
    }
    else if(auto binOp = dyn_cast<BinaryOperator>(I)){
        auto op0 = binOp->getOperand( 0 );
        auto op1 = binOp->getOperand( 1 );
        expr a = mapInstExpr.get( op0 );
        expr b = mapInstExpr.get( op1 );
        expr res = createFreshExprForInst(g_Z3Ctx,I);
        mapInstExpr.insert(I,res);
        switch( binOp->getOpcode()) {
            case Instruction::Add : e = (res==(a+b)); break;
            case Instruction::Sub : e = (res==(a-b)); break;
            case Instruction::Mul : e = (res==(a*b)); break;
            case Instruction::And : e = (res==(a&&b)); break;
            case Instruction::Or : e = (res==(a||b)); break;
            default: assert(0);break;
        }
    }
    else if(auto phi = dyn_cast<PHINode>(I)){
        e = encode(phi,mapInstExpr,BBEntryBit,BBExitBits);
    }
    else if(auto br = dyn_cast<BranchInst>(I)){
       
        if(br->isConditional()){
            auto entryBit = BBEntryBit.find(I->getParent())->second;
            auto exitBits = BBExitBits.find(I->getParent())->second;
            expr cond =mapInstExpr.get(br->getCondition());
            e = ((entryBit &&cond) == exitBits[0]).simplify();
            e = e && ((entryBit && !cond) == exitBits[1]).simplify();
        }
    }
    else if(auto cmp = dyn_cast<CmpInst>(I)){
        e = encode(cmp,mapInstExpr);
    }
    else if(auto ret = dyn_cast<ReturnInst>(I)){

    }
    else{
        errs()<<"Disable to encode: "<< *I<<"\n";
        assert(0);
    }
    return e;
}

expr encode(const PHINode* phi, InstExprMap& mapInstExpr,
    const map<const BasicBlock*,expr>& BBEntryBit,
    const map<const BasicBlock*,expr_vector>& BBExitBits){
    unsigned num = phi->getNumIncomingValues();
    auto var = createFreshExprForInst(g_Z3Ctx,phi);
    mapInstExpr.insert(phi,var);

    expr_vector exprs(g_Z3Ctx);
    for( unsigned i = 0 ; i < num ; i++ ) {
        auto BB = phi->getParent();
        const BasicBlock* inBB = phi->getIncomingBlock(i);
        const Value* inVal = phi->getIncomingValue(i);

        expr inExpr = mapInstExpr.get(inVal);

        auto inBBExitBits = BBExitBits.find(inBB)->second;
        auto entryBit = BBEntryBit.find(BB)->second;
        auto idx = getSuccessorIndex(inBB,BB);
        exprs.push_back(implies(inBBExitBits[idx] && entryBit, var == inExpr));
    }
    return And(g_Z3Ctx,exprs);
}

expr encode(const CmpInst* cmp, InstExprMap& mapInstExpr){
    Value* op0 = cmp->getOperand( 0 ),*op1 = cmp->getOperand( 1 );
    expr a = mapInstExpr.get(op0), b = mapInstExpr.get(op1);
    expr res = createFreshExprForInst(g_Z3Ctx,cmp);
    mapInstExpr.insert(cmp,res);


    expr ret(g_Z3Ctx);

    switch(cmp->getPredicate()) {
        case CmpInst::ICMP_EQ  : ret = (res ==(a==b)); break;
        case CmpInst::ICMP_NE  : ret = (res ==(a!=b)); break;
        case CmpInst::ICMP_UGT : ret = (res ==(a>b)); break;
        case CmpInst::ICMP_UGE : ret = (res ==(a>=b)); break;
        case CmpInst::ICMP_ULT : ret = (res ==(a<b)); break;
        case CmpInst::ICMP_ULE : ret = (res ==(a<=b)); break;
        case CmpInst::ICMP_SGT : ret = (res ==(a>b)); break;
        case CmpInst::ICMP_SGE : ret = (res ==(a>=b)); break;
        case CmpInst::ICMP_SLT : ret = (res ==(a<b)); break;
        case CmpInst::ICMP_SLE : ret = (res ==(a<=b)); break;
        default: assert(0);break;
    }
    return ret;
}
