#include "Encoder/Utils.h"

#include <list>

using namespace std;
using namespace llvm;
using namespace z3;

void wrapEdgeInfo(const Function& F,map<const BasicBlock*,ConstBlockSet>& inEdges,map<const BasicBlock*,ConstBlockSet>& outEdges){
    for(auto &BB : F){
        auto term = BB.getTerminator();
        assert(term);
        for(unsigned idx=0;idx<term->getNumSuccessors();idx++){
            auto succ = term->getSuccessor(idx);
            inEdges[succ].insert(&BB);
            outEdges[&BB].insert(succ);
        }
    }
}

vector<const BasicBlock*> BBTopOrderSort(const Function& F){
    vector<const BasicBlock*> blocks;
    map<const BasicBlock*,ConstBlockSet> inEdges,outEdges;
    wrapEdgeInfo(F,inEdges,outEdges);

    list<const BasicBlock*> worklist{&F.getEntryBlock()};
    while(!worklist.empty()){
        auto BB = worklist.front();
        worklist.pop_front();
        blocks.push_back(BB);
        for(auto succ : outEdges[BB]){
            inEdges[succ].erase(BB);
            if(inEdges[succ].empty()){
                worklist.emplace_back(succ);
            }
        }
    }
    return blocks;
}

int readConstInt( const ConstantInt* CI) {
    const APInt& n = CI->getUniqueInteger();
    unsigned len = n.getNumWords();
    assert( len <= 1 );
    return *n.getRawData();
}

unsigned getSuccessorIndex( const BasicBlock* BB, const BasicBlock* succ ) {
    auto term = BB->getTerminator();
    unsigned idx = 0u;
    for(; idx < term->getNumSuccessors();idx++ )  
        if( succ == term->getSuccessor( idx) ) 
            return idx;
    assert(0);
    return idx;
}

expr createFreshExprForInst(context& ctx,const Instruction* I){
    static unsigned cnt = 0u;
    cnt ++;

    auto type = I->getType();
    if(type->isIntegerTy()){
        auto BW = type->getIntegerBitWidth();
        if(32u == BW){
            return ctx.int_const(("i_%"+to_string(cnt)).c_str());
        }
        else if(1 == BW){
            return ctx.bool_const(("b_%"+to_string(cnt)).c_str());
        }
        else{
            assert(0);
        }
        
    }
    else if(type->isFloatTy() || type->isDoubleTy()){
        return ctx.real_const(("fp_%"+to_string(cnt)).c_str());
    }
    else{
        assert(0);
    }
    return expr(ctx);
}

expr createFreshBool(context& ctx, string name)
{
    static unsigned cnt = 0u;
    cnt++;
    stringstream ss;
    ss << "b_" << cnt << "_"<< name;
    return ctx.bool_const(ss.str().c_str());
}

expr createFreshInt(context& ctx, string name)
{
    static unsigned cnt = 0u;
    cnt++;
    stringstream ss;
    ss << "i_" << cnt << "_"<< name;
    return ctx.int_const(ss.str().c_str());
}

expr createConstTerm(context& ctx , const Constant* C) {
    if( const ConstantInt* CI = dyn_cast<ConstantInt>(C) ) {
        unsigned BW = CI->getBitWidth();
        if(BW == 32u)
            return ctx.int_val(readConstInt(CI));
        else if(BW == 1u) {
            int i = readConstInt(CI);
            assert( i == 0 || i == 1 );
            return i == 1?ctx.bool_val(true):ctx.bool_val(false);
        }
        else{
            assert(0);
        }
            
    }
    else if(const ConstantFP* CFP = dyn_cast<ConstantFP>(C) ) {
        auto type = CFP->getType();
        const APFloat& n = CFP->getValueAPF();
        double val = 0.0;
        if(type->isFloatTy()){
            val = n.convertToFloat();
        }
        else if(type->isDoubleTy()){
            val = n.convertToDouble();
        }
        else{
            assert(0);
        }
        return ctx.real_val(to_string(val).c_str());
    }
    else{
        assert(0);
    }
    expr e(ctx);
    return e;
}



expr And(context& ctx,const expr_vector& exprs) {
    switch (exprs.size())
    {
    case 0:
        return ctx.bool_val(true);
        break;
    case 1:
        return exprs.back();
        break;
    default:
        return mk_and(exprs);
        break;
    }
}

expr Or(context& ctx,const expr_vector& exprs) {
    switch (exprs.size())
    {
    case 0:
        return ctx.bool_val(true);
        break;
    case 1:
        return exprs.back();
        break;
    default:
        return mk_or(exprs);
        break;
    }
}
