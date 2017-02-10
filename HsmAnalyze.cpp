// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...");

namespace {
bool areSameVariable(const ValueDecl *First, const ValueDecl *Second) {
  return First && Second &&
         First->getCanonicalDecl() == Second->getCanonicalDecl();
}

bool areSameExpr(ASTContext *Context, const Expr *First, const Expr *Second) {
  if (!First || !Second)
    return false;
  llvm::FoldingSetNodeID FirstID, SecondID;
  First->Profile(FirstID, *Context, true);
  Second->Profile(SecondID, *Context, true);
  return FirstID == SecondID;
}

std::string getName(const CXXRecordDecl &RD) {
  std::string NameWithTemplateArgs;
  llvm::raw_string_ostream OS(NameWithTemplateArgs);
  RD.getNameForDiagnostic(OS, RD.getASTContext().getPrintingPolicy(), true);
  return OS.str();
}

std::string stringRemove(const std::string &S, const std::string &ToRemove) {
  std::string Result;
  size_t Index = 0, Off = 0;
  while ((Index = S.find(ToRemove, Off)) != -1) {
    Result += S.substr(Off, Index - Off);
    Off = Index + ToRemove.length();
  }
  Result += S.substr(Off, S.length() - Off);
  return Result;
}

std::string getName(const TemplateArgument &TA) {
  auto Name = TA.getAsType().getAsString();
  return stringRemove(stringRemove(Name, "struct "), "clang ");
}
} // namespace

enum class TransitionType {
  Sibling,
  Inner,
  InnerEntry,
  No,
};

static const char *TransitionTypeString[] = {"Sibling", "Inner", "InnerEntry",
                                             "No"};

inline TransitionType fuzzyNameToTransitionType(const StringRef &Name) {
  auto contains = [](auto stringRef, auto s) {
    return stringRef.find(s) != StringRef::npos;
  };

  if (contains(Name, "SiblingTransition"))
    return TransitionType::Sibling;
  if (contains(Name, "InnerTransition"))
    return TransitionType::Inner;
  if (contains(Name, "InnerEntryTransition"))
    return TransitionType::InnerEntry;
  assert(contains(Name, "No"));
  return TransitionType::No;
}

const auto StateTransitionMatcher =
    // Match function calls that return transitions within state-derived classes
    callExpr(
        hasType(recordDecl(hasName("hsm::Transition"))),
        hasDescendant(declRefExpr(to(functionDecl(decl().bind("transfunc"))))),
        hasAncestor(
            cxxRecordDecl(decl().bind("state"), isDerivedFrom("hsm::State"))));

class StateTransitionMapper : public MatchFinder::MatchCallback {
public:
  virtual void run(const MatchFinder::MatchResult &Result) {
    auto StateDecl = Result.Nodes.getNodeAs<CXXRecordDecl>("state");
    auto TransitionFuncDecl = Result.Nodes.getNodeAs<FunctionDecl>("transfunc");

    if (StateDecl && TransitionFuncDecl) {
      if (auto TSI = TransitionFuncDecl->getTemplateSpecializationInfo()) {

        auto TransType = fuzzyNameToTransitionType(
            TransitionFuncDecl->getCanonicalDecl()->getName());

        auto StateName = getName(*StateDecl);

        const TemplateArgument &TA = TSI->TemplateArguments->get(0);
        auto TargetStateName = getName(TA);

        llvm::errs() << "State: " << StateName << ", Transitions: "
                     << TransitionTypeString[static_cast<int>(TransType)]
                     << " to " << TargetStateName << "\n";
      }
    }
  }
};

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  StateTransitionMapper Mapper;
  MatchFinder Finder;
  Finder.addMatcher(StateTransitionMatcher, &Mapper);

  return Tool.run(newFrontendActionFactory(&Finder).get());
}