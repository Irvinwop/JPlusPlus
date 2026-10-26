#include "standard_module.hpp"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Lex/HeaderSearch.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>

#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>

using namespace clang;

namespace jpp {
namespace {
const std::map<std::string, std::string> cHeaders = {
    {"cassert", "assert.h"}, {"cctype", "ctype.h"},   {"cerrno", "errno.h"},
    {"cfenv", "fenv.h"},     {"cfloat", "float.h"},   {"cinttypes", "inttypes.h"},
    {"climits", "limits.h"}, {"clocale", "locale.h"}, {"cmath", "math.h"},
    {"csetjmp", "setjmp.h"}, {"csignal", "signal.h"}, {"cstdarg", "stdarg.h"},
    {"cstddef", "stddef.h"}, {"cstdint", "stdint.h"}, {"cstdio", "stdio.h"},
    {"cstdlib", "stdlib.h"}, {"cstring", "string.h"}, {"ctime", "time.h"},
    {"cuchar", "uchar.h"},   {"cwchar", "wchar.h"},   {"cwctype", "wctype.h"}};

std::string compatibilityHeaders() {
    std::string result;
    for (const auto& [cpp, c] : cHeaders)
        for (const auto& header : {cpp, c})
            result += "#if __has_include(<" + header + ">)\n#include <" + header + ">\n#endif\n";
    return result;
}

std::string standardHeaders() {
    // The vendor's GNU umbrella includes implementation extensions as well.
    // Enumerate newer standard headers too: older umbrellas omit some of them.
    const char* headers[] = {"bits/stdc++.h",
                             "algorithm",
                             "any",
                             "array",
                             "atomic",
                             "barrier",
                             "bit",
                             "bitset",
                             "cassert",
                             "ccomplex",
                             "cctype",
                             "cerrno",
                             "cfenv",
                             "cfloat",
                             "charconv",
                             "chrono",
                             "cinttypes",
                             "ciso646",
                             "climits",
                             "clocale",
                             "cmath",
                             "codecvt",
                             "compare",
                             "complex",
                             "concepts",
                             "condition_variable",
                             "coroutine",
                             "csetjmp",
                             "csignal",
                             "cstdalign",
                             "cstdarg",
                             "cstdbool",
                             "cstddef",
                             "cstdint",
                             "cstdio",
                             "cstdlib",
                             "cstring",
                             "ctgmath",
                             "ctime",
                             "cuchar",
                             "cwchar",
                             "cwctype",
                             "deque",
                             "exception",
                             "execution",
                             "expected",
                             "filesystem",
                             "flat_map",
                             "flat_set",
                             "format",
                             "forward_list",
                             "fstream",
                             "functional",
                             "future",
                             "generator",
                             "initializer_list",
                             "iomanip",
                             "ios",
                             "iosfwd",
                             "iostream",
                             "istream",
                             "iterator",
                             "latch",
                             "limits",
                             "list",
                             "locale",
                             "map",
                             "mdspan",
                             "memory",
                             "memory_resource",
                             "mutex",
                             "new",
                             "numbers",
                             "numeric",
                             "optional",
                             "ostream",
                             "print",
                             "queue",
                             "random",
                             "ranges",
                             "ratio",
                             "regex",
                             "scoped_allocator",
                             "semaphore",
                             "set",
                             "shared_mutex",
                             "source_location",
                             "span",
                             "spanstream",
                             "sstream",
                             "stack",
                             "stacktrace",
                             "stdexcept",
                             "stdfloat",
                             "stop_token",
                             "streambuf",
                             "string",
                             "string_view",
                             "strstream",
                             "syncstream",
                             "system_error",
                             "thread",
                             "tuple",
                             "type_traits",
                             "typeindex",
                             "typeinfo",
                             "unordered_map",
                             "unordered_set",
                             "utility",
                             "valarray",
                             "variant",
                             "vector",
                             "version"};
    std::string result;
    for (auto header : headers)
        result +=
            "#if __has_include(<" + std::string(header) + ">)\n#include <" + header + ">\n#endif\n";
    return result;
}

struct NamespaceExports {
    bool isInline = false;
    std::set<std::string> names, internalNames;
    std::map<std::string, std::string> aliases;
    std::map<std::string, NamespaceExports> children;
};

void collectExports(const DeclContext* context, NamespaceExports& exports) {
    for (auto declaration : context->decls()) {
        if (auto space = dyn_cast<NamespaceDecl>(declaration)) {
            if (space->isAnonymousNamespace())
                continue;
            auto& child = exports.children[space->getNameAsString()];
            child.isInline = space->isInline();
            collectExports(space, child);
            continue;
        }
        if (auto linkage = dyn_cast<LinkageSpecDecl>(declaration)) {
            collectExports(linkage, exports);
            continue;
        }
        auto named = dyn_cast<NamedDecl>(declaration);
        if (!named || isa<UsingDirectiveDecl>(named))
            continue;
        // Namespace declaration lists also contain lexically written out-of-line
        // members and friends. Export only names qualified lookup can find here.
        if (named->getDeclContext()->getRedeclContext()->getPrimaryContext() !=
            context->getRedeclContext()->getPrimaryContext())
            continue;
        if (auto alias = dyn_cast<NamespaceAliasDecl>(named)) {
            exports.aliases[alias->getNameAsString()] =
                alias->getNamespace()->getQualifiedNameAsString();
            continue;
        }
        if (!named->isInIdentifierNamespace(Decl::IDNS_Ordinary | Decl::IDNS_Tag | Decl::IDNS_Type))
            continue;
        auto name = named->getDeclName();
        if (name.isEmpty() ||
            !(name.isIdentifier() || name.getNameKind() == DeclarationName::CXXOperatorName ||
              name.getNameKind() == DeclarationName::CXXLiteralOperatorName))
            continue;
        const NamedDecl* entity = named;
        if (auto generic = dyn_cast<TemplateDecl>(entity))
            entity = generic->getTemplatedDecl();
        // A qualified using declaration exports its entire overload set; one
        // internal-linkage candidate makes that set ineligible for export.
        if (entity && isa<FunctionDecl, VarDecl>(entity) && !entity->isExternallyVisible())
            exports.internalNames.insert(name.getAsString());
        else
            exports.names.insert(name.getAsString());
    }
}

void collectLibraryNamespaces(const DeclContext* context,
                              std::map<std::string, NamespaceExports>& exports) {
    for (auto declaration : context->decls()) {
        if (auto linkage = dyn_cast<LinkageSpecDecl>(declaration))
            collectLibraryNamespaces(linkage, exports);
        else if (auto space = dyn_cast<NamespaceDecl>(declaration);
                 space && (space->getName() == "std" || space->getName() == "__gnu_cxx"))
            collectExports(space, exports[space->getNameAsString()]);
    }
}

bool cHeaderDeclaration(const NamedDecl* declaration, const SourceManager& sources) {
    auto file = llvm::sys::path::filename(
        sources.getFilename(sources.getExpansionLoc(declaration->getLocation())));
    // These <cstddef> aliases can originate in c++config.h rather than in
    // <cstddef> itself. Include the named types without collecting unrelated
    // implementation declarations from that shared configuration header.
    if (isa<TypedefNameDecl>(declaration)) {
        auto name = declaration->getName();
        if (name == "size_t" || name == "ptrdiff_t" || name == "nullptr_t")
            return true;
    }
    // libstdc++ puts the C abs overloads in a shared implementation header.
    return cHeaders.count(file.str()) || file == "std_abs.h";
}

void collectCNames(const DeclContext* context, const SourceManager& sources,
                   std::set<std::string>& names, bool inStandardNamespace = false) {
    for (auto declaration : context->decls()) {
        if (auto linkage = dyn_cast<LinkageSpecDecl>(declaration)) {
            collectCNames(linkage, sources, names, inStandardNamespace);
            continue;
        }
        if (auto space = dyn_cast<NamespaceDecl>(declaration)) {
            if ((!inStandardNamespace && space->getName() == "std") ||
                (inStandardNamespace && space->isInline()))
                collectCNames(space, sources, names, true);
            continue;
        }
        auto named = dyn_cast<NamedDecl>(declaration);
        if (!inStandardNamespace || !named || !named->getIdentifier() ||
            named->getDeclContext()->getRedeclContext()->getPrimaryContext() !=
                context->getRedeclContext()->getPrimaryContext() ||
            !cHeaderDeclaration(named, sources))
            continue;
        auto name = named->getNameAsString();
        if (!name.empty() && (name[0] != '_' || name == "_Exit"))
            names.insert(name);
    }
}

bool excludedCName(const std::string& name) {
    if (name == "byte" || name == "to_integer" || name == "lerp")
        return true;
    static const std::set<std::string> specialMath = {
        "assoc_laguerre", "assoc_legendre", "beta",         "comp_ellint_1", "comp_ellint_2",
        "comp_ellint_3",  "cyl_bessel_i",   "cyl_bessel_j", "cyl_bessel_k",  "cyl_neumann",
        "ellint_1",       "ellint_2",       "ellint_3",     "expint",        "hermite",
        "laguerre",       "legendre",       "riemann_zeta", "sph_bessel",    "sph_legendre",
        "sph_neumann"};
    return specialMath.count(name) ||
           (name.size() > 1 && (name.back() == 'f' || name.back() == 'l') &&
            specialMath.count(name.substr(0, name.size() - 1)));
}

void writeExports(const NamespaceExports& exports, const std::string& path,
                  llvm::raw_ostream& output) {
    for (const auto& name : exports.names)
        if (!exports.internalNames.count(name))
            output << "using " << path << "::" << name << ";\n";
    for (const auto& [name, child] : exports.children) {
        output << (child.isInline ? "inline " : "") << "namespace " << name << " {\n";
        writeExports(child, path + "::" + name, output);
        output << "}\n";
    }
    for (const auto& [name, target] : exports.aliases)
        output << "namespace " << name << " = ::" << target << ";\n";
}

class ExportConsumer final : public ASTConsumer {
    CompilerInstance& compiler;
    std::string& interface;
    bool& success;
    bool compatibility;
    std::string& standardFile;

