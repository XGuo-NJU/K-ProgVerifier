#include <string>
#include <sstream>

#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>

#include "Unroller/Unroller.h"
#include "Encoder/Encoder.h"
#include <llvm/Analysis/CFG.h>
#include <z3++.h>
#include <iostream>

using namespace std;
using namespace llvm;
using namespace z3;

string compileCodeToIR(const string& t_CFilename);

LLVMContext g_llvmCtx;

#define DEBUG

int main(int argc, char* argv[]){
	auto IRFilename = compileCodeToIR(argv[1]);
	SMDiagnostic err;
	auto MPtr  = parseIRFile(IRFilename,err,g_llvmCtx);
	unrollLoops(*MPtr,10);
	errs()<<*MPtr->getFunction("main")<<"\n";

#ifdef DEBUG
	SmallVector<pair<const BasicBlock*, const BasicBlock*>> backedges;
	FindFunctionBackedges(*MPtr->getFunction("main"),backedges);
	assert(backedges.empty());
#endif
	auto formula = encode(*MPtr->getFunction("main"));
	cout<<formula<<endl;
	solver s(formula.ctx());
    s.add(formula);
    check_result res = s.check();
    if(res == z3::sat){
		cout<<"Err Block Reached!"<<endl;
	}
	else{
		cout<<"Program Safe!"<<endl;
	}
	return 0;
}

string compileCodeToIR(const string& t_CFilename){
	auto posExt = t_CFilename.find_last_of('.');
	auto ext = t_CFilename.substr(posExt);
	if(".c"!=ext){
		return "";
	}
	const string IRFilename = t_CFilename.substr(0,posExt)+".bc";
	ostringstream cmd;
	// cmd << "clang -emit-llvm -O2 -finline-hint-functions"<<" -c "<<t_CFilename<<" -o "<<IRFilename;
	cmd << "clang -emit-llvm -O3"<<" -c "<<t_CFilename<<" -o "<<IRFilename;
	return system(cmd.str().c_str())==0?IRFilename:"";
}
