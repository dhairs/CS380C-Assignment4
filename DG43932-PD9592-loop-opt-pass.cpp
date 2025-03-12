#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Analysis/DominanceFrontier.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Analysis/ValueTracking.h"

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

        // Collect candidate instructions in reverse order
        for (BasicBlock *BB : L->blocks())
        {
            for (auto I = BB->rbegin(); I != BB->rend(); ++I)
            {
                I->dump();
                if (isLoopInvariant(&*I, L, DT))
                {
                    Invariants.push_back(&*I);
                    errs() << "is invariant!!!!!!!!!!!!!!!!!!!!!!!!\n";
                }
                else
                {
                    errs() << "is not invariant\n";
                }
            }
        }

        // Hoist safe instructions
        errs() << "HOIST PHASE\n";
        for (Instruction *I : Invariants)
        {
            I->dump();
            if (safeToHoist(I, L, DT))
            {
                I->moveBefore(Preheader->getTerminator());
                errs() << "is safe to hoist\n";
            }
            else
            {
                errs() << "is not safe to hoist\n";
            }
        }
    }

    bool isLoopInvariant(Instruction *I, Loop *L, DominatorTree &DT)
    {
        // Check allowed instruction types
        if (!isa<BinaryOperator>(I) &&
            !isa<SelectInst>(I) &&
            !isa<CastInst>(I) &&
            !isa<GetElementPtrInst>(I) &&
            !I->isShift())
        {
            return false;
        }

        // Disallowed instruction types
        if (I->isTerminator() || isa<PHINode>(I))
        {
            return false;
        }

        // Check all operands are either:
        // 1. Constants, or
        // 2. Computed outside the loop
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
        if (!isSafeToSpeculativelyExecute(I, nullptr, &DT))
        {
            errs() << "not safe to spec exec\n";
            return false;
        }

        // Only need to check exit dominance for potentially excepting instructions
        if (I->mayThrow())
        {
            SmallVector<BasicBlock *, 4> ExitBlocks;
            L->getExitBlocks(ExitBlocks);

            for (BasicBlock *Exit : ExitBlocks)
            {
                if (!DT.dominates(I->getParent(), Exit))
                {
                    errs() << I->getParent() << " does not dominate " << Exit << '\n';
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
    return {LLVM_PLUGIN_API_VERSION, "DG43932-PD9592-Loop-Opt-Pass", LLVM_VERSION_STRING,
            [](PassBuilder &PB)
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
