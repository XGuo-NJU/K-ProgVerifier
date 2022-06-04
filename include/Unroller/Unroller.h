#ifndef UNROLLER_UNROLLER_H
#define UNROLLER_UNROLLER_H

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
void unrollLoops(llvm::Module&, unsigned);

#endif

