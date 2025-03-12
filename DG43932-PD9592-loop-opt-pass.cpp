#include "llvm/Analysis/DominanceFrontier.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

using namespace llvm;

struct LoopPass : PassInfoMixin<LoopPass>
{
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM)
    {
        auto &LI = FAM.getResult<LoopAnalysis>(F);
        auto &DT = FAM.getResult<DominatorTreeAnalysis>(F);
        // Process all loops in depth-first order
        for (Loop *L : LI)
        {
            processLoop(L, DT);
        }

        return PreservedAnalyses::all();
    }

private:
    void processLoop(Loop *L, DominatorTree &DT)
    {
        // Process subloops first
        for (Loop *SubLoop : L->getSubLoops())
        {
            processLoop(SubLoop, DT);
        }

        BasicBlock *Preheader = L->getLoopPreheader();
        if (!Preheader)
            return;

        SmallVector<Instruction *, 16> Invariants;

        for (BasicBlock *BB : L->blocks())
        {
            for (auto I = BB->rbegin(); I != BB->rend(); ++I)
            {
                if (isLoopInvariant(&*I, L, DT))
                {
                    Invariants.push_back(&*I);
                }
            }
        }

        for (Instruction *I : Invariants)
        {
            if (safeToHoist(I, L, DT))
            {
                I->moveBefore(Preheader->getTerminator());
            }
        }
    }

    bool isLoopInvariant(Instruction *I, Loop *L, DominatorTree &DT)
    {
        if (!isa<BinaryOperator>(I) && !isa<SelectInst>(I) &&
            !isa<CastInst>(I) && !isa<GetElementPtrInst>(I) && !I->isShift())
        {
            return false;
        }

        if (I->isTerminator() || isa<PHINode>(I))
        {
            return false;
        }

        // we nedd to make sure that the operands are constants or expresssed
        // outsided of loop
        for (Value *Op : I->operands())
        {
            if (isa<Constant>(Op))
            {
                continue;
            }

            if (Instruction *OpInst = dyn_cast<Instruction>(Op))
            {
                if (L->contains(OpInst->getParent()))
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool safeToHoist(Instruction *I, Loop *L, DominatorTree &DT)
    {
        if (!isSafeToSpeculativelyExecute(I, nullptr, nullptr, &DT))
        {
            return false;
        }

        // make sure that we can't have exceptions and if we can't then we check
        // to make sure that this is safe
        if (I->mayThrow())
        {
            SmallVector<BasicBlock *, 4> ExitBlocks;
            L->getExitBlocks(ExitBlocks);

            for (BasicBlock *Exit : ExitBlocks)
            {
                if (!DT.dominates(I->getParent(), Exit))
                {
                    return false;
                }
            }
        }
        return true;
    }

    static bool isRequired() { return true; }
};

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getHelloWorldPluginInfo()
{
    return {LLVM_PLUGIN_API_VERSION, "DG43932-PD9592-Loop-Opt-Pass",
            LLVM_VERSION_STRING, [](PassBuilder &PB)
            {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, FunctionPassManager &FPM,
                       ArrayRef<PassBuilder::PipelineElement>)
                    {
                        if (Name == "DG43932-PD9592-loop-opt-pass")
                        {
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
llvmGetPassPluginInfo()
{
    return getHelloWorldPluginInfo();
}
