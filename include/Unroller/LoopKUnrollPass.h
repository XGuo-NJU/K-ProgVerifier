#ifndef UNROLLER_LOOPKUNROLLPASS_H
#define UNROLLER_LOOPKUNROLLPASS_H


#include <llvm/Pass.h>


class LoopKUnrollPass : public llvm::FunctionPass {
public:
    LoopKUnrollPass(unsigned t_depth);
    bool runOnFunction(llvm::Function&);
    llvm::StringRef getPassName() const;
    void getAnalysisUsage(llvm::AnalysisUsage& AU) const;
public:
    static char ID;

private:
    unsigned m_depth;
};

#endif