  public:
    ExportConsumer(CompilerInstance& instance, std::string& result, bool& ready, bool compat,
                   std::string& provided)
        : compiler(instance), interface(result), success(ready), compatibility(compat),
          standardFile(provided) {}
    void HandleTranslationUnit(ASTContext& context) override {
        if (context.getDiagnostics().hasErrorOccurred())
            return;
        standardFile =
            compiler.getPreprocessor().getHeaderSearchInfo().getPrebuiltModuleFileName("std");
        if (compatibility) {
            std::set<std::string> names;
            collectCNames(context.getTranslationUnitDecl(), context.getSourceManager(), names);
            NamespaceExports global;
            collectExports(context.getTranslationUnitDecl(), global);
            llvm::raw_string_ostream output(interface);
            output << "module;\n"
                   << compatibilityHeaders() << "export module std.compat;\nexport import std;\n";
            for (const auto& name : names)
                if (!excludedCName(name) && !global.internalNames.count(name))
                    output << "export using " << (global.names.count(name) ? "::" : "std::") << name
                           << ";\n";
            success = true;
            return;
        }
        std::map<std::string, NamespaceExports> exports;
        // Some vendor headers put namespace std inside extern "C++" at the
        // translation-unit level, notably the allocation and exception headers.
        collectLibraryNamespaces(context.getTranslationUnitDecl(), exports);
        llvm::raw_string_ostream output(interface);
        output << "module;\n" << standardHeaders() << "export module std;\n";
        // [std.modules] also exports <new>'s global allocation functions.
        for (auto operation :
             {"operator new", "operator new[]", "operator delete", "operator delete[]"})
            output << "export using ::" << operation << ";\n";
        for (const auto& [name, symbols] : exports) {
            output << "export namespace " << name << " {\n";
            writeExports(symbols, name, output);
            output << "}\n";
        }
        success = true;
    }
};

class ExportAction final : public ASTFrontendAction {
    std::string& interface;
    bool& success;
    bool compatibility;
    std::string& standardFile;

