#define DEBUG_TYPE "bugon-libc"
#include "BugOn.h"
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/ADT/DenseMap.h>

using namespace llvm;

namespace {

struct BugOnLibc : BugOnPass {
	static char ID;
	BugOnLibc() : BugOnPass(ID) {}

	virtual bool doInitialization(Module &);
	virtual bool visit(Instruction *);

private:
	typedef bool (BugOnLibc::*handler_t)(CallInst *);
	DenseMap<Function *, handler_t> Handlers;

	bool visitAbs(CallInst *);
	bool visitDiv(CallInst *);
};

} // anonymous nmaespace

bool BugOnLibc::doInitialization(Module &M) {
#define HANDLER(method, name) \
	if (Function *F = M.getFunction(name)) \
		Handlers[F] = &BugOnLibc::method;

	HANDLER(visitAbs, "abs");
	HANDLER(visitAbs, "labs");
	HANDLER(visitAbs, "llabs");

	HANDLER(visitDiv, "div");
	HANDLER(visitDiv, "ldiv");
	HANDLER(visitDiv, "lldiv");

#undef HANDLER
	return false;
}

bool BugOnLibc::visit(Instruction *I) {
	CallInst *CI = dyn_cast<CallInst>(I);
	if (!CI)
		return false;
	Function *F = CI->getCalledFunction();
	if (!F)
		return false;
	handler_t Handler = Handlers.lookup(F);
	if (!Handler)
		return false;
	return (this->*Handler)(CI);
}

// abs(x): x == INT_MIN
bool BugOnLibc::visitAbs(CallInst *I) {
	if (I->getNumArgOperands() != 1)
		return false;
	Value *R = I->getArgOperand(0);
	IntegerType *T = dyn_cast<IntegerType>(R->getType());
	if (!T)
		return false;
	Constant *SMin = ConstantInt::get(T, APInt::getSignedMinValue(T->getBitWidth()));
	Value *V = Builder->CreateICmpEQ(R, SMin);
	return insert(V, I->getCalledFunction()->getName());
}

// div(numer, denom): denom == 0 || (numer == INT_MIN && denom == -1)
bool BugOnLibc::visitDiv(CallInst *I) {
	if (I->getNumArgOperands() != 2)
		return false;
	Value *L = I->getArgOperand(0);
	Value *R = I->getArgOperand(1);
	IntegerType *T = dyn_cast<IntegerType>(L->getType());
	if (!T)
		return false;
	if (T != R->getType())
		return false;
	bool Changed = false;
	StringRef Name = I->getCalledFunction()->getName();
	Changed |= insert(Builder->CreateIsNull(R), Name);
	Constant *SMin = ConstantInt::get(T, APInt::getSignedMinValue(T->getBitWidth()));
	Constant *MinusOne = Constant::getAllOnesValue(T);
	Value *V = Builder->CreateAnd(
		Builder->CreateICmpEQ(L, SMin),
		Builder->CreateICmpEQ(R, MinusOne));
	Changed |= insert(V, Name);
	return Changed;
}

char BugOnLibc::ID;

static RegisterPass<BugOnLibc>
X("bugon-libc", "Insert bugon calls for libc functions");