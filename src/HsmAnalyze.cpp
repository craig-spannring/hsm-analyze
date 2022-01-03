#include "clang/Basic/Version.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

#include "DotGenerator.h"
#include "HsmAstMatcher.h"
#include "HsmTypes.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

const char *ToolName = "hsm-analyze";
const char *ToolVersiona = "1.0.0.0";
static const char *ToolDescription = "A tool that analyzes C++ code that makes "
                                     "use of the HSM library "
                                     "(https://github.com/amaiorano/hsm)\n";

static cl::OptionCategory HsmAnalyzeCategory("hsm-analyze options");

static cl::opt<bool> PrintMap("map", cl::desc("Print state-transition map"),
                              cl::cat(HsmAnalyzeCategory));

static cl::opt<bool>
    PrintDotFile("dot", cl::desc("Print state-transition dot file contents"),
                 cl::cat(HsmAnalyzeCategory));

static cl::opt<bool> DotLeftRightOrdering(
    "lr", cl::desc("dot option: left-right ordering (default is top-down)"),
    cl::cat(HsmAnalyzeCategory));

static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

static void PrintVersion(raw_ostream& raw) {
    raw << ToolName << " " << ToolVersiona << '\n';
}

int main(int argc, const char **argv) {
  cl::AddExtraVersionPrinter(PrintVersion);

  auto ExpectedParser = CommonOptionsParser::create(argc, argv,
                                                    HsmAnalyzeCategory,
                                                    llvm::cl::OneOrMore,
                                                    ToolDescription);
  CommonOptionsParser& OptionsParser = ExpectedParser.get();

  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  if (!(PrintMap || PrintDotFile)) {
    llvm::errs() << "Nothing to do. Please specify "
                    "an action to perform.\n\n";
    cl::PrintHelpMessage();
    return -1;
  }

  StateTransitionMap Map;
  MatchFinder Finder;
  HsmAstMatcher::addMatchers(Finder, Map);

  auto Result = Tool.run(newFrontendActionFactory(&Finder).get());
  if (Result != 0) {
    return Result;
  }

  if (PrintMap) {
    for (auto &kvp : Map) {
      auto SourceStateName = kvp.first;
      auto TransType = std::get<0>(kvp.second);
      auto TargetStateName = std::get<1>(kvp.second);

      llvm::outs() << SourceStateName << " "
                   << TransitionTypeVisualString[static_cast<int>(TransType)]
                   << " " << TargetStateName << "\n";
      llvm::outs().flush();
    }
  }

  if (PrintDotFile) {
    DotGenerator::Options Options;
    Options.LeftRightOrdering = DotLeftRightOrdering;
    auto DotFileContents = DotGenerator::generateDotFileContents(Map, Options);
    llvm::outs() << DotFileContents;
    llvm::outs().flush();
  }
}