  public:
    ExportAction(std::string& result, bool& ready, bool compat, std::string& provided)
        : interface(result), success(ready), compatibility(compat), standardFile(provided) {}
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& compiler,
                                                   llvm::StringRef) override {
        return std::make_unique<ExportConsumer>(compiler, interface, success, compatibility,
                                                standardFile);
    }
};

class ExportFactory final : public tooling::FrontendActionFactory {
    std::string& interface;
    bool& success;
    bool compatibility;
    std::string& standardFile;

  public:
    ExportFactory(std::string& result, bool& ready, bool compat, std::string& provided)
        : interface(result), success(ready), compatibility(compat), standardFile(provided) {}
    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<ExportAction>(interface, success, compatibility, standardFile);
    }
};

class MissingStandardModule final : public TextDiagnosticPrinter {
  public:
    std::string missing;
    MissingStandardModule(llvm::raw_ostream& output, DiagnosticOptions* options)
        : TextDiagnosticPrinter(output, options) {}
    void HandleDiagnostic(DiagnosticsEngine::Level level, const Diagnostic& diagnostic) override {
        if (diagnostic.getID() == diag::err_module_not_found && diagnostic.getNumArgs()) {
            auto kind = diagnostic.getArgKind(0);
            llvm::StringRef name;
            if (kind == DiagnosticsEngine::ak_std_string)
                name = diagnostic.getArgStdStr(0);
            else if (kind == DiagnosticsEngine::ak_c_string)
                name = diagnostic.getArgCStr(0);
            if (name == "std" || name == "std.compat")
                missing = name.str();
        }
        TextDiagnosticPrinter::HandleDiagnostic(level, diagnostic);
    }
};

