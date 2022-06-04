#include "Unroller/LoopKUnrollPass.h"
#include <llvm/Analysis/LoopPass.h>
#include <llvm/IR/IRBuilder.h>


#include <llvm/Transforms/Utils/Cloning.h>
#include <map>
#include <set>

#include "Unroller/Utils.h"
using namespace std;
using namespace llvm;

char LoopKUnrollPass::ID(0);

LoopKUnrollPass::LoopKUnrollPass(unsigned t_depth)
:FunctionPass(ID),m_depth(t_depth){}

bool LoopKUnrollPass::runOnFunction(Function& func){
    // errs()<<func<<"\n===============================\n";
    
    

    auto &LInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

    BasicBlock* errBlk = nullptr;
    if(!LInfo.empty()){
        errBlk = BasicBlock::Create(func.getContext(),"Err",&func);
        {
            IRBuilder<> builder(errBlk);
            auto ONE = ConstantInt::get(func.getContext(),APInt(32,1,true));
            ReturnInst::Create(func.getContext(),ONE,errBlk);
        }
    }
    for(auto L : LInfo.getTopLevelLoops()){
        assert(L->getSubLoops().empty());


        auto header = L->getHeader();
        auto latch = L->getLoopLatch();
        auto predecessor = L->getLoopPredecessor();
        assert(latch && predecessor);

        
        set<BasicBlock*> exitingBlks,exitBlks;
        map<BasicBlock*,bool> hasExitPHI;
        {
            SmallVector<BasicBlock*> SV;
            L->getExitingBlocks(SV);
            exitingBlks.insert(SV.begin(),SV.end());
            SV.clear();
            L->getExitBlocks(SV);
            exitBlks.insert(SV.begin(),SV.end());
            for(auto exit : exitBlks){
                bool hasPhi = false;
                for(auto &I : *exit){
                    if(auto phi = dyn_cast<PHINode>(&I)){
                        for(unsigned idx = 0 ;idx<phi->getNumIncomingValues();idx++){
                            auto inBlk = phi->getIncomingBlock(idx);
                            if(exitingBlks.count(inBlk)){
                                hasPhi = true;
                                break;
                            }
                        }
                    }
                    else{
                        break;
                    }
                }
                hasExitPHI[exit] = hasPhi;
            }
        }
        

        SmallVector<BasicBlock*> blks(L->getBlocksVector().begin(),L->getBlocksVector().end());

        for(unsigned i= 1;i<m_depth;i++){
            map<const BasicBlock*,BasicBlock*> BBMap;
            SmallVector<BasicBlock*> copies;
            ValueToValueMapTy LValMap;
            for(auto BB : blks){
                ValueToValueMapTy VMap;
                auto copy = CloneBasicBlock(BB,VMap,"",&func);
                copies.push_back(copy);
                BBMap.insert({BB,copy});
                LValMap.insert(VMap.begin(),VMap.end());
            }

            remapInstructionsInBlocks(copies,LValMap);

            for(auto BB : blks){
                for(auto &I : *BB){
                    if(auto phi=dyn_cast<PHINode>(&I)){
                        auto copyPHI = dyn_cast<PHINode>(LValMap[phi]);
                        for(unsigned idx =0 ;idx<phi->getNumIncomingValues();idx++){
                            auto inBlk = phi->getIncomingBlock(idx);
                            if(BBMap.count(inBlk)){
                                copyPHI->setIncomingBlock(idx,BBMap[inBlk]);
                            }
                        }
                    }
                    else{
                        break;
                    }
                }

                auto term = BB->getTerminator();
                assert(term);
                if(auto br = dyn_cast<BranchInst>(term)){
                    auto copyBr = dyn_cast<BranchInst>(LValMap[br]);
                    assert(copyBr);
                    for(unsigned idx = 0;idx<br->getNumSuccessors();idx++){
                        auto succ = br->getSuccessor(idx);
                        if(BBMap.count(succ)){
                            copyBr->setSuccessor(idx,BBMap[succ]);
                        }
                    } 
                }
                else if(isa<ReturnInst>(term)){
                }
                else{
                    assert(0);
                }
            }
            
            auto latchBr = dyn_cast<BranchInst>(latch->getTerminator());
            assert(latchBr);
            for(unsigned idx = 0;idx<latchBr->getNumSuccessors();idx++){
                auto succ = latchBr->getSuccessor(idx);
                if(succ == header){
                    latchBr->setSuccessor(idx,BBMap[header]);
                    break;
                }
            }
            set<BasicBlock*> newExitingBlks,newExitBlks;

            for(auto exiting : exitingBlks){
                newExitingBlks.insert(BBMap[exiting]);
                auto br = dyn_cast<BranchInst>(exiting->getTerminator());
                assert(br);
                for(unsigned idx=0;idx<br->getNumSuccessors();idx++){
                    auto succ = br->getSuccessor(idx);
                    if(exitBlks.count(succ)){
                        if(hasExitPHI[succ]){
                            BasicBlock* newExit = BasicBlock::Create(func.getContext(),"",&func,succ);
                            br->setSuccessor(idx,newExit);
                            auto copyBr = dyn_cast<BranchInst>(LValMap[br]);
                            copyBr->setSuccessor(idx,newExit);
                            IRBuilder<> builder(newExit);
                            for(auto &I : *succ){
                                if(auto phi = dyn_cast<PHINode>(&I)){
                                    assert(2 >= phi->getNumIncomingValues());
                                    for(unsigned phi_idx = 0;phi_idx<phi->getNumIncomingValues();phi_idx++){
                                        auto inBlk = phi->getIncomingBlock(phi_idx);
                                        auto inVal = phi->getIncomingValue(phi_idx);
                                        if(exiting == inBlk){
                                            auto newPhi = builder.CreatePHI(phi->getType(),2);
                    
                                            newPhi->addIncoming(inVal,inBlk);
                                            newPhi->addIncoming(LValMap[inVal],BBMap[inBlk]);
                            
                                            phi->setIncomingBlock(phi_idx,newExit);
                                            phi->setIncomingValue(phi_idx, newPhi);
                                        }
                                    }
                                }
                                else{
                                    break;
                                }
                            }
                            builder.CreateBr(succ);
                            newExitBlks.insert(newExit);
                            hasExitPHI[newExit] = true;
                        }
                        else{
                            newExitBlks.insert(succ);
                        }
                    }
                }
            }

            ValueToValueMapTy VMap;
            set<PHINode*> toRemove;
            for(auto &I : *header){
                assert(&I);
                if(auto phi = dyn_cast<PHINode>(&I)){
                    assert(2 == phi->getNumIncomingValues());
                    for(unsigned idx = 0;idx<phi->getNumIncomingValues();idx++){
                        if(phi->getIncomingBlock(idx) == predecessor){
                            VMap.insert({phi,phi->getIncomingValue(idx)});
                            toRemove.insert(phi);

                            auto copyPhi = dyn_cast<PHINode>(LValMap[phi]);
                            assert(copyPhi);
                            copyPhi->setIncomingBlock(idx,latch);
                            copyPhi->setIncomingValue(idx,phi->getIncomingValue(1-idx));
                        }
                    }
                }
            }
            remapInstructionsInBlocks(blks,VMap);
            remapInstructionsInBlocks(copies,VMap);
            remapInstructionsInBlocks(SmallVector<BasicBlock*>(newExitBlks.begin(),newExitBlks.end()),VMap);
            for(auto phi : toRemove){
                phi->eraseFromParent();
            }

            predecessor = latch;
            header = BBMap[header];
            latch = BBMap[latch];
            blks = copies;
            exitingBlks = newExitingBlks;
            exitBlks = newExitBlks;
        }
        ValueToValueMapTy VMap;
        set<PHINode*> toRemove;
        for(auto &I : *header){
            assert(&I);
            if(auto phi = dyn_cast<PHINode>(&I)){
                assert(2 == phi->getNumIncomingValues());
                for(unsigned idx = 0;idx<phi->getNumIncomingValues();idx++){
                    if(phi->getIncomingBlock(idx) == predecessor){
                        VMap.insert({phi,phi->getIncomingValue(idx)});
                        toRemove.insert(phi);
                    }
                }
            }
        }
        remapInstructionsInBlocks(blks,VMap);
        remapInstructionsInBlocks(SmallVector<BasicBlock*>(exitBlks.begin(),exitBlks.end()),VMap);
        for(auto phi : toRemove){
            phi->eraseFromParent();
        }

        auto latchBr = dyn_cast<BranchInst>(latch->getTerminator());
        assert(latchBr);
        assert(latchBr->isConditional());
        for(unsigned idx = 0;idx<latchBr->getNumSuccessors();idx++){
            auto succ = latchBr->getSuccessor(idx);
            if(succ ==  header){
                latchBr->setSuccessor(idx,errBlk);
            }
        }
    }

    // errs()<<func<<'\n';
    return !LInfo.getTopLevelLoops().empty();
}

StringRef LoopKUnrollPass::getPassName() const{
    return "LoopKUnrollPass";
}

void LoopKUnrollPass::getAnalysisUsage(AnalysisUsage& AU) const{
	AU.addRequired<LoopInfoWrapperPass>();
    AU.setPreservesAll();
}
