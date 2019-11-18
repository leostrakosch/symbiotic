//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include <cassert>
#include <vector>
#include <set>

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/Type.h"
#if LLVM_VERSION_MAJOR >= 4 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 5)
  #include "llvm/IR/InstIterator.h"
#else
  #include "llvm/Support/InstIterator.h"
#endif
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <llvm/IR/DebugInfoMetadata.h>

using namespace llvm;

bool CloneMetadata(const llvm::Instruction *i1, llvm::Instruction *i2);

namespace {

class FindExits : public FunctionPass {
  public:
    static char ID;

    FindExits() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override;
    bool processBlock(BasicBlock& B);
};

bool FindExits::runOnFunction(Function &F)
{
  bool modified = false;

  for (auto& B : F) {
      modified |= processBlock(B);
  }

  return modified;
}

bool FindExits::processBlock(BasicBlock& B) {
  bool modified = false;
  Module *M = B.getParent()->getParent();
  LLVMContext& Ctx = M->getContext();

  Type *argTy = Type::getInt32Ty(Ctx);
  auto exitC = M->getOrInsertFunction("__VERIFIER_silent_exit",
                                       Type::getVoidTy(Ctx), argTy
#if LLVM_VERSION_MAJOR < 5
                                       , nullptr
#endif
                                       );
#if LLVM_VERSION_MAJOR >= 9
        auto exitF = cast<Function>(exitC.getCallee());
#else
        auto exitF = cast<Function>(exitC);
#endif
  exitF->addFnAttr(Attribute::NoReturn);

  if (succ_begin(&B) == succ_end(&B)) {
    auto new_CI = CallInst::Create(exitF, {ConstantInt::get(argTy, 0)});
    auto& BI = B.back();
    CloneMetadata(&BI, new_CI);
    new_CI->insertBefore(&BI);
  }

  /*
  for (auto I = B.begin(), E = B.end(); I != E;) {
    Instruction *ins = &*I;
    ++I;
    if (auto CI = dyn_cast<CallInst>(ins)) {
      auto calledFun = dyn_cast<Function>(CI->getCalledValue()->stripPointerCasts());
      if (!calledFun)
          continue;

      if (calledFun->hasFnAttribute(Attribute::NoReturn))
          ...

      auto new_CI = CallInst::Create(exitF);
      CloneMetadata(CI, new_CI);
      new_CI->insertAfter(CI);

      modified = true;
    }
  }
  */

  return modified;
}

} // namespace

static RegisterPass<FindExits> DM("find-exits",
                                  "Put calls to __VERIFIER_silent_exit into bitcode "
                                  "before any exit from the program.");
char FindExits::ID;