struct ModuleDirectory {
    llvm::SmallString<128> path;
    ModuleDirectory() {
        if (auto error = llvm::sys::fs::createUniqueDirectory("jpp-module", path))
            throw std::runtime_error("cannot create module directory: " + error.message());
    }
    ~ModuleDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(std::filesystem::path(path.str().str()), ignored);
    }
    std::string file(llvm::StringRef name) const {
        auto result = path;
        llvm::sys::path::append(result, name);
        return result.str().str();
    }
};

void writeFile(const std::string& path, const std::string& contents) {
    std::error_code error;
    llvm::raw_fd_ostream output(path, error);
    if (error)
        throw std::runtime_error("cannot write standard module: " + error.message());
    output << contents;
    output.close();
    if (output.has_error())
        throw std::runtime_error("cannot finish writing standard module");
}

bool buildLibraryModule(const ModuleDirectory& directory, const std::vector<std::string>& args,
                        const std::string& name, std::map<std::string, std::string>& files) {
    bool compatibility = name == "std.compat";
    auto headers = directory.file("headers-" + name + ".cpp");
    writeFile(headers, compatibility ? compatibilityHeaders() : standardHeaders());
    auto libraryArgs = args;
    // These diagnostics describe the generated library interface, not an input
    // declaration. Deprecated library declarations must remain available.
    libraryArgs.insert(libraryArgs.end(), {"-Wno-deprecated", "-Wno-deprecated-declarations",
                                           "-Wno-reserved-module-identifier"});
    tooling::FixedCompilationDatabase scanDatabase(std::filesystem::current_path().string(),
                                                   libraryArgs);
    tooling::ClangTool scan(scanDatabase, {headers});
    std::string interface;
    std::string standardFile;
    bool ready = false;
    ExportFactory exports(interface, ready, compatibility, standardFile);
    if (scan.run(&exports) != 0 || !ready)
        return false;
    auto source = directory.file(name + ".cppm");
    writeFile(source, interface);
    auto moduleArgs = libraryArgs;
    if (compatibility) {
        if (standardFile.empty()) {
            if (!buildLibraryModule(directory, args, "std", files))
                return false;
            standardFile = files.at("std");
        }
        moduleArgs.push_back("-fmodule-file=std=" + standardFile);
        files["std"] = standardFile;
    }
    moduleArgs.insert(moduleArgs.end(), {"-o", directory.file(name + ".pcm")});
    tooling::FixedCompilationDatabase moduleDatabase(std::filesystem::current_path().string(),
                                                     moduleArgs);
    tooling::ClangTool build(moduleDatabase, {source});
    // The ordinary tooling adjuster strips -o; this action needs the actual BMI.
    build.clearArgumentsAdjusters();
    build.appendArgumentsAdjuster(tooling::getClangStripDependencyFileAdjuster());
    auto action = tooling::newFrontendActionFactory<GenerateModuleInterfaceAction>();
    if (build.run(action.get()) != 0)
        return false;
    files[name] = directory.file(name + ".pcm");
    return true;
}
} // namespace

bool withStandardModule(const Options&, const std::vector<std::string>& args,
                        const TranslationAttempt& attempt) {
    auto moduleArgs = args;
    std::unique_ptr<ModuleDirectory> directory;
    std::set<std::string> prepared;
    while (true) {
        llvm::IntrusiveRefCntPtr<DiagnosticOptions> options(new DiagnosticOptions);
        std::string diagnostics;
        llvm::raw_string_ostream messages(diagnostics);
        MissingStandardModule first(messages, options.get());
        bool success = attempt(moduleArgs, &first);
        if (success || first.missing.empty() || !prepared.insert(first.missing).second) {
            llvm::errs() << diagnostics;
            return success;
        }
        if (!directory)
            directory = std::make_unique<ModuleDirectory>();
        std::map<std::string, std::string> files;
        if (!buildLibraryModule(*directory, moduleArgs, first.missing, files)) {
            llvm::errs() << diagnostics;
            return false;
        }
        for (const auto& [name, path] : files)
            moduleArgs.push_back("-fmodule-file=" + name + "=" + path);
    }
}
} // namespace jpp
