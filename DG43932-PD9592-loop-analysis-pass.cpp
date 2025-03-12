#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// New PM implementation
struct LoopPass : PassInfoMixin<LoopPass>
{
    // Main entry point, takes IR unit to run the pass on (&F) and the
    // corresponding pass manager (to be queried if need be)
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM)
    {
        auto &LI = FAM.getResult<LoopAnalysis>(F);
        static int loopCounter = 0;

        for (Loop *L : LI.getLoopsInPreorder())
        {
            std::string funcName = F.getName().str();
            unsigned depth = L->getLoopDepth();
            bool hasSubLoops = !L->getSubLoops().empty();

            unsigned topLevelBBs = 0;
            for (BasicBlock *BB : L->blocks())
            {
                bool inSubLoop = false;
                for (Loop *SubL : L->getSubLoops())
                {
                    if (SubL->contains(BB))
                    {
                        inSubLoop = true;
                        break;
                    }
                }
                if (!inSubLoop)
                    topLevelBBs++;
            }

            SmallVector<BasicBlock *, 32> allBlocks;
            for (BasicBlock *BB : L->getBlocks()) {
                allBlocks.push_back(BB);
            }

            unsigned instrs = 0;
            unsigned atomics = 0;
            for (BasicBlock *BB : allBlocks)
            {
                instrs += BB->size();
                for (Instruction &I : *BB)
                {
                    if (I.isAtomic())
                        atomics++;
                }
            }

            unsigned branches = 0;
            for (BasicBlock *BB : L->blocks())
            {
                bool inSubLoop = false;
                for (Loop *SubL : L->getSubLoops())
                {
                    if (SubL->contains(BB))
                    {
                        inSubLoop = true;
                        break;
                    }
                }
                if (!inSubLoop)
                {
                    Instruction *Term = BB->getTerminator();
                    if (isa<BranchInst>(Term) || isa<SwitchInst>(Term) || isa<IndirectBrInst>(Term))
                    {
                        branches++;
                    }
                }
            }

            errs() << loopCounter << ": func=" << funcName
                   << ", depth=" << depth
                   << ", subLoops=" << (hasSubLoops ? "true" : "false")
                   << ", BBs=" << topLevelBBs
                   << ", instrs=" << instrs
                   << ", atomics=" << atomics
                   << ", branches=" << branches << "\n";
            loopCounter++;
        }

        return PreservedAnalyses::all();
    }

    // Without isRequired returning true, this pass will be skipped for functions
    // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
    // all functions with optnone.
    static bool isRequired() { return true; }
};

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getHelloWorldPluginInfo()
{
    return {LLVM_PLUGIN_API_VERSION, "DG43932-PD9592-Loop-Analysis-Pass", LLVM_VERSION_STRING,
            [](PassBuilder &PB)
            {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, FunctionPassManager &FPM,
                       ArrayRef<PassBuilder::PipelineElement>)
                    {
                        if (Name == "DG43932-PD9592-loop-analysis-pass")
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
