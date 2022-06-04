#include "Unroller/Unroller.h"
#include "Unroller/LoopKUnrollPass.h"

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Pass.h>
#include <llvm/Transforms/Scalar.h>

using namespace std;
using namespace llvm;

void unrollLoops(Module& mod, unsigned depth){
    legacy::PassManager PM;
    PM.add(createLoopRotatePass());
    PM.add(new LoopKUnrollPass(depth));
    PM.run(mod);

}
