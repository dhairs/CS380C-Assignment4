#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"


#include "llvm/Analysis/DominanceFrontier.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Analysis/ValueTracking.h"

using namespace llvm;

struct LoopPass : PassInfoMixin<LoopPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        // errs() << "BEFORE TRANSFORM\n";
        // F.dump();
        auto &LI = FAM.getResult<LoopAnalysis>(F);
        auto &DT = FAM.getResult<DominatorTreeAnalysis>(F);
        // Process all loops in depth-first order
        for (Loop *L : LI) {
            processLoop(L, DT);
        }

        // errs() << "AFTER TRANSFORM\n";
        // F.dump();
        return PreservedAnalyses::all();
    }

private:
    void processLoop(Loop *L, DominatorTree &DT) {
        // Process subloops first
        for (Loop *SubLoop : L->getSubLoops()) {
            processLoop(SubLoop, DT);
        }

        BasicBlock *Preheader = L->getLoopPreheader();
        if (!Preheader) return;

        SmallVector<Instruction *, 16> Invariants;

        // Collect candidate instructions in reverse order
        for (BasicBlock *BB : L->blocks()) {
            for (auto I = BB->rbegin(); I != BB->rend(); ++I) {
                if (isLoopInvariant(&*I, L, DT)) {
                    Invariants.push_back(&*I);
                }
            }
        }

        // Hoist safe instructions
        for (Instruction *I : Invariants) {
            if (safeToHoist(I, L, DT)) {
                I->moveBefore(Preheader->getTerminator());
            }
        }
    }

    bool isLoopInvariant(Instruction *I, Loop *L, DominatorTree &DT) {
        // First check instruction type
        if (!isa<BinaryOperator>(I) &&
            !isa<SelectInst>(I) &&
            !isa<CastInst>(I) &&
            !isa<GetElementPtrInst>(I) &&
            !I->isShift()) {
            return false;
        }

        if (I->isTerminator() || isa<PHINode>(I)) {
            return false;
        }
        
        for (Value *Op : I->operands()) {
            if (Instruction *OpInst = dyn_cast<Instruction>(Op)) {
                if (L->contains(OpInst->getParent()) || !isLoopInvariant(OpInst, L, DT)) {
                    return false;
                }
            }
            else if (!isa<Constant>(Op)) {
                return false;
            }
        }
        return true;
    }

    bool safeToHoist(Instruction *I, Loop *L, DominatorTree &DT) {
        if (!isSafeToSpeculativelyExecute(I)) {
            return false;
        }

        SmallVector<BasicBlock *, 4> ExitBlocks;
        L->getExitBlocks(ExitBlocks);

        for (BasicBlock *Exit : ExitBlocks) {
            if (!DT.dominates(I->getParent(), Exit)) {
                return false;
            }
        }
        return true;
    }

    static bool isRequired() { return true; }
};

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getHelloWorldPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "DG43932-PD9592-Loop-Opt-Pass", LLVM_VERSION_STRING,
            [](PassBuilder &PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, FunctionPassManager &FPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "DG43932-PD9592-loop-opt-pass") {
                            FPM.addPass(LoopSimplifyPass());
                            FPM.addPass(LoopPass());
                            return true;
                        }
                        return false;
                    });
            }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getHelloWorldPluginInfo();
}
