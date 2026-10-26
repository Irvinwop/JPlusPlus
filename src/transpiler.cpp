#include "transpiler.hpp"
#include "runtime.hpp"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/ParentMapContext.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/StmtCXX.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Format/Format.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/Lexer.h>
#include <clang/Sema/Lookup.h>
#include <clang/Sema/Sema.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

using namespace clang;
namespace jpp {
namespace {
const std::set<std::string> javaWords = {
    "abstract", "assert", "boolean",    "break",     "byte",       "case",      "catch",
    "char",     "class",  "const",      "continue",  "default",    "do",        "double",
    "else",     "enum",   "extends",    "final",     "finally",    "float",     "for",
    "goto",     "if",     "implements", "import",    "instanceof", "int",       "interface",
    "long",     "native", "new",        "package",   "private",    "protected", "public",
    "return",   "short",  "static",     "strictfp",  "super",      "switch",    "synchronized",
    "this",     "throw",  "throws",     "transient", "try",        "void",      "volatile",
    "while",    "true",   "false",      "null",      "_",          "var",       "yield",
    "record",   "sealed", "permits"};

const std::set<std::string> javaRuntimeNames = {
    // Keep source identifiers from hiding Java library types.
    "Object",
    "String",
    "StringBuilder",
    "CharSequence",
    "Boolean",
    "Byte",
    "Short",
    "Character",
    "Integer",
    "Long",
    "Float",
    "Double",
    "Number",
    "Math",
    "StrictMath",
    "System",
    "Runtime",
    "OutOfMemoryError",
    "InterruptedException",
    "Thread",
    "Class",
    "Arrays",
    "Collections",
    "Comparator",
    "Comparable",
    "Objects",
    "List",
    "ArrayList",
    "Collection",
    "Map",
    "Set",
    "TreeMap",
    "HashMap",
    "TreeSet",
    "NavigableMap",
    "Iterator",
    "Deque",
    "ArrayDeque",
    "PriorityQueue",
    "InputStream",
    "OutputStream",
    "FileInputStream",
    "FileOutputStream",
    "PrintWriter",
    "BufferedWriter",
    "OutputStreamWriter",
    "UncheckedIOException",
    "IOException",
    "ByteArrayOutputStream",
    "Locale",
    "BigInteger",
    "BigDecimal",
    "MathContext",
    "RoundingMode",
    "AutoCloseable",
    "IllegalStateException",
    "UnsupportedOperationException",
    "IllegalArgumentException",
    "IndexOutOfBoundsException",
    "BitSet",
    "ArithmeticException",
    "AbstractList",
    "NoSuchElementException",
    "AssertionError",
    "NumberFormatException"};

std::string join(const std::vector<std::string>& a, std::string_view sep = ", ") {
    std::string s;
    for (const auto& v : a) {
        if (!s.empty())
            s += sep;
        s += v;
    }
    return s;
}

std::string number(const llvm::APSInt& value) {
    llvm::SmallString<40> s;
    value.toString(s, 10);
    return s.str().str();
}

std::string quote(std::string_view s, bool bytes = false) {
    std::string r = "\"";
    for (unsigned char c : s) {
        switch (c) {
        case '\\':
            r += "\\\\";
            break;
        case '"':
            r += "\\\"";
            break;
        case '\n':
            r += "\\n";
            break;
        case '\r':
            r += "\\r";
            break;
        case '\t':
            r += "\\t";
            break;
        default:
            if (c < 32 || c == 127 || (bytes && c > 127)) {
                char b[8];
                snprintf(b, sizeof b, "\\%03o", c);
                r += b;
            } else
                r += static_cast<char>(c);
        }
    }
    return r + '"';
}

std::string eraseTypeArguments(std::string_view type) {
    std::string raw;
    int nesting = 0;
    for (char c : type) {
        if (c == '<')
            ++nesting;
        else if (c == '>')
            --nesting;
        else if (!nesting)
            raw += c;
    }
    return raw;
}

struct Unsupported : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Split the embedded support library at Java declaration boundaries. Strings and
// comments are skipped, so braces in a format string do not affect the structure.
std::vector<std::string> declarations(std::string_view code) {
    std::vector<std::string> parts;
    size_t start = 0;
    int depth = 0, parens = 0;
    for (size_t i = 0; i < code.size(); ++i) {
        char c = code[i];
        if (c == '"' || c == '\'') {
            char q = c;
            while (++i < code.size()) {
                if (code[i] == '\\')
                    ++i;
                else if (code[i] == q)
                    break;
            }
            continue;
        }
        if (c == '/' && i + 1 < code.size() && code[i + 1] == '/') {
            while (i < code.size() && code[i] != '\n')
                ++i;
            continue;
        }
        if (c == '/' && i + 1 < code.size() && code[i + 1] == '*') {
            i += 2;
            while (i + 1 < code.size() && !(code[i] == '*' && code[i + 1] == '/'))
                ++i;
            ++i;
            continue;
        }
        if (c == '{')
            ++depth;
        if (c == '}')
            --depth;
        if (c == '(')
            ++parens;
        if (c == ')')
            --parens;
        if (depth == 0 && parens == 0 && (c == ';' || c == '}')) {
            parts.emplace_back(code.substr(start, i - start + 1));
            start = i + 1;
        }
    }
    return parts;
}

std::set<std::string> words(std::string_view s) {
    std::set<std::string> r;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '"' || s[i] == '\'') {
            char q = s[i++];
            while (i < s.size()) {
                if (s[i] == '\\') {
                    i += 2;
                    continue;
                }
                if (s[i++] == q)
                    break;
            }
            continue;
        }
        if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            while (i < s.size() && s[i] != '\n')
                ++i;
            continue;
        }
        if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            i += 2;
            while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/'))
                ++i;
            i += 2;
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == '$') {
            size_t first = i++;
            while (i < s.size() &&
                   (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == '$'))
                ++i;
            r.emplace(s.substr(first, i - first));
            size_t after = i;
            while (after < s.size() && std::isspace(static_cast<unsigned char>(s[after])))
                ++after;
            if ((after < s.size() && s[after] == '(') ||
                (first >= 2 && s.substr(first - 2, 2) == "::"))
                r.emplace("call:" + std::string(s.substr(first, i - first)));
        } else
            ++i;
    }
    return r;
}

std::string supportFor(const std::string& program, std::string& startup) {
    struct Part {
        std::string code;
        std::set<std::string> defines;
        bool method = false, selected = false;
        bool input = false, output = false;
    };
    std::vector<Part> parts;
    for (auto code : declarations(javaRuntime)) {
        bool inputClass = code.find("static class JppInput ") != std::string::npos;
        bool outputClass = code.find("static class JppOutput ") != std::string::npos;
        if (inputClass || outputClass) {
            auto body = code.substr(code.find('{') + 1);
            body.resize(body.rfind('}'));
            std::string fields;
            for (auto decl : declarations(body)) {
                auto brace = decl.find('{'), paren = decl.find('(');
                if (brace != std::string::npos && paren < brace) {
                    size_t end = paren;
                    while (end && std::isspace(static_cast<unsigned char>(decl[end - 1])))
                        --end;
                    size_t begin = end;
                    while (begin && (std::isalnum(static_cast<unsigned char>(decl[begin - 1])) ||
                                     decl[begin - 1] == '_'))
                        --begin;
                    parts.push_back({decl,
                                     {"call:" + decl.substr(begin, end - begin)},
                                     false,
                                     false,
                                     inputClass,
                                     outputClass});
                } else
                    fields += decl;
            }
            parts.push_back({fields,
                             {inputClass ? "JppInput" : "JppOutput"},
                             false,
                             false,
                             inputClass,
                             outputClass});
        } else if (code.find("static class Jpp {") != std::string::npos) {
            auto body = code.substr(code.find('{') + 1);
            body.resize(body.rfind('}'));
            for (auto method : declarations(body)) {
                auto stat = method.find("static ");
                auto paren = method.find('(', stat);
                if (stat == std::string::npos || paren == std::string::npos)
                    continue;
                size_t end = paren;
                while (end && std::isspace(static_cast<unsigned char>(method[end - 1])))
                    --end;
                size_t begin = end;
                while (begin && (std::isalnum(static_cast<unsigned char>(method[begin - 1])) ||
                                 method[begin - 1] == '_'))
                    --begin;
                parts.push_back(
                    {method, {"call:" + method.substr(begin, end - begin)}, true, false});
            }
        } else {
            std::set<std::string> defines;
            std::smatch match;
            if (std::regex_search(code, match, std::regex(R"(\b(?:class|interface)\s+(\w+))")))
                defines.insert(match[1]);
            else if (code.find("jppCopy(") != std::string::npos)
                defines.insert("jppCopy");
            else {
                if (code.find("jppIn =") != std::string::npos)
                    defines.insert("jppIn");
                if (code.find("jppOut =") != std::string::npos) {
                    defines.insert("jppOut");
                    defines.insert("jppErr");
                    defines.insert("jppLog");
                }
            }
            parts.push_back({code, defines, false, false});
        }
    }
    auto needed = words(program);
    bool changed;
    do {
        changed = false;
        for (auto& part : parts)
            if (!part.selected && std::any_of(part.defines.begin(), part.defines.end(),
                                              [&](const auto& n) { return needed.count(n); })) {
                part.selected = true;
                auto dependencies = words(part.code);
                needed.insert(dependencies.begin(), dependencies.end());
                if (part.defines.count("JppInput") || part.defines.count("JppOutput")) {
                    needed.insert("call:close");
                    needed.insert("JppStreamState");
                }
                changed = true;
            }
    } while (changed);
    std::string result =
        "\n    // Helpers used by this program. Everything stays in this source file.\n";
    for (auto& p : parts)
        if (p.selected && !p.method && !p.input && !p.output) {
            if (p.defines.count("jppIn") || p.defines.count("jppOut"))
                startup += p.code + '\n';
            else
                result += p.code + '\n';
        }
    if (needed.count("JppInput")) {
        result += "\n    static class JppInput extends JppStreamState implements AutoCloseable {\n";
        for (auto& p : parts)
            if (p.selected && p.input)
                result += p.code + '\n';
        result += "\n    }\n";
    }
    if (needed.count("JppOutput")) {
        result +=
            "\n    static class JppOutput extends JppStreamState implements AutoCloseable {\n";
        for (auto& p : parts)
            if (p.selected && p.output)
                result += p.code + '\n';
        result += "\n    }\n";
    }
    bool any =
        std::any_of(parts.begin(), parts.end(), [](auto& p) { return p.selected && p.method; });
    if (any) {
        result += "\n    static class Jpp {\n";
        for (auto& p : parts)
            if (p.selected && p.method)
                result += p.code + '\n';
        result += "\n    }\n";
    }
    return result;
}

std::string formatJava(const std::string& code) {
    auto style = format::getGoogleStyle(format::FormatStyle::LK_Java);
    style.IndentWidth = 4;
    style.ContinuationIndentWidth = 8;
    style.ColumnLimit = 100;
    auto replacements =
        format::reformat(style, code, {{0, static_cast<unsigned>(code.size())}}, "Main.java");
    auto formatted = tooling::applyAllReplacements(code, replacements);
    return formatted ? *formatted : code;
}

class Emitter {
    ASTContext& ctx;
    Sema& sema;
    const Options& options;
    SourceManager& sm;
    std::ostringstream out;
    int indent = 1, serial = 0;
    unsigned expressionDepth = 0;
    std::unordered_map<const NamedDecl*, std::string> names;
    std::map<std::string, int> usedNames;
    std::unordered_map<const FunctionDecl*, std::map<std::string, int>> localNames;
    std::vector<const FunctionDecl*> functions;
    std::vector<const CXXRecordDecl*> records;
    std::vector<const CXXMethodDecl*> defaultedMoves;
    std::unordered_set<const CXXMethodDecl*> requiredDefaultedMoves;
    std::vector<std::pair<const CXXRecordDecl*, size_t>> recordOperationsAt;
    std::vector<const VarDecl*> globals;
    std::vector<const EnumDecl*> enums;
    std::unordered_set<const Decl*> collected;
    std::unordered_set<const FunctionDecl*> emittedFunctions;
    std::unordered_map<const ValueDecl*, std::string> aliases;
    std::unordered_map<const OpaqueValueExpr*, std::string> opaqueValues;
    std::unordered_map<const OpaqueValueExpr*, std::string> opaqueReferences;
    std::unordered_map<const CallExpr*, std::string> checkpointValues;
    std::unordered_map<const Expr*, std::string> arrayBounds;
    std::unordered_set<const CXXBindTemporaryExpr*> localInitializers;
    std::unordered_set<const CXXRecordDecl*> byteRecords;
    std::unordered_map<const CXXRecordDecl*, std::string> recordLayoutNames;
    struct ArrayBoundScope {
        std::unordered_map<const Expr*, std::string>& bounds;
        std::unordered_map<const Expr*, std::string> previous;
        explicit ArrayBoundScope(std::unordered_map<const Expr*, std::string>& bounds)
            : bounds(bounds), previous(bounds) {}
        ~ArrayBoundScope() {
            bounds = std::move(previous);
        }
    };
    std::unordered_set<std::string> reservedNames;
    std::unordered_set<const VarDecl*> cells;
    std::unordered_set<const VarDecl*> references;
    std::unordered_set<const VarDecl*> hoistedVariables;
    std::unordered_set<const VarDecl*> selfInitialized;
    std::unordered_map<const VarDecl*, std::string> staticStorage;
    std::vector<const VarDecl*> staticLocals;
    std::unordered_map<const LabelDecl*, std::string> jumpLabels;
    std::unordered_map<const FunctionProtoType*, std::string> functionTypes;
    std::vector<std::pair<const FunctionProtoType*, std::string>> interfaces;
    std::set<std::string> closureInterfaces;
    struct FunctionPointerField {
        const FunctionDecl* declaration;
        std::string field, javaType;
    };
    std::vector<FunctionPointerField> functionPointers;
    std::unordered_map<const Type*, std::string> constantArrayLayouts;
    std::vector<std::pair<std::string, std::string>> arrayLayoutFields;
    std::map<std::string, std::string> literalArrays;
    std::map<std::string, std::string> extendedConstants;
    std::set<unsigned> tupleArities;
    std::map<std::vector<bool>, std::string> productReferenceKinds;
    bool vectorStorage = false;
    const FunctionDecl* current = nullptr;
    std::vector<const FunctionDecl*> enclosingFunctions;
    const CXXRecordDecl* currentRecord = nullptr;
    std::string constructionThis;
    QualType switchType;
    std::unordered_map<const CaseStmt*, std::string> switchLabels;

    [[noreturn]] void fail(SourceLocation loc, const std::string& message) const {
        auto p = sm.getPresumedLoc(sm.getExpansionLoc(loc));
        if (p.isValid())
            llvm::errs() << p.getFilename() << ':' << p.getLine() << ':' << p.getColumn()
                         << ": error: j++: " << message << '\n';
        else
            llvm::errs() << "j++: error: " << message << '\n';
        throw Unsupported(message);
    }
    [[noreturn]] void fail(const Stmt* s, const std::string& msg) const {
        fail(s ? s->getBeginLoc() : SourceLocation(), msg);
    }
    bool user(const Decl* d) const {
        auto loc = sm.getExpansionLoc(d->getLocation());
        return loc.isValid() && !sm.isInSystemHeader(loc) && !sm.isWrittenInBuiltinFile(loc);
    }
    void line(const std::string& text = "") {
        if (!text.empty())
            out << std::string(indent * 4, ' ') << text;
        out << '\n';
    }
    void source(const Decl* d) {
        if (!options.sourceComments)
            return;
        auto p = sm.getPresumedLoc(sm.getExpansionLoc(d->getLocation()));
        if (p.isValid())
            line("// C++ line " + std::to_string(p.getLine()));
    }
    std::string name(const NamedDecl* d) {
        d = cast<NamedDecl>(d->getCanonicalDecl());
        if (auto it = names.find(d); it != names.end())
            return it->second;
        std::string base = identifier(d->getNameAsString());
        if (base == options.className)
            base = "cpp_" + base;
        if (auto f = dyn_cast<FunctionDecl>(d)) {
            static const std::set<std::string> objectMethods = {
                "getClass", "wait",   "notify", "notifyAll", "toString",
                "hashCode", "equals", "clone",  "finalize"};
            if (!isa<CXXConstructorDecl>(f) && !isa<CXXDestructorDecl>(f) &&
                (objectMethods.count(base) ||
                 (isa<CXXMethodDecl>(f) && (base == "copy" || base == "compareTo"))))
                base = "cpp_" + base;
            if (auto method = dyn_cast<CXXMethodDecl>(f);
                method && base == "close" &&
                scopedDestructor(ctx.getRecordType(method->getParent())))
                base = "cpp_close";
            if (auto method = dyn_cast<CXXMethodDecl>(f);
                method && base == "jppLayout" && byteRecords.count(method->getParent()))
                base = "cpp_jppLayout";
            if (f->isMain())
                base = "cppMain";
            else if (f->isOverloadedOperator()) {
                switch (f->getOverloadedOperator()) {
                case OO_Call:
                    base = "call";
                    break;
                case OO_Less:
                    base = "opLess";
                    break;
                case OO_EqualEqual:
                    base = "opEquals";
                    break;
                case OO_Equal:
                    base = "assign";
                    break;
                case OO_Plus:
                    base = "opPlus";
                    break;
                default:
                    base = "op" + std::to_string(f->getOverloadedOperator());
                }
            }
        }
        if (base.empty())
            base = "anonymous";
        if (auto v = dyn_cast<VarDecl>(d);
            current && ((v && v->isLocalVarDeclOrParm()) || isa<BindingDecl>(d))) {
            bool shadows = std::any_of(names.begin(), names.end(), [&](const auto& entry) {
                if (entry.second != base)
                    return false;
                auto function = dyn_cast<FunctionDecl>(entry.first->getDeclContext());
                return function && std::any_of(enclosingFunctions.begin(), enclosingFunctions.end(),
                                               [&](const FunctionDecl* outer) {
                                                   return outer->getCanonicalDecl() ==
                                                          function->getCanonicalDecl();
                                               });
            });
            if (shadows)
                base = temporary(base + "_lambda");
            int count = ++localNames[current][base];
            if (count > 1)
                base += "_" + std::to_string(count);
        }
        // Member/local scopes retain natural names. Java overload erasure is avoided for functions.
        if (isa<FunctionDecl>(d) || isa<CXXRecordDecl>(d) || isa<EnumConstantDecl>(d) ||
            (isa<VarDecl>(d) && cast<VarDecl>(d)->isFileVarDecl())) {
            int n = ++usedNames[base];
            if (n > 1)
                base += "_" + std::to_string(n);
        }
        return names[d] = base;
    }
    static QualType clean(QualType t) {
        t = t.getNonReferenceType().getUnqualifiedType().getCanonicalType();
        if (auto enumeration = t->getAs<EnumType>())
            return enumeration->getDecl()->getIntegerType().getCanonicalType();
        return t;
    }
    bool longDouble(QualType t) const {
        t = clean(t);
        return t->isRealFloatingType() &&
               &ctx.getFloatTypeSemantics(t) == &llvm::APFloat::x87DoubleExtended();
    }
    std::string extendedConstant(const llvm::APFloat& value) {
        if (value.isNaN() && value.isSignaling())
            fail(SourceLocation(), "signaling long double NaNs need floating-environment lowering");
        llvm::SmallString<48> digits;
        value.toString(digits);
        auto [entry, inserted] = extendedConstants.try_emplace(digits.str().str());
        if (inserted)
            entry->second = temporary("jppLongDouble");
        return options.className + "." + entry->second;
    }
    const CXXRecordDecl* record(QualType t) const {
        return clean(t)->getAsCXXRecordDecl();
    }
    const CXXMethodDecl* lambdaOperator(const CXXRecordDecl* r) {
        auto f = r->getLambdaCallOperator();
        if (auto generic = f->getDescribedFunctionTemplate()) {
            const CXXMethodDecl* selected = nullptr;
            for (auto instance : generic->specializations()) {
                if (instance->getType()->isDependentType() || !instance->hasBody())
                    continue;
                if (selected)
                    fail(r->getLocation(), "generic lambda has multiple concrete call signatures");
                selected = cast<CXXMethodDecl>(instance);
            }
            if (!selected)
                fail(r->getLocation(), "generic lambda has no concrete call signature");
            return selected;
        }
        return f;
    }
    std::string family(QualType t) const {
        auto r = record(t);
        return r && !user(r) ? r->getNameAsString() : "";
    }
    bool iotaView(QualType t) const {
        if (family(t) == "iota_view")
            return true;
        auto arguments =
            family(t) == "view_interface" ? templateArgs(t) : std::vector<TemplateArgument>();
        return arguments.size() == 1 && family(arguments[0].getAsType()) == "iota_view";
    }
    const CXXRecordDecl* iotaPositionOwner(QualType t) const {
        auto r = record(t);
        auto parent = r ? dyn_cast<CXXRecordDecl>(r->getDeclContext()) : nullptr;
        return parent && !user(r) && !user(parent) && parent->getName() == "iota_view" ? parent
                                                                                       : nullptr;
    }
    std::string rangeFactory(QualType t) const {
        auto r = record(t);
        if (!r || user(r))
            return {};
        auto n = r->getQualifiedNameAsString();
        return n == "std::ranges::views::_Iota"    ? "JppIotaFactory"
               : n == "std::ranges::__for_each_fn" ? "JppRangesForEach"
                                                   : "";
    }
    QualType iotaElement(QualType t) {
        if (family(t) == "view_interface")
            t = templateArgs(t)[0].getAsType();
        if (auto owner = iotaPositionOwner(t))
            t = ctx.getRecordType(owner);
        auto arguments = templateArgs(t);
        auto item = arguments[0].getAsType();
        auto bound = arguments[1].getAsType();
        if (!item->isIntegerType() || !bound->isIntegerType())
            fail(SourceLocation(), "this iota value type needs additional range lowering");
        if (ctx.getTypeSize(item) > 64 || ctx.getTypeSize(bound) > 64)
            fail(SourceLocation(), "129-bit iota differences need additional range lowering");
        if (item->isSignedIntegerType() != bound->isSignedIntegerType())
            fail(SourceLocation(), "mixed-signedness iota needs additional range lowering");
        return item;
    }
    std::string iotaDecoder(QualType t) {
        auto argument = temporary("jppOrdinal");
        auto item = iotaElement(t);
        return argument + " -> " + (big(item) ? argument : converted(argument, ctx.Int128Ty, item));
    }
    std::string iotaValueInfo(QualType t) {
        auto item = iotaElement(t);
        return ", " + std::to_string(ctx.getTypeSize(item)) + ", " +
               (item->isSignedIntegerType() ? "true" : "false");
    }
    std::string ordinal(const Expr* e) {
        return big(e->getType()) ? expr(e) : value(e, ctx.Int128Ty);
    }
    std::string makeIota(QualType t, const Expr* first, const Expr* last) {
        if (!last ||
            (!clean(last->getType())->isIntegerType() && !iotaPositionOwner(last->getType())))
            fail(first, "this iota bound needs additional range lowering");
        auto arguments = templateArgs(t);
        if (clean(arguments[0].getAsType())->isSignedIntegerType() !=
            clean(arguments[1].getAsType())->isSignedIntegerType())
            fail(first, "mixed-signedness iota needs additional range lowering");
        auto endpoint = [&](const Expr* value) {
            return iotaPositionOwner(value->getType()) ? expr(value) + ".position" : ordinal(value);
        };
        return "new " + type(t) + "(" + endpoint(first) + ", " + endpoint(last) + ", " +
               iotaDecoder(t) + iotaValueInfo(t) + ")";
    }
    bool isString(QualType t) const {
        return family(t) == "basic_string" || family(t) == "basic_string_view";
    }
    bool jumpBuffer(QualType t) const {
        auto r = record(t);
        return r && !user(r) && r->getQualifiedNameAsString() == "__jmp_buf_tag";
    }
    const CallExpr* checkpointCall(const Expr* e) const {
        auto call = e ? dyn_cast<CallExpr>(bare(e)) : nullptr;
        auto function = call ? call->getDirectCallee() : nullptr;
        if (!function || user(function) || call->getNumArgs() != 1)
            return nullptr;
        auto n = function->getQualifiedNameAsString();
        return n == "_setjmp" || n == "setjmp" ? call : nullptr;
    }
    const CallExpr* checkpointCondition(const Expr* condition) const {
        if (auto direct = checkpointCall(condition))
            return direct;
        auto e = bare(condition);
        if (auto negation = dyn_cast<UnaryOperator>(e);
            negation && negation->getOpcode() == UO_LNot)
            return checkpointCall(negation->getSubExpr());
        if (auto comparison = dyn_cast<BinaryOperator>(e);
            comparison && comparison->isComparisonOp()) {
            if (auto call = checkpointCall(comparison->getLHS());
                call && comparison->getRHS()->isIntegerConstantExpr(ctx))
                return call;
            if (auto call = checkpointCall(comparison->getRHS());
                call && comparison->getLHS()->isIntegerConstantExpr(ctx))
                return call;
        }
        return nullptr;
    }
    const CallExpr* checkpoint(const Stmt* statement) const {
        const CallExpr* result = nullptr;
        if (auto e = dyn_cast_or_null<Expr>(statement))
            result = checkpointCall(e);
        else if (auto branch = dyn_cast_or_null<IfStmt>(statement);
                 branch && !branch->getInit() && !branch->getConditionVariable() &&
                 !branch->isConstexpr() && !branch->isConsteval())
            result = checkpointCondition(branch->getCond());
        else if (auto selection = dyn_cast_or_null<SwitchStmt>(statement);
                 selection && !selection->getInit() && !selection->getConditionVariable())
            result = checkpointCondition(selection->getCond());
        return result && !checkpointValues.count(result) ? result : nullptr;
    }
    bool bitReference(QualType t) const {
        auto r = record(t);
        if (!r || user(r) || r->getName() != "reference")
            return false;
        auto owner = dyn_cast<CXXRecordDecl>(r->getDeclContext());
        return owner && !user(owner) && owner->getName() == "bitset";
    }
    std::string bitsetSize(QualType t) {
        auto bound = templateArgs(t)[0].getAsIntegral();
        if (bound.getActiveBits() > 31)
            fail(SourceLocation(), "bitset bound exceeds Java's indexed collection limit");
        return number(bound);
    }
    bool fileStream(QualType t) const {
        auto n = family(t);
        return n == "basic_ifstream" || n == "basic_ofstream";
    }
    bool isVec(QualType t) const {
        auto n = family(t);
        return n == "vector" || n == "array";
    }
    bool isMap(QualType t) const {
        auto n = family(t);
        return n == "map" || n == "unordered_map";
    }
    bool isSet(QualType t) const {
        auto n = family(t);
        return n == "set" || n == "multiset" || n == "unordered_set" || n == "unordered_multiset";
    }
    bool orderTree(QualType t) const {
        auto r = record(t);
        return r && !user(r) && r->getQualifiedNameAsString() == "__gnu_pbds::tree";
    }
    bool orderTreeIterator(QualType t) const {
        auto r = record(t);
        return r && !user(r) &&
               (r->getQualifiedNameAsString() == "__gnu_pbds::detail::bin_search_tree_const_it_" ||
                r->getQualifiedNameAsString() == "__gnu_pbds::detail::bin_search_tree_it_");
    }
    std::string orderTreeDirection(QualType t) const {
        return templateArgs(t)[6].getAsIntegral().isZero() ? "-1" : "1";
    }
    void requireOrderTree(QualType t) {
        auto args = templateArgs(t);
        auto named = [&](unsigned i, const std::string& expected) {
            auto r = record(args[i].getAsType());
            return r && !user(r) && r->getQualifiedNameAsString() == expected;
        };
        if (args.size() != 6 || !named(1, "__gnu_pbds::null_type") ||
            !named(3, "__gnu_pbds::rb_tree_tag") ||
            args[4].getKind() != TemplateArgument::Template ||
            args[4].getAsTemplate().getAsTemplateDecl()->getQualifiedNameAsString() !=
                "__gnu_pbds::tree_order_statistics_node_update" ||
            !named(5, "std::allocator"))
            fail(SourceLocation(), "this GNU policy-based tree needs additional policy lowering");
        auto comparison = record(args[2].getAsType());
        if (!comparison || user(comparison) ||
            (comparison->getQualifiedNameAsString() != "std::less" &&
             comparison->getQualifiedNameAsString() != "std::greater"))
            fail(SourceLocation(),
                 "custom policy-tree comparators need additional ownership lowering");
        if (ownsDestructor(element(t)))
            fail(SourceLocation(),
                 "policy-tree key destruction needs additional lifetime lowering");
    }
    bool isIter(QualType t) const {
        auto n = family(t);
        return n.find("iterator") != std::string::npos || n == "__normal_iterator" ||
               orderTreeIterator(t);
    }
    bool dataPointer(QualType t) const {
        t = clean(t);
        return t->isPointerType() && !t->getPointeeType()->isFunctionType();
    }
    bool primitiveArray(QualType t) const {
        t = clean(t);
        if (!t->isArrayType())
            return false;
        while (auto a = ctx.getAsArrayType(t))
            t = clean(a->getElementType());
        return (t->isArithmeticType() || t->isEnumeralType()) && !longDouble(t);
    }
    std::string arrayBound(const ArrayType* a) {
        if (auto fixed = dyn_cast<ConstantArrayType>(a))
            return std::to_string(fixed->getSize().getZExtValue());
        if (auto variable = dyn_cast<VariableArrayType>(a)) {
            auto size = variable->getSizeExpr();
            if (auto saved = arrayBounds.find(size); saved != arrayBounds.end())
                return saved->second;
            return "Jpp.arrayBound(" + value(size, ctx.LongLongTy) + ")";
        }
        fail(SourceLocation(), "array bound is not known");
    }
    void captureArrayBounds(QualType t) {
        t = t.getNonReferenceType().getUnqualifiedType();
        if (auto declared = dyn_cast<DecltypeType>(t.getTypePtr()))
            captureExpressionBounds(declared->getUnderlyingExpr());
        if (t->isPointerType()) {
            captureArrayBounds(t->getPointeeType());
            return;
        }
        if (auto a = ctx.getAsArrayType(t)) {
            // GCC evaluates the innermost bound before the containing bounds.
            captureArrayBounds(a->getElementType());
            if (auto variable = dyn_cast<VariableArrayType>(a)) {
                auto size = variable->getSizeExpr();
                if (!arrayBounds.count(size)) {
                    auto bound = temporary("jppLength");
                    captureExpressionBounds(size);
                    auto initial = "Jpp.arrayBound(" + value(size, ctx.LongLongTy) + ")";
                    arrayBounds[size] = bound;
                    line("final int " + bound + " = " + initial + ";");
                }
            }
        }
    }
    void scanArrayBounds(QualType t, bool captureVariables = false) {
        t = t.getNonReferenceType().getUnqualifiedType();
        if (auto declared = dyn_cast<DecltypeType>(t.getTypePtr())) {
            scan(declared->getUnderlyingExpr());
            if (captureVariables)
                markCapturedVariables(declared->getUnderlyingExpr());
        }
        if (t->isPointerType()) {
            scanArrayBounds(t->getPointeeType(), captureVariables);
            return;
        }
        if (auto a = ctx.getAsArrayType(t)) {
            if (auto variable = dyn_cast<VariableArrayType>(a)) {
                scan(variable->getSizeExpr());
                if (captureVariables)
                    markCapturedVariables(variable->getSizeExpr());
            }
            scanArrayBounds(a->getElementType(), captureVariables);
        }
    }
    bool uncapturedArrayBounds(QualType t) {
        if (auto declared = dyn_cast<DecltypeType>(t.getNonReferenceType().getTypePtr()))
            if (expressionBounds(declared->getUnderlyingExpr(), false))
                return true;
        t = clean(t);
        if (t->isPointerType())
            return uncapturedArrayBounds(t->getPointeeType());
        if (auto a = ctx.getAsArrayType(t)) {
            if (auto variable = dyn_cast<VariableArrayType>(a))
                if (!arrayBounds.count(variable->getSizeExpr()))
                    return true;
            return uncapturedArrayBounds(a->getElementType());
        }
        return false;
    }
    // GCC forms variably modified types at the enclosing full expression,
    // including types written in an unselected conditional operand. Only the
    // type's bounds are evaluated; sizeof's value operand remains unevaluated.
    bool expressionBounds(const Stmt* s, bool capture) {
        if (!s || isa<LambdaExpr>(s))
            return false;
        QualType type;
        if (auto cast = dyn_cast<ExplicitCastExpr>(s))
            type = cast->getTypeAsWritten();
        if (auto trait = dyn_cast<UnaryExprOrTypeTraitExpr>(s); trait && trait->isArgumentType())
            type = trait->getTypeOfArgument();
        bool found = !type.isNull() && uncapturedArrayBounds(type);
        if (found && capture)
            captureArrayBounds(type);
        if (auto trait = dyn_cast<TypeTraitExpr>(s))
            for (auto argument : trait->getArgs())
                if (uncapturedArrayBounds(argument->getType())) {
                    found = true;
                    if (capture)
                        captureArrayBounds(argument->getType());
                }
        for (auto child : s->children())
            found = expressionBounds(child, capture) || found;
        return found;
    }
    void captureExpressionBounds(const Stmt* s) {
        expressionBounds(s, true);
    }
    const CXXRecordDecl* byteRecord(QualType t) const {
        auto r = record(t);
        if (r)
            r = r->getDefinition();
        if (!r || !user(r) || !r->isCompleteDefinition() || !r->isStandardLayout() ||
            !r->isTriviallyCopyable() || r->isUnion() || r->isLambda() || r->getNumBases())
            return nullptr;
        for (auto field : r->fields()) {
            auto item = field->getType();
            if (field->isBitField() || item->isReferenceType() ||
                (!item->isIntegerType() && !item->isEnumeralType() &&
                 !(item->isRealFloatingType() &&
                   (ctx.getTypeSize(item) == 32 || ctx.getTypeSize(item) == 64))))
                return nullptr;
        }
        return r;
    }
    std::string memoryType(QualType t) {
        t = clean(t);
        if (auto r = byteRecord(t); r && byteRecords.count(r)) {
            if (auto saved = recordLayoutNames.find(r); saved != recordLayoutNames.end())
                return saved->second;
            auto layout = temporary("jppRecordLayout");
            recordLayoutNames[r] = layout;
            std::vector<std::string> fields;
            const auto& native = ctx.getASTRecordLayout(r);
            unsigned index = 0;
            for (auto field : r->fields()) {
                auto fieldType = memoryType(field->getType());
                auto member = name(field);
                fields.push_back(
                    "new JppMemoryField<" + name(r) + ", " + boxed(field->getType()) + ">(" +
                    quote(member) + ", " +
                    std::to_string(native.getFieldOffset(index++) / ctx.getCharWidth()) + "L, " +
                    fieldType + ", object -> object." + member + ", (object, value) -> object." +
                    member + " = value)");
            }
            auto initializer =
                "JppMemoryType.record(" + std::to_string(native.getSize().getQuantity()) + "L, " +
                targetLittleEndian() + ", () -> new " + name(r) + "((JppUninitialized)null)" +
                (fields.empty() ? "" : ", " + join(fields)) + ")";
            arrayLayoutFields.emplace_back(layout, initializer);
            return layout;
        }
        if (t->isRecordType())
            return {};
        if (auto a = ctx.getAsArrayType(t)) {
            if (auto saved = constantArrayLayouts.find(t.getTypePtr());
                saved != constantArrayLayouts.end())
                return saved->second;
            auto element = memoryType(a->getElementType());
            if (element.empty())
                return "";
            auto layout = "JppMemoryType.array(" + element + ", " + arrayBound(a) + ")";
            if (t->isVariablyModifiedType())
                return layout;
            auto field = temporary("jppArrayLayout");
            constantArrayLayouts[t.getTypePtr()] = field;
            arrayLayoutFields.emplace_back(field, layout);
            return field;
        }
        auto javaType = type(t);
        if (t->isEnumeralType())
            t = t->getAs<EnumType>()->getDecl()->getIntegerType();
        if (t->isBooleanType())
            return "JppMemoryType.BOOL";
        if (javaType == "char")
            return "JppMemoryType.C" + std::to_string(ctx.getTypeSize(t));
        if (t->isIntegerType())
            return std::string("JppMemoryType.") + (t->isSignedIntegerType() ? "I" : "U") +
                   std::to_string(ctx.getTypeSize(t));
        if (t->isRealFloatingType() && (ctx.getTypeSize(t) == 32 || ctx.getTypeSize(t) == 64))
            return "JppMemoryType.F" + std::to_string(ctx.getTypeSize(t));
        return {};
    }
    std::string targetLittleEndian() const {
        return ctx.getTargetInfo().isBigEndian() ? "false" : "true";
    }
    bool chrono(QualType t) const {
        return family(t) == "duration" || family(t) == "time_point";
    }
    std::string chronoPeriod(QualType t) {
        if (family(t) == "time_point")
            t = element(t, 1);
        auto rep = clean(element(t));
        if (!rep->isSignedIntegerType() || ctx.getTypeSize(rep) != 64)
            fail(SourceLocation(),
                 "this chrono representation needs additional arithmetic lowering");
        auto ratio = templateArgs(element(t, 1));
        return number(ratio[0].getAsIntegral()) + "L, " + number(ratio[1].getAsIntegral()) + "L";
    }
    std::string chronoValue(QualType t, const std::string& value, bool duration = false,
                            const std::string& rounding = "cast") {
        auto result =
            duration ? "(" + value + ").convert(" + chronoPeriod(t) + ", " + quote(rounding) + ")"
                     : "new JppDuration(" + value + ", " + chronoPeriod(t) + ")";
        return family(t) == "time_point" ? "new JppTimePoint(" + result + ")" : result;
    }
    bool big(QualType t) const {
        t = clean(t);
        return t->isSpecificBuiltinType(BuiltinType::Int128) ||
               t->isSpecificBuiltinType(BuiltinType::UInt128);
    }
    bool unsignedLong(QualType t) const {
        t = clean(t);
        return t->isUnsignedIntegerType() && ctx.getTypeSize(t) == 64;
    }
    std::vector<TemplateArgument> templateArgs(QualType t) const {
        if (auto r = dyn_cast_or_null<ClassTemplateSpecializationDecl>(record(t)))
            return {r->getTemplateArgs().asArray().begin(), r->getTemplateArgs().asArray().end()};
        return {};
    }
    bool product(QualType t) const {
        return family(t) == "tuple" || family(t) == "pair";
    }
    bool ordering(QualType t) const {
        auto n = family(t);
        return n == "strong_ordering" || n == "weak_ordering" || n == "partial_ordering";
    }
    bool tupleLike(QualType t) const {
        return product(t) || family(t) == "array";
    }
    const CXXDestructorDecl* scopedDestructor(QualType t) const {
        if (t->isReferenceType() || t->isPointerType())
            return nullptr;
        auto r = record(t);
        auto destructor = r && user(r) ? r->getDestructor() : nullptr;
        return destructor && !destructor->isImplicit() && !destructor->isDefaulted() ? destructor
                                                                                     : nullptr;
    }
    bool ownsDestructor(QualType t, std::unordered_set<const Type*>& visited) {
        if (t->isReferenceType() || t->isPointerType())
            return false;
        t = clean(t);
        if (!visited.insert(t.getTypePtr()).second)
            return false;
        if (scopedDestructor(t))
            return true;
        if (auto array = ctx.getAsArrayType(t)) {
            if (auto fixed = dyn_cast<ConstantArrayType>(array); fixed && fixed->getSize().isZero())
                return false;
            return ownsDestructor(array->getElementType(), visited);
        }
        auto n = family(t);
        if (n == "reference_wrapper" || n == "function")
            return false;
        if (n == "array" && templateArgs(t)[1].getAsIntegral().isZero())
            return false;
        if (isVec(t) || n == "deque" || n == "queue" || n == "stack" || n == "priority_queue" ||
            isSet(t) || orderTree(t))
            return ownsDestructor(element(t), visited);
        if (isMap(t))
            return ownsDestructor(element(t), visited) || ownsDestructor(element(t, 1), visited);
        if (product(t))
            for (auto item : productTypes(t))
                if (ownsDestructor(item, visited))
                    return true;
        if (auto r = record(t); r && user(r))
            for (auto field : r->fields())
                if (ownsDestructor(field->getType(), visited))
                    return true;
        return false;
    }
    bool ownsDestructor(QualType t) {
        std::unordered_set<const Type*> visited;
        return ownsDestructor(t, visited);
    }
    bool scopedResource(QualType t) const {
        return fileStream(t) || scopedDestructor(t);
    }
    void markLocalInitializer(const Expr* e) {
        if (!e)
            return;
        if (auto temporary = dyn_cast<CXXBindTemporaryExpr>(e)) {
            localInitializers.insert(temporary);
            markLocalInitializer(temporary->getSubExpr());
        } else if (auto cleanup = dyn_cast<ExprWithCleanups>(e))
            markLocalInitializer(cleanup->getSubExpr());
        else if (auto parens = dyn_cast<ParenExpr>(e))
            markLocalInitializer(parens->getSubExpr());
        else if (auto cast = dyn_cast<CastExpr>(e))
            markLocalInitializer(cast->getSubExpr());
        else if (auto conditional = dyn_cast<ConditionalOperator>(e)) {
            markLocalInitializer(conditional->getTrueExpr());
            markLocalInitializer(conditional->getFalseExpr());
        } else if (auto comma = dyn_cast<BinaryOperator>(e);
                   comma && comma->getOpcode() == BO_Comma)
            markLocalInitializer(comma->getRHS());
    }
    bool containsVector(QualType t, std::unordered_set<const Type*>& visited) {
        if (t->isReferenceType() || t->isPointerType())
            return false;
        t = clean(t);
        if (!visited.insert(t.getTypePtr()).second)
            return false;
        if (family(t) == "vector")
            return true;
        if (family(t) == "array")
            return containsVector(element(t), visited);
        if (auto array = ctx.getAsArrayType(t))
            return containsVector(array->getElementType(), visited);
        if (product(t))
            for (auto item : productTypes(t))
                if (containsVector(item, visited))
                    return true;
        if (auto r = record(t); r && user(r))
            for (auto field : r->fields())
                if (containsVector(field->getType(), visited))
                    return true;
        return false;
    }
    bool containsVector(QualType t) {
        std::unordered_set<const Type*> visited;
        return containsVector(t, visited);
    }
    bool defaultedMove(const FunctionDecl* function) const {
        auto method = dyn_cast_or_null<CXXMethodDecl>(function);
        if (!method || !user(method) || method->getParent()->isLambda() ||
            (!method->isImplicit() && !method->isDefaulted()))
            return false;
        auto constructor = dyn_cast<CXXConstructorDecl>(method);
        return constructor ? constructor->isMoveConstructor() : method->isMoveAssignmentOperator();
    }
    void requireDefaultedMove(const CXXMethodDecl* method) {
        method = method->getCanonicalDecl();
        if (requiredDefaultedMoves.insert(method).second)
            defaultedMoves.push_back(method);
    }
    DeclContext* semanticContext() const {
        if (current)
            return const_cast<FunctionDecl*>(current);
        return ctx.getTranslationUnitDecl();
    }
    std::vector<QualType> productTypes(QualType t) {
        std::vector<QualType> types;
        auto args = templateArgs(t);
        if (family(t) == "array") {
            types.assign(args[1].getAsIntegral().getZExtValue(), args[0].getAsType());
            return types;
        }
        std::function<void(const TemplateArgument&)> append = [&](const TemplateArgument& arg) {
            if (arg.getKind() == TemplateArgument::Pack)
                for (const auto& item : arg.pack_elements())
                    append(item);
            else if (arg.getKind() == TemplateArgument::Type)
                types.push_back(arg.getAsType());
        };
        for (const auto& arg : args)
            append(arg);
        return types;
    }
    std::string productStorageType(QualType t) {
        return t->isReferenceType() ? "JppRef<" + boxed(t) + ">" : boxed(t);
    }
    std::string productField(QualType t, unsigned index) const {
        return family(t) == "pair" ? (index == 0 ? "first" : "second")
                                   : "item" + std::to_string(index);
    }
    std::string productRead(const std::string& object, QualType t, unsigned index) {
        if (family(t) == "array")
            return object + ".get(" + std::to_string(index) + ")";
        auto field = object + "." + productField(t, index);
        return productTypes(t)[index]->isReferenceType() ? field + ".get()" : field;
    }
    std::string productReference(const std::string& object, QualType t, unsigned index) {
        auto item = productTypes(t)[index];
        auto ref = family(t) == "array" ? "JppRef.at(" + object + ", " + std::to_string(index) + ")"
                                        : "Jpp.<" + boxed(item) + ">productReference(" + object +
                                              ", " + std::to_string(index) + ")";
        if (primitiveArray(item))
            ref = "Jpp.arrayReference(" + ref + ", " + memoryType(item) + ", " +
                  targetLittleEndian() + ")";
        return ref;
    }
    std::string productComparisonItem(const std::string& object, QualType t, unsigned index,
                                      QualType& item) {
        item = productTypes(t)[index];
        if (auto array = ctx.getAsArrayType(clean(item))) {
            item = ctx.getPointerType(array->getElementType());
            return "Jpp.<" + boxed(array->getElementType()) + ">arrayDecay((" +
                   productReference(object, t, index) + ").pointer())";
        }
        return productRead(object, t, index);
    }
    bool homogeneousProduct(QualType t) {
        auto elements = productTypes(t);
        return elements.size() >= 16 && !ctx.getAsArrayType(clean(elements[0])) &&
               std::all_of(elements.begin(), elements.end(), [&](QualType element) {
                   return ctx.hasSameType(clean(elements[0]), clean(element));
               });
    }
    unsigned productIndex(const CallExpr* call) {
        auto types = productTypes(call->getArg(0)->getType());
        auto args = call->getDirectCallee()->getTemplateSpecializationArgs();
        if (!args || !args->size())
            fail(call, "cannot determine tuple element selection");
        auto selected = args->get(0);
        if (selected.getKind() == TemplateArgument::Integral)
            return selected.getAsIntegral().getZExtValue();
        if (selected.getKind() == TemplateArgument::Type)
            for (unsigned i = 0; i < types.size(); ++i)
                if (ctx.hasSameType(types[i], selected.getAsType()))
                    return i;
        fail(call, "cannot determine tuple element selection");
    }
    bool productGet(const CallExpr* call) const {
        return call->getDirectCallee() && !user(call->getDirectCallee()) &&
               call->getDirectCallee()->getNameAsString() == "get" && call->getNumArgs() == 1 &&
               tupleLike(call->getArg(0)->getType());
    }
    std::string productConstruct(QualType t, const std::vector<std::string>& values) {
        auto arguments = join(values);
        if (values.size() > 200)
            arguments = "new Object[] {" + arguments + "}, 0";
        std::vector<bool> references;
        for (auto item : productTypes(t))
            references.push_back(item->isReferenceType());
        if (std::any_of(references.begin(), references.end(),
                        [](bool reference) { return reference; })) {
            auto [entry, inserted] = productReferenceKinds.try_emplace(references);
            if (inserted)
                entry->second = temporary("jppReferenceKinds");
            arguments = entry->second + (arguments.empty() ? "" : ", " + arguments);
        }
        return "new " + type(t) + "(" + arguments + ")";
    }
    std::string productArgument(const Expr* e, QualType t) {
        if (t->isReferenceType())
            return family(e->getType()) == "reference_wrapper" && family(t) != "reference_wrapper"
                       ? expr(e)
                       : reference(e);
        if (clean(t)->isRecordType())
            return expr(resolveConstruction(t, {const_cast<Expr*>(e)}, e->getExprLoc()));
        return value(e, t, true);
    }
    OpaqueValueExpr* mappedValue(QualType t, ExprValueKind kind, const std::string& value,
                                 const std::string& location, SourceLocation loc) {
        auto expression = new (ctx) OpaqueValueExpr(loc, t.getNonReferenceType(), kind);
        opaqueValues[expression] = value;
        if (!location.empty())
            opaqueReferences[expression] = location;
        return expression;
    }
    Expr* resolveConstruction(QualType t, std::vector<Expr*> arguments, SourceLocation loc) {
        Sema::ContextRAII context(sema, semanticContext());
        auto result = sema.BuildCXXTypeConstructExpr(ctx.getTrivialTypeSourceInfo(t, loc), loc,
                                                     arguments, loc, false);
        if (result.isInvalid())
            fail(loc, "cannot resolve this element construction");
        return result.get();
    }
    Expr* resolveSwap(Expr* left, Expr* right, SourceLocation loc) {
        Sema::ContextRAII context(sema, semanticContext());
        NamespaceDecl* standard = nullptr;
        for (auto declaration : ctx.getTranslationUnitDecl()->lookup(&ctx.Idents.get("std")))
            if (auto ns = dyn_cast<NamespaceDecl>(declaration))
                standard = ns;
        if (!standard)
            fail(loc, "cannot find the standard swap declarations");
        LookupResult lookup(sema, &ctx.Idents.get("swap"), loc, Sema::LookupOrdinaryName);
        sema.LookupQualifiedName(lookup, standard);
        auto callee = sema.BuildDeclarationNameExpr(CXXScopeSpec(), lookup, true);
        if (callee.isInvalid())
            fail(loc, "cannot resolve tuple element swap");
        std::vector<Expr*> arguments{left, right};
        auto result = sema.BuildCallExpr(nullptr, callee.get(), loc, arguments, loc);
        if (result.isInvalid())
            fail(loc, "cannot resolve tuple element swap");
        return result.get();
    }
    Expr* productElement(const std::string& object, QualType t, unsigned index,
                         ExprValueKind category, SourceLocation loc) {
        auto item = productTypes(t)[index];
        auto elementType = item.getNonReferenceType();
        if (!item->isReferenceType() && t.isConstQualified())
            elementType = ctx.getConstType(elementType);
        auto kind = category == VK_LValue || item->isLValueReferenceType() ? VK_LValue : VK_XValue;
        return mappedValue(elementType, kind, productRead(object, t, index),
                           productReference(object, t, index), loc);
    }
    std::string constructElement(QualType target, const Expr* source) {
        if (auto array = ctx.getAsArrayType(target.getNonReferenceType())) {
            auto sourceArray = ctx.getAsArrayType(source->getType());
            auto elementRef = temporary("jppElement");
            auto item = sourceArray->getElementType();
            auto opaque = mappedValue(item, source->getValueKind(), elementRef + ".get()",
                                      elementRef, source->getExprLoc());
            auto javaType = type(target), raw = eraseTypeArguments(javaType);
            auto dimension = raw.find("[]");
            auto allocated = "new " + raw.substr(0, dimension) + "[" + arrayBound(array) + "]" +
                             raw.substr(dimension + 2);
            if (javaType != raw)
                allocated = "(" + javaType + ") " + allocated;
            return "Jpp.constructArray(" + allocated + ", " + expr(source) + ", (JppRef<" +
                   boxed(item) + "> " + elementRef + ") -> " +
                   constructElement(array->getElementType(), opaque) + ")";
        }
        return expr(resolveConstruction(target, {const_cast<Expr*>(source)}, source->getExprLoc()));
    }
    std::string moveStandardArray(const Expr* source, QualType target) {
        if (templateArgs(target)[1].getAsIntegral().isZero())
            return "new " + type(target) + "(" + expr(source) + ")";
        auto elementRef = temporary("jppElement");
        auto item = element(source->getType());
        if (source->getType().isConstQualified())
            item = ctx.getConstType(item);
        auto opaque =
            mappedValue(item, VK_XValue, elementRef + ".get()", elementRef, source->getExprLoc());
        return "Jpp.constructStandardArray(" + expr(source) + ", (JppRef<" + boxed(item) + "> " +
               elementRef + ") -> " + constructElement(element(target), opaque) + ")";
    }
    std::string assignElement(const Expr* target, const Expr* source) {
        if (auto array = ctx.getAsArrayType(target->getType())) {
            auto sourceArray = ctx.getAsArrayType(source->getType());
            auto a = temporary("jppSourceElement"), b = temporary("jppTargetElement");
            auto from = sourceArray->getElementType(), to = array->getElementType();
            auto right =
                mappedValue(from, source->getValueKind(), a + ".get()", a, source->getExprLoc());
            auto left = mappedValue(to, VK_LValue, b + ".get()", b, target->getExprLoc());
            return "Jpp.assignArray(" + expr(source) + ", " + expr(target) + ", (JppRef<" +
                   boxed(from) + "> " + a + ", JppRef<" + boxed(to) + "> " + b + ") -> { " +
                   assignElement(left, right) + "; })";
        }
        Sema::ContextRAII context(sema, semanticContext());
        auto assignment = sema.BuildBinOp(nullptr, target->getExprLoc(), BO_Assign,
                                          const_cast<Expr*>(target), const_cast<Expr*>(source));
        if (assignment.isInvalid())
            fail(target, "cannot resolve this element assignment");
        return statementValue(assignment.get());
    }
    std::string assignStandardArray(const Expr* target, const Expr* source,
                                    bool asReference = false) {
        if (templateArgs(target->getType())[1].getAsIntegral().isZero())
            return "Jpp.last(" + expr(source) + ", " +
                   (asReference ? reference(target) : expr(target)) + ")";
        auto from = element(source->getType()), to = element(target->getType());
        if (source->getType().isConstQualified())
            from = ctx.getConstType(from);
        auto a = temporary("jppSourceElement"), b = temporary("jppTargetElement");
        auto right =
            mappedValue(from, source->getValueKind(), a + ".get()", a, source->getExprLoc());
        auto left = mappedValue(to, VK_LValue, b + ".get()", b, target->getExprLoc());
        return std::string(asReference ? "Jpp.assignStandardArrayRef("
                                       : "Jpp.assignStandardArray(") +
               expr(source) + ", " + (asReference ? reference(target) : expr(target)) +
               ", (JppRef<" + boxed(from) + "> " + a + ", JppRef<" + boxed(to) + "> " + b +
               ") -> { " + assignElement(left, right) + "; })";
    }
    std::string assignDefaultedMove(const CXXMethodDecl* method, const Expr* target,
                                    const Expr* source, bool asReference = false) {
        requireDefaultedMove(method);
        auto a = temporary("jppSource"), b = temporary("jppTarget");
        return std::string(asReference ? "Jpp.assignUsingRef(" : "Jpp.assignUsing(") +
               expr(source) + ", " + (asReference ? reference(target) : expr(target)) + ", (" + a +
               ", " + b + ") -> " + b + "." + name(method) + "(" + a + "))";
    }
    bool referenceMember(const Expr* e) const {
        auto member = dyn_cast_or_null<MemberExpr>(bare(e));
        if (!member)
            return false;
        auto owner = member->getBase()->getType();
        if (owner->isPointerType())
            owner = owner->getPointeeType();
        return family(owner) == "pair" && member->getMemberDecl()->getType()->isReferenceType();
    }
    std::string convertProduct(const std::string& source, QualType from, QualType to,
                               ExprValueKind category = VK_LValue, SourceLocation loc = {}) {
        auto sourceName = temporary("jppTuple");
        auto sourceTypes = productTypes(from), targetTypes = productTypes(to);
        if (sourceTypes.size() != targetTypes.size())
            fail(SourceLocation(), "tuple conversion has different element counts");
        std::vector<std::string> values;
        for (unsigned i = 0; i < sourceTypes.size(); ++i) {
            auto target = targetTypes[i];
            if (target->isReferenceType())
                values.push_back(family(sourceTypes[i]) == "reference_wrapper" &&
                                         family(target) != "reference_wrapper"
                                     ? productRead(sourceName, from, i)
                                     : productReference(sourceName, from, i));
            else {
                auto item =
                    copyType(target)
                        ? constructElement(target,
                                           productElement(sourceName, from, i, category, loc))
                        : converted(productRead(sourceName, from, i), sourceTypes[i], target);
                values.push_back(item);
            }
        }
        return "Jpp.convertProduct(" + source + ", " + sourceName + " -> " +
               productConstruct(to, values) + ")";
    }
    std::string assignProduct(const Expr* target, const Expr* source) {
        if (family(target->getType()) == "array")
            return assignStandardArray(target, source);
        auto from = source->getType(), to = target->getType();
        auto sourceTypes = productTypes(from), targetTypes = productTypes(to);
        if (sourceTypes.size() != targetTypes.size())
            fail(target, "tuple assignment size mismatch");
        auto a = temporary("jppSource"), b = temporary("jppTarget");
        std::string body;
        for (unsigned i = 0; i < sourceTypes.size(); ++i) {
            auto accepted = targetTypes[i];
            if ((clean(accepted)->isRecordType() || clean(sourceTypes[i])->isRecordType()) &&
                family(accepted) != "_Swallow_assign" && family(accepted) != "reference_wrapper") {
                auto left = productElement(b, to, i, VK_LValue, target->getExprLoc());
                auto right =
                    productElement(a, from, i, source->getValueKind(), source->getExprLoc());
                Sema::ContextRAII context(sema, semanticContext());
                auto assignment =
                    sema.BuildBinOp(nullptr, target->getExprLoc(), BO_Assign, left, right);
                if (assignment.isInvalid())
                    fail(target, "cannot resolve this tuple element assignment");
                body += statementValue(assignment.get()) + "; ";
                continue;
            }
            auto item = productRead(a, from, i);
            if (family(accepted) == "reference_wrapper" &&
                family(sourceTypes[i]) != "reference_wrapper")
                item = productReference(a, from, i);
            else if (family(accepted) != "_Swallow_assign")
                item = converted(item, sourceTypes[i], accepted);
            body += "Jpp.storeProductField(" + b + ", " + std::to_string(i) + ", " + item + "); ";
        }
        return "Jpp.assignProduct(" + expr(source) + ", " + expr(target) + ", (" + a + ", " + b +
               ") -> { " + body + "})";
    }
    std::string swapProduct(const Expr* left, const Expr* right) {
        return supplier([&] {
            auto leftType = left->getType(), rightType = right->getType();
            if (leftType->isPointerType())
                leftType = leftType->getPointeeType();
            if (rightType->isPointerType())
                rightType = rightType->getPointeeType();
            auto a = temporary("jppLeft"), b = temporary("jppRight");
            line("final " + type(leftType) + " " + a + " = " + expr(left) +
                 (left->getType()->isPointerType() ? ".get()" : "") + ";");
            line("final " + type(rightType) + " " + b + " = " + expr(right) +
                 (right->getType()->isPointerType() ? ".get()" : "") + ";");
            auto elements = productTypes(leftType);
            for (unsigned i = 0; i < elements.size(); ++i)
                expressionStatement(
                    resolveSwap(productElement(a, leftType, i, VK_LValue, left->getExprLoc()),
                                productElement(b, rightType, i, VK_LValue, right->getExprLoc()),
                                left->getExprLoc()));
            line("return null;");
        });
    }
    std::string swapArrays(const Expr* left, const Expr* right) {
        return supplier([&] {
            auto t = left->getType();
            auto array = ctx.getAsArrayType(t);
            auto item = array->getElementType();
            auto a = temporary("jppLeft"), b = temporary("jppRight"), index = temporary("jppIndex"),
                 ar = temporary("jppLeftElement"), br = temporary("jppRightElement");
            bool primitive = primitiveArray(t);
            auto storage = primitive ? "JppPtr<" + boxed(item) + ">" : type(t);
            line("final " + storage + " " + a + " = " +
                 (primitive ? arrayDecay(left) : expr(left)) + ";");
            line("final " + storage + " " + b + " = " +
                 (primitive ? arrayDecay(right) : expr(right)) + ";");
            line("for (int " + index + " = 0; " + index + " < " + arrayBound(array) + "; ++" +
                 index + ") {");
            ++indent;
            for (const auto& [reference, object] :
                 std::vector<std::pair<std::string, std::string>>{{ar, a}, {br, b}})
                line("final JppRef<" + boxed(item) + "> " + reference + " = JppRef.at(" +
                     (primitive ? object + ".plus(" + index + ")" : object + ", " + index) + ");");
            expressionStatement(
                resolveSwap(mappedValue(item, VK_LValue, ar + ".get()", ar, left->getExprLoc()),
                            mappedValue(item, VK_LValue, br + ".get()", br, right->getExprLoc()),
                            left->getExprLoc()));
            --indent;
            line("}");
            line("return null;");
        });
    }
    std::string swapObjects(const Expr* left, const Expr* right) {
        return supplier([&] {
            auto t = left->getType().getNonReferenceType();
            auto a = temporary("jppLeft"), b = temporary("jppRight"), saved = temporary("jppSaved");
            line("final " + type(t) + " " + a + " = " + expr(left) + ";");
            line("final " + type(t) + " " + b + " = " + expr(right) + ";");
            auto source = [&](const std::string& object) {
                return mappedValue(t, VK_XValue, object, "JppRef.object(" + object + ")",
                                   left->getExprLoc());
            };
            line(type(t) + " " + saved + " = " +
                 expr(resolveConstruction(t, {source(a)}, left->getExprLoc())) + ";");
            for (const auto& [target, value] :
                 std::vector<std::pair<std::string, std::string>>{{a, b}, {b, saved}}) {
                auto destination = mappedValue(t, VK_LValue, target,
                                               "JppRef.object(" + target + ")", left->getExprLoc());
                Sema::ContextRAII context(sema, ctx.getTranslationUnitDecl());
                auto assignment = sema.BuildBinOp(nullptr, left->getExprLoc(), BO_Assign,
                                                  destination, source(value));
                if (assignment.isInvalid())
                    fail(left, "cannot resolve this swap assignment");
                expressionStatement(assignment.get());
            }
            line("return null;");
        });
    }
    std::string tupleDefinitions() {
        std::string code;
        for (unsigned n : tupleArities) {
            std::vector<std::string> types, params, copies, arguments;
            for (unsigned i = 0; i < n; ++i) {
                auto item = "item" + std::to_string(i), t = "A" + std::to_string(i);
                types.push_back(t);
                params.push_back(t + " " + item);
                copies.push_back("jppCopy(" + item + ")");
                arguments.push_back(item);
            }
            auto name = "JppTuple" + std::to_string(n);
            auto self = name + (n ? "<" + join(types) + ">" : "");
            code += "static class " + self + " implements JppProduct, JppAssignable<" + self +
                    ">, Comparable<" + self + "> {\n";
            for (unsigned i = 0; i < n; ++i)
                code += types[i] + " item" + std::to_string(i) + ";\n";
            code += "final boolean[] referenceKinds;\n";
            auto parameters = n > 200 ? "Object[] values, int marker" : join(params);
            auto forwarded = n > 200 ? "values, marker" : join(arguments);
            code +=
                name + "(" + parameters + ") { this(null" + (n ? ", " + forwarded : "") + "); }\n";
            code += name + "(boolean[] referenceKinds" + (n ? ", " + parameters : "") + ") {\n";
            code += "this.referenceKinds = referenceKinds;\n";
            for (unsigned i = 0; i < n; ++i) {
                auto field = "item" + std::to_string(i);
                code += "this." + field + " = " +
                        (n > 200 ? "(" + types[i] + ")values[" + std::to_string(i) + "]" : field) +
                        ";\n";
            }
            auto copied = n > 200 ? "new Object[] {" + join(copies) + "}, 0" : join(copies);
            code += "}\npublic " + self + " copy() { return new " + self + "(referenceKinds" +
                    (n ? ", " + copied : "") + "); }\n";
            code += "public " + self + " jppAssignFrom(" + self + " source) {\n";
            for (unsigned i = 0; i < n; ++i) {
                auto field = "item" + std::to_string(i);
                code += field + " = (" + types[i] + ")Jpp.assignProductField(" + field +
                        ", Jpp.productValue(source, " + std::to_string(i) + "), binding(" +
                        std::to_string(i) + "));\n";
            }
            code += "return this; }\npublic int arity() { return " + std::to_string(n) + "; }\n";
            code += "public boolean binding(int index) { return referenceKinds != null && "
                    "referenceKinds[index]; }\n";
            code += "public String memberName(int index) { return \"item\" + index; }\n";
            code += "public Object raw(int index) { switch (index) {\n";
            for (unsigned i = 0; i < n; ++i)
                code += "case " + std::to_string(i) + ": return item" + std::to_string(i) + ";\n";
            code += "default: throw new IndexOutOfBoundsException(); }}\n";
            code += "public void raw(int index, Object value) { switch (index) {\n";
            for (unsigned i = 0; i < n; ++i)
                code += "case " + std::to_string(i) + ": item" + std::to_string(i) + " = (" +
                        types[i] + ")value; return;\n";
            code += "default: throw new IndexOutOfBoundsException(); }}\n";
            code += "public int compareTo(" + self +
                    " other) { for (int i=0; i<arity(); ++i) { "
                    "int order = Jpp.compare(Jpp.productValue(this, i), "
                    "Jpp.productValue(other, i)); "
                    "if (order != 0) return order; } return 0; }\n";
            code += "public boolean equals(Object other) { if (!(other instanceof " + name +
                    ")) return false; JppProduct tuple = (JppProduct)other; "
                    "for (int i=0; i<arity(); ++i) if (!Jpp.valueEquals(Jpp.productValue(this, i), "
                    "Jpp.productValue(tuple, i))) return false; return true; }\n";
            code +=
                "public int hashCode() { int result=1; for (int i=0; i<arity(); ++i) "
                "result=31*result+Jpp.valueHash(Jpp.productValue(this, i)); return result; }\n}\n";
        }
        return code;
    }
    QualType element(QualType t, unsigned i = 0) {
        if (family(t) == "tuple")
            return productTypes(t).at(i);
        auto a = templateArgs(t);
        if (i < a.size() && a[i].getKind() == TemplateArgument::Type)
            return a[i].getAsType();
        fail(SourceLocation(), "cannot determine element type of " + t.getAsString());
    }
    QualType iteratorElement(QualType t) {
        t = clean(t);
        if (auto a = ctx.getAsArrayType(t))
            return a->getElementType();
        if (t->isPointerType())
            return t->getPointeeType();
        if (orderTreeIterator(t))
            return element(t, 1);
        auto n = family(t);
        if (n == "back_insert_iterator" || n == "front_insert_iterator" || n == "insert_iterator")
            return element(element(t));
        auto inner = element(t);
        if (n == "reverse_iterator" || n == "move_iterator")
            return iteratorElement(inner);
        return inner->isPointerType() ? inner->getPointeeType() : inner;
    }
    const FunctionProtoType* callableType(QualType t) {
        t = clean(t);
        if (t->isPointerType())
            t = t->getPointeeType();
        if (auto f = t->getAs<FunctionProtoType>())
            return f;
        if (family(t) == "function")
            return element(t)->getAs<FunctionProtoType>();
        if (auto r = record(t)) {
            if (r->isLambda())
                return lambdaOperator(r)->getType()->getAs<FunctionProtoType>();
            for (auto method : r->methods())
                if (method->isOverloadedOperator() && method->getOverloadedOperator() == OO_Call &&
                    !method->getDescribedFunctionTemplate())
                    return method->getType()->getAs<FunctionProtoType>();
        }
        fail(SourceLocation(), "cannot determine callable signature of " + t.getAsString());
    }
    std::string callableMethod(QualType t) {
        if (family(t) == "less" || family(t) == "greater")
            return "test";
        if (auto r = record(t); r && user(r) && !r->isLambda())
            for (auto method : r->methods())
                if (method->isOverloadedOperator() && method->getOverloadedOperator() == OO_Call)
                    return name(method);
        return "call";
    }
    std::string boxed(QualType t) {
        auto s = type(t);
        static const std::map<std::string, std::string> boxes = {
            {"int", "Integer"}, {"long", "Long"},   {"boolean", "Boolean"}, {"char", "Character"},
            {"byte", "Byte"},   {"short", "Short"}, {"double", "Double"},   {"float", "Float"}};
        auto it = boxes.find(s);
        return it == boxes.end() ? s : it->second;
    }
    std::string type(QualType input) {
        auto t = clean(input);
        if (t->isVoidType())
            return "void";
        if (t->isNullPtrType())
            return "JppPtr<?>";
        if (t->isBooleanType())
            return "boolean";
        if (t->isCharType() || t->isChar16Type() ||
            (t->isWideCharType() && ctx.getTypeSize(t) <= 16))
            return "char";
        if (t->isEnumeralType())
            return "int";
        if (big(t))
            return "BigInteger";
        if (t->isIntegerType())
            return ctx.getTypeSize(t) > 32 ? "long" : "int";
        if (t->isRealFloatingType()) {
            if (longDouble(t))
                return "JppLongDouble";
            if (ctx.getTypeSize(t) != 32 && ctx.getTypeSize(t) != 64)
                fail(SourceLocation(),
                     "extended floating-point types need a software precision backend");
            return ctx.getTypeSize(t) <= 32 ? "float" : "double";
        }
        if (auto a = ctx.getAsArrayType(t))
            return type(a->getElementType()) + "[]";
        if (t->isFunctionType())
            return functionType(t->getAs<FunctionProtoType>());
        if (t->isPointerType()) {
            auto p = t->getPointeeType();
            if (p->isFunctionType())
                return functionType(p->getAs<FunctionProtoType>());
            return "JppPtr<" + (p->isVoidType() ? std::string("?") : boxed(p)) + ">";
        }
        auto n = family(t);
        if (jumpBuffer(t))
            return "JppJumpBuffer";
        if (iotaView(t))
            return "JppIota<" + boxed(iotaElement(t)) + ">";
        if (iotaPositionOwner(t))
            return "JppIotaIter<" + boxed(iotaElement(t)) + ">";
        if (n == "in_fun_result")
            return "JppRangeResult<" + boxed(element(t)) + ", " + boxed(element(t, 1)) + ">";
        if (auto factory = rangeFactory(t); !factory.empty())
            return factory;
        if (chrono(t)) {
            chronoPeriod(t);
            return n == "duration" ? "JppDuration" : "JppTimePoint";
        }
        if (ordering(t))
            return "JppOrdering";
        if (n == "basic_ifstream" || n == "basic_istream")
            return "JppInput";
        if (n == "basic_ofstream" || n == "basic_ostream")
            return "JppOutput";
        if (auto r = record(t); r && r->isLambda())
            return functionType(lambdaOperator(r)->getType()->getAs<FunctionProtoType>());
        if (isString(t))
            return "JppString";
        if (family(t) == "bitset")
            return "JppBitset";
        if (n == "mersenne_twister_engine") {
            auto arguments = templateArgs(t);
            auto width = arguments[1].getAsIntegral().getZExtValue();
            if (width < 2 || width > 64 ||
                arguments[2].getAsIntegral().getZExtValue() > 2147483647ULL)
                fail(SourceLocation(), "this Mersenne Twister configuration needs wider storage");
            return "JppMtEngine";
        }
        if (n == "seed_seq")
            return "JppSeedSeq";
        if (bitReference(t))
            return "JppBitRef";
        if (isVec(t))
            return "ArrayList<" + boxed(element(t)) + ">";
        if (n == "reference_wrapper")
            return "JppRef<" + boxed(element(t)) + ">";
        if (n == "_Swallow_assign")
            return "JppIgnore";
        if (n == "tuple") {
            auto elements = productTypes(t);
            tupleArities.insert(elements.size());
            std::vector<std::string> arguments;
            for (auto item : elements)
                arguments.push_back(productStorageType(item));
            return "JppTuple" + std::to_string(elements.size()) +
                   (elements.empty() ? "" : "<" + join(arguments) + ">");
        }
        if (n == "pair")
            return "JppPair<" + productStorageType(element(t)) + ", " +
                   productStorageType(element(t, 1)) + ">";
        if (isMap(t))
            return "JppMap<" + boxed(element(t)) + ", " + boxed(element(t, 1)) + ">";
        if (isSet(t))
            return "JppSet<" + boxed(element(t)) + ">";
        if (orderTree(t)) {
            requireOrderTree(t);
            return "JppOrderSet<" + boxed(element(t)) + ">";
        }
        if (n == "deque")
            return "JppDeque<" + boxed(element(t)) + ">";
        if (n == "queue" || n == "stack")
            return "ArrayDeque<" + boxed(element(t)) + ">";
        if (n == "priority_queue")
            return "PriorityQueue<" + boxed(element(t)) + ">";
        if (n == "function")
            return functionType(element(t)->getAs<FunctionProtoType>());
        if (n == "less" || n == "greater") {
            auto argument = element(t);
            auto value = argument->isVoidType() ? "Object" : boxed(argument);
            return "java.util.function.BiPredicate<" + value + ", " + value + ">";
        }
        if (isIter(t)) {
            return "JppIter<" + boxed(iteratorElement(t)) + ">";
        }
        if (auto r = record(t); r && user(r))
            return name(r);
        fail(SourceLocation(), "unsupported type '" + input.getAsString() + "'");
    }
    std::string functionType(const FunctionProtoType* f) {
        if (!f)
            fail(SourceLocation(), "unsupported function type");
        // Method cv/ref qualifiers do not change the callable Java interface.
        f = cast<FunctionProtoType>(ctx.getFunctionType(f->getReturnType(), f->getParamTypes(),
                                                        FunctionProtoType::ExtProtoInfo())
                                        .getCanonicalType()
                                        .getTypePtr());
        if (functionTypes.count(f))
            return functionTypes[f];
        auto n = "JppFn" + std::to_string(functionTypes.size());
        functionTypes[f] = n;
        interfaces.emplace_back(f, n);
        return n;
    }
    std::string functionPointer(const FunctionDecl* f, QualType target) {
        f = f->getCanonicalDecl();
        if (!user(f))
            fail(f->getLocation(), "this library function pointer needs a Java adapter");
        for (const auto& entry : functionPointers)
            if (entry.declaration == f)
                return entry.field;
        auto field = temporary("jppFunction");
        functionPointers.push_back({f, field, type(target)});
        return field;
    }
    std::string mtEngine(QualType t, const std::string& seed = "5489L") {
        auto arguments = templateArgs(t);
        std::vector<std::string> values;
        for (unsigned i = 1; i < arguments.size(); ++i) {
            bool word = i == 5 || i == 7 || i == 9 || i == 11 || i == 13;
            values.push_back(word ? integerLiteral(arguments[i].getAsIntegral().zextOrTrunc(64),
                                                   ctx.UnsignedLongLongTy)
                                  : number(arguments[i].getAsIntegral()));
        }
        values.push_back(seed);
        return "new " + type(t) + "(" + join(values) + ")";
    }
    std::string defaultValue(QualType t) {
        if (t->isReferenceType())
            t = t.getNonReferenceType();
        if (t->isNullPtrType())
            return "null";
        if (ctx.getAsArrayType(t))
            return newArray(t, nullptr);
        auto jt = type(t);
        if (longDouble(t))
            return "JppLongDouble.ZERO";
        if (jt == "boolean")
            return "false";
        if (jt == "char")
            return "'\\0'";
        if (jt == "long")
            return "0L";
        if (jt == "float")
            return "0.0f";
        if (jt == "double")
            return "0.0";
        if (jt == "int")
            return "0";
        if (big(t))
            return "BigInteger.ZERO";
        if (fileStream(t))
            return "new " + type(t) + "((String)null)";
        if (t->isPointerType() || family(t) == "function")
            return "null";
        if (family(t) == "bitset")
            return "new JppBitset(" + bitsetSize(t) + ")";
        if (family(t) == "mersenne_twister_engine")
            return mtEngine(t);
        if (family(t) == "seed_seq")
            return "new JppSeedSeq()";
        if (orderTreeIterator(t))
            return "new JppOrderIter<" + boxed(iteratorElement(t)) + ">(null, null, " +
                   orderTreeDirection(t) + ")";
        if (family(t) == "vector" && vectorStorage)
            return "new JppVector<" + boxed(element(t)) + ">()";
        if (chrono(t))
            return chronoValue(t, "0L");
        if (family(t) == "less" || family(t) == "greater")
            return element(t)->isVoidType()
                       ? "(" + jt + ") ((a, b) -> Jpp.compare(a, b) " +
                             (family(t) == "less" ? "<" : ">") + " 0)"
                       : comparisonPredicate(element(t), element(t), family(t) == "greater");
        if (isMap(t))
            return "new " + jt + "(" + (family(t) == "map" ? "true" : "false") + ", () -> " +
                   defaultValue(element(t, 1)) + ", Jpp.comparator(" +
                   (family(t) == "map" ? comparatorPredicate(templateArgs(t)[2].getAsType(),
                                                             element(t), element(t))
                                       : comparisonPredicate(element(t), element(t))) +
                   "))";
        if (orderTree(t))
            return "new " + jt + "(Jpp.comparator(" +
                   comparatorPredicate(templateArgs(t)[2].getAsType(), element(t), element(t)) +
                   "))";
        if (isSet(t))
            return "new " + jt + "(" +
                   ((family(t) == "multiset" || family(t) == "unordered_multiset") ? "true"
                                                                                   : "false") +
                   ", Jpp.comparator(" +
                   (family(t).starts_with("unordered_")
                        ? comparisonPredicate(element(t), element(t))
                        : comparatorPredicate(templateArgs(t)[1].getAsType(), element(t),
                                              element(t))) +
                   "))";
        if (family(t) == "priority_queue") {
            auto args = templateArgs(t);
            auto comparator = "Jpp.heapComparator(" +
                              comparatorPredicate(args[2].getAsType(), element(t), element(t)) +
                              ")";
            return "new " + jt + "(" + comparator + ")";
        }
        if (family(t) == "array") {
            auto a = templateArgs(t);
            if (a[1].getAsIntegral().isZero())
                return "new " + jt + "()";
            return "Jpp.vectorDefault(" + number(a[1].getAsIntegral()) + ", () -> " +
                   defaultValue(element(t)) + ")";
        }
        if (product(t)) {
            std::vector<std::string> elements;
            for (auto item : productTypes(t))
                elements.push_back(defaultValue(item));
            return productConstruct(t, elements);
        }
        if (family(t) == "_Swallow_assign")
            return "new JppIgnore()";
        if (iotaView(t))
            return "new " + type(t) + "(BigInteger.ZERO, BigInteger.ZERO, " + iotaDecoder(t) +
                   iotaValueInfo(t) + ")";
        if (iotaPositionOwner(t))
            return "new " + type(t) + "(BigInteger.ZERO, " + iotaDecoder(t) + iotaValueInfo(t) +
                   ")";
        if (family(t) == "in_fun_result")
            return "new " + type(t) + "(" + defaultValue(element(t)) + ", " +
                   defaultValue(element(t, 1)) + ")";
        if (auto r = record(t); r && user(r))
            for (auto constructor : r->ctors())
                if (constructor->isDefaultConstructor() && constructor->getNumParams()) {
                    std::vector<std::string> arguments;
                    for (auto parameter : constructor->parameters()) {
                        auto argument = parameter->getDefaultArg();
                        auto target = parameter->getType();
                        arguments.push_back(referenceABI(target) ? reference(argument)
                                                                 : value(argument, target, true));
                    }
                    return "new " + jt + "(" + join(arguments) + ")";
                }
        return "new " + jt + "()";
    }
    const Expr* strip(const Expr* e) const {
        while (e) {
            if (auto x = dyn_cast<ExprWithCleanups>(e))
                e = x->getSubExpr();
            else if (auto x = dyn_cast<MaterializeTemporaryExpr>(e))
                e = x->getSubExpr();
            else if (auto x = dyn_cast<CXXBindTemporaryExpr>(e))
                e = x->getSubExpr();
            else if (auto x = dyn_cast<ParenExpr>(e))
                e = x->getSubExpr();
            else if (auto x = dyn_cast<ConstantExpr>(e))
                e = x->getSubExpr();
            else if (auto x = dyn_cast<CXXDefaultArgExpr>(e))
                e = x->getExpr();
            else if (auto x = dyn_cast<CXXDefaultInitExpr>(e))
                e = x->getExpr();
            else
                break;
        }
        return e;
    }
    const Expr* bare(const Expr* e) const {
        return strip(e ? e->IgnoreParenImpCasts() : nullptr);
    }
    bool copyType(QualType t) const {
        return !t->isReferenceType() && (t->isRecordType() || ctx.getAsArrayType(t));
    }
    std::string integerResult(const Expr* e, const std::string& result) {
        auto t = clean(e->getType());
        return t->isIntegerType() && !big(t) && ctx.getTypeSize(t) > 32 ? "(long)(" + result + ")"
                                                                        : result;
    }
    std::string integerLiteral(const llvm::APInt& value, QualType target) {
        target = clean(target);
        if (target->isBooleanType())
            return value.isZero() ? "false" : "true";
        if (big(target)) {
            llvm::SmallString<40> digits;
            value.toString(digits, 10, !target->isUnsignedIntegerType());
            return "new BigInteger(" + quote(digits.str().str()) + ")";
        }
        if (ctx.getTypeSize(target) > 32)
            return std::to_string(value.getSExtValue()) + "L";
        if (type(target) == "char") {
            auto mask = ctx.getTypeSize(target) == 8 ? 255 : 65535;
            return "(char)" + std::to_string(value.getZExtValue() & mask);
        }
        return std::to_string(target->isUnsignedIntegerType()
                                  ? static_cast<int32_t>(value.getZExtValue())
                                  : static_cast<int32_t>(value.getSExtValue()));
    }
    std::string converted(std::string s, QualType from, QualType target) {
        from = clean(from);
        target = clean(target);
        if (orderTreeIterator(from) && orderTreeIterator(target) && !ctx.hasSameType(from, target))
            return "Jpp.orderIterator(" + s + ", " + orderTreeDirection(target) + ")";
        bool borrowed =
            family(from) == "reference_wrapper" && family(target) != "reference_wrapper";
        if (borrowed) {
            s += ".get()";
            from = clean(element(from));
        }
        if (family(target) == "function" && !from->isNullPtrType() &&
            (borrowed || type(from) != type(target))) {
            auto source = callableType(from), destination = callableType(target);
            if (source->getNumParams() != destination->getNumParams())
                fail(SourceLocation(), "callable conversion needs argument-pack lowering");
            return supplier([&] {
                auto function = temporary("jppCallable");
                line("final " + type(from) + " " + function + " = " + s + ";");
                std::vector<std::string> parameters, arguments;
                for (unsigned i = 0; i < destination->getNumParams(); ++i) {
                    auto parameter = temporary("jppArgument");
                    auto accepted = destination->getParamType(i), needed = source->getParamType(i);
                    parameters.push_back(parameterType(accepted) + " " + parameter);
                    auto argument = referenceABI(accepted) ? parameter + ".get()" : parameter;
                    if (referenceABI(needed) && referenceABI(accepted) &&
                        ctx.hasSameType(clean(accepted), clean(needed)))
                        argument = parameter;
                    else {
                        argument = converted(argument, accepted, needed);
                        if (referenceABI(needed))
                            argument = "JppRef.cell(" + argument + ")";
                        else if (copyType(needed))
                            argument = "jppCopy(" + argument + ")";
                    }
                    arguments.push_back(argument);
                }
                auto invocation =
                    function + "." + callableMethod(from) + "(" + join(arguments) + ")";
                std::string body;
                if (destination->getReturnType()->isVoidType())
                    body = "{ " + invocation + "; }";
                else if (referenceABI(destination->getReturnType())) {
                    if (!referenceABI(source->getReturnType()) ||
                        !ctx.hasSameType(clean(source->getReturnType()),
                                         clean(destination->getReturnType())))
                        fail(SourceLocation(), "this callable reference-result conversion needs "
                                               "additional lifetime lowering");
                    body = invocation;
                } else {
                    if (referenceABI(source->getReturnType()))
                        invocation += ".get()";
                    body = converted(invocation, source->getReturnType(),
                                     destination->getReturnType());
                }
                line("return (" + type(target) + ") ((" + join(parameters) + ") -> " + body + ");");
            });
        }
        if (product(target) && tupleLike(from) && !ctx.hasSameType(from, target))
            return convertProduct(s, from, target);
        if (target->isBooleanType())
            return booleanValue(s, from);
        if (longDouble(target)) {
            if (longDouble(from))
                return s;
            if (from->isRealFloatingType())
                return "JppLongDouble.floating(" + s + ")";
            return "JppLongDouble.integer(" + converted(s, from, ctx.Int128Ty) + ")";
        }
        if (longDouble(from) && (target->isArithmeticType() || target->isEnumeralType())) {
            if (target->isRealFloatingType())
                return "(" + s + ")." +
                       (target->isSpecificBuiltinType(BuiltinType::Float) ? "floatValue()"
                                                                          : "doubleValue()");
            return converted("(" + s + ").integerValue()", ctx.Int128Ty, target);
        }
        if (from->isRealFloatingType() && target->isUnsignedIntegerType())
            return converted("Jpp.floatingInteger(" + s + ")", ctx.Int128Ty, target);
        if (from->isCharType() && ctx.getTypeSize(from) == 8)
            s = "(char)((int)(char)(" + s + ") & 255)";
        if (from->isBooleanType() && (target->isArithmeticType() || target->isEnumeralType())) {
            s = "(" + s + " ? 1 : 0)";
            from = ctx.IntTy;
        }
        if (from->isCharType() && from->isSignedIntegerType() && ctx.getTypeSize(from) == 8 &&
            (target->isArithmeticType() || target->isEnumeralType()) && !target->isCharType()) {
            s = "((int)(byte)(char)(" + s + "))";
            from = ctx.IntTy;
        }
        if (from->isUnsignedIntegerType() && ctx.getTypeSize(from) == 32 &&
            (target->isRealFloatingType() ||
             (target->isIntegerType() && ctx.getTypeSize(target) > 32))) {
            s = "Integer.toUnsignedLong(" + s + ")";
            from = ctx.UnsignedLongLongTy;
        }
        if (unsignedLong(from) && target->isRealFloatingType()) {
            bool single = target->isSpecificBuiltinType(BuiltinType::Float);
            s = std::string(single ? "Jpp.unsignedFloat(" : "Jpp.unsignedDouble(") + s + ")";
            from = single ? ctx.FloatTy : ctx.DoubleTy;
        }
        if (big(target) && !big(from))
            s = std::string(from->isRealFloatingType() ? "Jpp.floatingInteger("
                            : unsignedLong(from)       ? "Jpp.unsignedBigInteger("
                                                       : "BigInteger.valueOf(") +
                s + ")";
        else if (!big(target) && big(from)) {
            auto jt = type(target);
            s = "(" + s + ")." +
                (jt == "long"     ? "longValue"
                 : jt == "double" ? "doubleValue"
                 : jt == "float"  ? "floatValue"
                                  : "intValue") +
                "()";
            if (jt == "char")
                s = "(char)(" + s + ")";
        } else if ((target->isArithmeticType() || target->isEnumeralType()) &&
                   (from->isArithmeticType() || from->isEnumeralType()) &&
                   type(target) != type(from)) {
            auto source = type(from), destination = type(target);
            static const std::map<std::string, int> rank = {
                {"char", 0}, {"int", 1}, {"long", 2}, {"float", 3}, {"double", 4}};
            // Java cannot unbox a wrapper and narrow its primitive value in the same cast.
            if (rank.at(destination) < rank.at(source))
                s = "(" + source + ")(" + s + ")";
            s = "(" + destination + ")(" + s + ")";
        }
        if (target->isCharType() && ctx.getTypeSize(target) == 8)
            s = "(char)((int)(" + s + ") & 255)";
        if (target->isIntegerType() && type(target) == "int" && ctx.getTypeSize(target) < 32 &&
            !ctx.hasSameType(from, target)) {
            auto width = ctx.getTypeSize(target);
            if (target->isUnsignedIntegerType())
                s = "((int)(" + s + ") & " + std::to_string((1ULL << width) - 1) + ")";
            else
                s = "(int)(" + std::string(width == 8 ? "byte" : "short") + ")(int)(" + s + ")";
        }
        return s;
    }
    std::string value(const Expr* e, QualType target, bool copy = false) {
        if (dataPointer(target))
            return "Jpp.pointer(" + expr(e) + ")";
        auto s = expr(e);
        if (isString(target) && !isString(e->getType()))
            s = "new JppString(Jpp.cString(" + s + "))";
        else
            s = converted(s, e->getType(), target);
        if (copy && copyType(target) && e->isLValue())
            s = "jppCopy(" + s + ")";
        return s;
    }
    std::string intArgument(const Expr* e) {
        // Java indices and sizes use 32 bits. Wider intermediate integral casts preserve them.
        while (auto cast = dyn_cast<ImplicitCastExpr>(strip(e))) {
            if (cast->getCastKind() != CK_IntegralCast || ctx.getTypeSize(cast->getType()) < 32 ||
                !cast->getSubExpr()->getType()->isIntegerType())
                break;
            e = cast->getSubExpr();
        }
        return converted(expr(e), e->getType(), ctx.IntTy);
    }
    bool referenceABI(QualType t) const {
        return t->isReferenceType() &&
               (clean(t)->isArithmeticType() || clean(t)->isEnumeralType() || dataPointer(t) ||
                primitiveArray(t) || family(t) == "reference_wrapper");
    }
    std::string parameterType(QualType t) {
        return referenceABI(t) ? "JppRef<" + boxed(t) + ">" : type(t);
    }
    const VarDecl* variableRef(const Expr* e) const {
        if (auto r = dyn_cast_or_null<DeclRefExpr>(bare(e)))
            return dyn_cast<VarDecl>(r->getDecl());
        return nullptr;
    }
    bool cell(const VarDecl* v) const {
        return v && (cells.count(v) || references.count(v));
    }
    std::string temporary(std::string_view prefix) {
        std::string result;
        do {
            result = std::string(prefix) + std::to_string(++serial);
        } while (!reservedNames.insert(result).second);
        return result;
    }
    void markReference(const Expr* e) {
        if (auto update = dyn_cast_or_null<UnaryOperator>(bare(e));
            update && update->isPrefix() && update->isIncrementDecrementOp())
            markReference(update->getSubExpr());
        if (auto binary = dyn_cast_or_null<BinaryOperator>(bare(e))) {
            if (binary->isAssignmentOp())
                markReference(binary->getLHS());
            if (binary->getOpcode() == BO_Comma) {
                markReference(binary->getRHS());
                markCapturedVariables(binary);
            }
        }
        if (auto c = dyn_cast_or_null<ConditionalOperator>(bare(e))) {
            markReference(c->getTrueExpr());
            markReference(c->getFalseExpr());
        }
        if (auto c = dyn_cast_or_null<BinaryConditionalOperator>(bare(e))) {
            markReference(c->getCommon());
            markReference(c->getFalseExpr());
        }
        if (auto c = dyn_cast_or_null<CallExpr>(bare(e));
            c && c->getDirectCallee() && !user(c->getDirectCallee())) {
            auto n = c->getDirectCallee()->getNameAsString();
            if ((n == "move" || n == "forward" || n == "as_const") && c->getNumArgs())
                markReference(c->getArg(0));
            if ((n == "min" || n == "max") && c->getNumArgs() >= 2 &&
                c->getDirectCallee()->getReturnType()->isReferenceType()) {
                markReference(c->getArg(0));
                markReference(c->getArg(1));
            }
        }
        if (auto v = variableRef(e); v && !v->hasGlobalStorage())
            cells.insert(v);
    }
    void markCapturedVariables(const Stmt* s) {
        if (!s)
            return;
        if (isa<LambdaExpr>(s))
            return;
        if (auto r = dyn_cast<DeclRefExpr>(s))
            if (auto v = dyn_cast<VarDecl>(r->getDecl()); v && !v->hasGlobalStorage())
                cells.insert(v);
        for (auto child : s->children())
            markCapturedVariables(child);
    }
    void markReturnedReferences(const Stmt* s) {
        if (!s || isa<LambdaExpr>(s))
            return;
        if (auto returned = dyn_cast<ReturnStmt>(s); returned && returned->getRetValue())
            markReference(returned->getRetValue());
        for (auto child : s->children())
            markReturnedReferences(child);
    }
    void scan(const Stmt* s) {
        if (!s)
            return;
        if (auto branch = dyn_cast<IfStmt>(s);
            branch && (branch->isConstexpr() || branch->isConsteval())) {
            scan(branch->getInit());
            scan(branch->getConditionVariableDeclStmt());
            scan(branch->getCond());
            bool selected = branch->isConsteval()
                                ? branch->isNegatedConsteval()
                                : branch->getCond()->EvaluateKnownConstInt(ctx).getBoolValue();
            scan(selected ? branch->getThen() : branch->getElse());
            return;
        }
        if (auto expression = dyn_cast<Expr>(s)) {
            auto t = clean(expression->getType());
            if (dataPointer(t))
                if (auto r = byteRecord(clean(t->getPointeeType())))
                    byteRecords.insert(r);
        }
        if (auto temporary = dyn_cast<CXXBindTemporaryExpr>(s);
            temporary && ownsDestructor(temporary->getType()) &&
            !localInitializers.count(temporary))
            fail(s, "this temporary's destruction needs additional lifetime lowering");
        if (auto allocated = dyn_cast<CXXNewExpr>(s);
            allocated && ownsDestructor(allocated->getAllocatedType()))
            fail(s, "heap object destruction needs additional lifetime lowering");
        if (auto deleted = dyn_cast<CXXDeleteExpr>(s);
            deleted && ownsDestructor(deleted->getArgument()->getType()->getPointeeType()))
            fail(s, "heap object destruction needs additional lifetime lowering");
        if (auto call = dyn_cast<CallExpr>(s);
            call && call->getDirectCallee() && isa<CXXDestructorDecl>(call->getDirectCallee()))
            fail(s, "explicit destructor calls need additional lifetime lowering");
        if (auto call = dyn_cast<CallExpr>(s); call && call->getDirectCallee() &&
                                               !user(call->getDirectCallee()) &&
                                               call->getDirectCallee()->getNameAsString() == "swap")
            for (auto argument : call->arguments())
                if (ownsDestructor(argument->getType()))
                    fail(s, "swap temporary destruction needs additional lifetime lowering");
        if (isa<Expr>(s) && expressionBounds(s, false))
            markCapturedVariables(s);
        if (auto cast = dyn_cast<ExplicitCastExpr>(s))
            scanArrayBounds(cast->getTypeAsWritten(), true);
        if (auto trait = dyn_cast<UnaryExprOrTypeTraitExpr>(s); trait && trait->isArgumentType())
            scanArrayBounds(trait->getTypeOfArgument(), true);
        if (auto trait = dyn_cast<TypeTraitExpr>(s))
            for (auto argument : trait->getArgs())
                scanArrayBounds(argument->getType(), true);
        if (auto argument = dyn_cast<CXXDefaultArgExpr>(s))
            scan(argument->getExpr());
        if (auto r = dyn_cast<DeclRefExpr>(s))
            reservedNames.insert(identifier(r->getDecl()->getNameAsString()));
        if (auto l = dyn_cast<LambdaExpr>(s)) {
            auto callable = lambdaOperator(l->getLambdaClass());
            for (auto field : l->getLambdaClass()->fields())
                if (ownsDestructor(field->getType()))
                    fail(l, "captured object destruction needs additional lifetime lowering");
            if (ownsDestructor(callable->getReturnType()))
                fail(l, "returned object destruction needs additional lifetime lowering");
            for (auto parameter : callable->parameters())
                if (ownsDestructor(parameter->getType()))
                    fail(l, "value-parameter destruction needs additional lifetime lowering");
            if (referenceABI(callable->getReturnType()))
                markReturnedReferences(callable->getBody());
            for (auto p : callable->parameters()) {
                reservedNames.insert(identifier(p->getNameAsString()));
                if (referenceABI(p->getType()))
                    references.insert(p);
            }
            for (auto capture : l->captures())
                if (capture.capturesVariable())
                    if (auto v = dyn_cast<VarDecl>(capture.getCapturedVar()))
                        cells.insert(v);
            if (callable->getBody() != l->getBody())
                scan(callable->getBody());
        }
        if (auto d = dyn_cast<DeclStmt>(s)) {
            for (auto decl : d->decls()) {
                if (auto v = dyn_cast<VarDecl>(decl)) {
                    if (ownsDestructor(v->getType())) {
                        if (v->hasAttr<CleanupAttr>())
                            fail(v->getLocation(),
                                 "cleanup attributes need additional lifetime lowering");
                        if (!scopedDestructor(v->getType()))
                            fail(v->getLocation(),
                                 "element destruction needs additional lifetime lowering");
                        if (!v->hasLocalStorage() || v->isStaticLocal())
                            fail(v->getLocation(),
                                 "static object destruction needs additional lifetime lowering");
                        auto parents = ctx.getParents(*d);
                        if (parents.size() != 1 || !parents[0].get<CompoundStmt>())
                            fail(v->getLocation(),
                                 "this declaration's destruction needs additional scope lowering");
                        markLocalInitializer(v->getInit());
                    }
                    scanArrayBounds(v->getType());
                    reservedNames.insert(identifier(v->getNameAsString()));
                    if (v->isStaticLocal() && !staticStorage.count(v->getCanonicalDecl())) {
                        if (v->getTLSKind() != VarDecl::TLS_None)
                            fail(v->getLocation(),
                                 "thread-local storage needs per-thread lowering");
                        staticStorage[v->getCanonicalDecl()] =
                            temporary("jppStatic_" + identifier(v->getNameAsString()) + "_");
                        staticLocals.push_back(v);
                        cells.insert(v);
                        markCapturedVariables(v->getInit());
                        if (v->getType()->isReferenceType())
                            markReference(v->getInit());
                    }
                    std::function<bool(const Stmt*)> refersToSelf = [&](const Stmt* s) {
                        if (!s)
                            return false;
                        if (auto ref = dyn_cast<DeclRefExpr>(s); ref && ref->getDecl() == v)
                            return true;
                        return std::any_of(s->child_begin(), s->child_end(), refersToSelf);
                    };
                    if (refersToSelf(v->getInit())) {
                        cells.insert(v);
                        selfInitialized.insert(v);
                    }
                    if (auto bindings = dyn_cast<DecompositionDecl>(v))
                        for (auto b : bindings->bindings())
                            reservedNames.insert(identifier(b->getNameAsString()));
                    if (referenceABI(v->getType())) {
                        references.insert(v);
                        markReference(v->getInit());
                    } else if (v->getType()->isReferenceType())
                        markReference(v->getInit());
                } else if (auto alias = dyn_cast<TypedefNameDecl>(decl))
                    scanArrayBounds(alias->getUnderlyingType());
            }
        }
        if (auto c = dyn_cast<CallExpr>(s);
            c && c->getDirectCallee() &&
            (user(c->getDirectCallee()) || c->getDirectCallee()->getNameAsString() == "swap")) {
            auto f = c->getDirectCallee();
            if (auto method = dyn_cast<CXXMethodDecl>(f); method && method->isStatic())
                if (auto member = dyn_cast<MemberExpr>(bare(c->getCallee()));
                    member && member->getBase()->HasSideEffects(ctx))
                    markCapturedVariables(c);
            unsigned offset = isa<CXXOperatorCallExpr>(c) && isa<CXXMethodDecl>(f) ? 1 : 0;
            for (unsigned i = 0; i < f->getNumParams() && i + offset < c->getNumArgs(); ++i)
                if (referenceABI(f->getParamDecl(i)->getType()))
                    markReference(c->getArg(i + offset));
        }
        if (auto call = dyn_cast<CallExpr>(s);
            call && call->getDirectCallee() && !user(call->getDirectCallee())) {
            auto method = call->getDirectCallee()->getNameAsString();
            if (method == "tie" || method == "forward_as_tuple" || method == "ref" ||
                method == "cref")
                for (auto argument : call->arguments())
                    markReference(argument);
            if (method == "tuple_cat" || method == "apply" || method == "make_from_tuple")
                markCapturedVariables(call);
            if (method == "swap")
                markCapturedVariables(call);
            if (method == "swap" || method == "move" || method == "forward") {
                for (auto argument : call->arguments())
                    vectorStorage |= containsVector(argument->getType());
                if (auto member = dyn_cast<CXXMemberCallExpr>(call)) {
                    auto owner = clean(member->getImplicitObjectArgument()->getType());
                    if (owner->isPointerType())
                        owner = owner->getPointeeType();
                    vectorStorage |= containsVector(owner);
                }
            }
            for (unsigned i = 0;
                 i < call->getNumArgs() && i < call->getDirectCallee()->getNumParams(); ++i)
                if (call->getDirectCallee()->getParamDecl(i)->getType()->isRValueReferenceType())
                    vectorStorage |= containsVector(call->getArg(i)->getType());
            if (method == "emplace" || method == "emplace_back" || method == "emplace_front")
                if (auto member = dyn_cast<CXXMemberCallExpr>(call)) {
                    auto container = clean(member->getImplicitObjectArgument()->getType());
                    if (container->isPointerType())
                        container = container->getPointeeType();
                    if (isVec(container) || family(container) == "deque" ||
                        family(container) == "queue" || family(container) == "stack" ||
                        family(container) == "priority_queue") {
                        auto item = element(container);
                        if (product(item)) {
                            auto elements = productTypes(item);
                            for (unsigned i = 0; i < call->getNumArgs() && i < elements.size(); ++i)
                                if (elements[i]->isReferenceType())
                                    markReference(call->getArg(i));
                        }
                    }
                }
        }
        if (auto c = dyn_cast<CXXOperatorCallExpr>(s); c && c->getOperator() == OO_Call)
            if (auto function = c->getDirectCallee())
                for (unsigned i = 0; i < function->getNumParams() && i + 1 < c->getNumArgs(); ++i)
                    if (referenceABI(function->getParamDecl(i)->getType()))
                        markReference(c->getArg(i + 1));
        if (auto c = dyn_cast<CXXConstructExpr>(s))
            if (product(c->getType()) || c->getConstructor()->isMoveConstructor())
                vectorStorage |= containsVector(c->getType());
        if (auto c = dyn_cast<CXXConstructExpr>(s))
            for (unsigned i = 0; i < c->getConstructor()->getNumParams() && i < c->getNumArgs();
                 ++i)
                if (referenceABI(c->getConstructor()->getParamDecl(i)->getType()))
                    markReference(c->getArg(i));
        if (auto constructor = dyn_cast<CXXConstructExpr>(s);
            constructor && product(constructor->getType())) {
            auto elements = productTypes(constructor->getType());
            if (constructor->getNumArgs() == elements.size())
                for (unsigned i = 0; i < elements.size(); ++i)
                    if (elements[i]->isReferenceType())
                        markReference(constructor->getArg(i));
        }
        if (auto c = dyn_cast<CallExpr>(s);
            c && c->getDirectCallee() && c->getDirectCallee()->getNameAsString() == "getline" &&
            c->getNumArgs() >= 2 && isString(c->getArg(1)->getType())) {
            if (c->getArg(0)->HasSideEffects(ctx))
                markCapturedVariables(c);
            if (addressHasSideEffects(c->getArg(1)))
                markReference(c->getArg(1));
        }
        if (auto c = dyn_cast<CallExpr>(s);
            c && !c->getDirectCallee() && !c->getCallee()->getType()->isDependentType()) {
            auto signature = clean(c->getCallee()->getType());
            if (signature->isPointerType())
                signature = signature->getPointeeType();
            if (auto prototype = signature->getAs<FunctionProtoType>())
                for (unsigned i = 0; i < prototype->getNumParams() && i < c->getNumArgs(); ++i)
                    if (referenceABI(prototype->getParamType(i)))
                        markReference(c->getArg(i));
        }
        if (auto c = dyn_cast<CompoundAssignOperator>(s);
            c && (c->getRHS()->HasSideEffects(ctx) || big(c->getLHS()->getType()) ||
                  dataPointer(c->getLHS()->getType()) || addressHasSideEffects(c->getLHS())))
            markReference(c->getLHS());
        if (auto b = dyn_cast<BinaryOperator>(s);
            b && b->isAssignmentOp() && isa<AbstractConditionalOperator>(bare(b->getLHS())))
            markReference(b->getLHS());
        if (auto u = dyn_cast<UnaryOperator>(s);
            u && u->isIncrementDecrementOp() &&
            (isa<AbstractConditionalOperator>(bare(u->getSubExpr())) ||
             dataPointer(u->getSubExpr()->getType())))
            markReference(u->getSubExpr());
        if (auto u = dyn_cast<UnaryOperator>(s); u && u->getOpcode() == UO_AddrOf) {
            markReference(u->getSubExpr());
            markCapturedVariables(u);
        }
        if (auto b = dyn_cast<BinaryOperator>(s);
            b && b->getOpcode() == BO_Comma &&
            (b->getLHS()->getType()->isVoidType() || b->getRHS()->getType()->isVoidType()))
            markCapturedVariables(b);
        if (auto c = dyn_cast<ConditionalOperator>(s); c && c->getType()->isVoidType())
            markCapturedVariables(c);
        if (isa<BinaryConditionalOperator>(s) || isa<StmtExpr>(s) || isa<CXXNewExpr>(s))
            markCapturedVariables(s);
        if (auto list = dyn_cast<InitListExpr>(s);
            list && record(list->getType()) && user(record(list->getType())))
            for (auto init : list->inits())
                if (isa<CXXDefaultInitExpr>(init)) {
                    markCapturedVariables(list);
                    break;
                }
        if (auto op = dyn_cast<CXXOperatorCallExpr>(s);
            op && op->getOperator() == OO_Equal && op->getNumArgs() == 2)
            markReference(op->getArg(0));
        if (auto op = dyn_cast<CXXOperatorCallExpr>(s);
            op && op->getOperator() == OO_GreaterGreater && op->getNumArgs() == 2 &&
            stream(op->getArg(0)->getType())) {
            if (inputRoot(op)->HasSideEffects(ctx))
                markCapturedVariables(op);
            if (addressHasSideEffects(op->getArg(1)))
                markReference(op->getArg(1));
        }
        for (auto child : s->children())
            scan(child);
    }
    const Expr* referenceSource(const Expr* e) const {
        auto wrapped = e;
        while (true) {
            if (auto cleanup = dyn_cast<ExprWithCleanups>(wrapped))
                wrapped = cleanup->getSubExpr();
            else if (auto parens = dyn_cast<ParenExpr>(wrapped))
                wrapped = parens->getSubExpr();
            else if (auto cast = dyn_cast<ImplicitCastExpr>(wrapped))
                wrapped = cast->getSubExpr();
            else if (auto argument = dyn_cast<CXXDefaultArgExpr>(wrapped))
                wrapped = argument->getExpr();
            else
                break;
        }
        return wrapped;
    }
    bool returnsReference(const CallExpr* call) {
        if (auto function = call->getDirectCallee()) {
            if (user(function))
                return referenceABI(function->getReturnType());
            if (productGet(call))
                return referenceABI(function->getReturnType());
            if (function->getNameAsString() == "apply" ||
                function->getNameAsString() == "make_from_tuple")
                return referenceABI(function->getReturnType());
            if (auto member = dyn_cast<CXXMemberCallExpr>(call);
                member &&
                (function->getNameAsString() == "get" || isa<CXXConversionDecl>(function)) &&
                family(member->getImplicitObjectArgument()->getType()) == "reference_wrapper")
                return referenceABI(function->getReturnType());
            if (auto op = dyn_cast<CXXOperatorCallExpr>(call);
                op && op->getOperator() == OO_Call && op->getNumArgs() &&
                (family(op->getArg(0)->getType()) == "function" ||
                 family(op->getArg(0)->getType()) == "reference_wrapper"))
                return referenceABI(function->getReturnType());
            return false;
        }
        return referenceABI(callableType(call->getCallee()->getType())->getReturnType());
    }
    std::string reference(const Expr* e) {
        if (auto opaque = dyn_cast_or_null<OpaqueValueExpr>(bare(e));
            opaque && opaqueReferences.count(opaque))
            return opaqueReferences.at(opaque);
        if (auto cast = dyn_cast_or_null<ExplicitCastExpr>(bare(e));
            cast && cast->getCastKind() == CK_NoOp && cast->isGLValue())
            return reference(cast->getSubExpr());
        if (auto declaration = dyn_cast_or_null<DeclRefExpr>(bare(e));
            declaration && isa<FunctionDecl>(declaration->getDecl()))
            return "JppRef.object(" +
                   functionPointer(cast<FunctionDecl>(declaration->getDecl()),
                                   ctx.getPointerType(e->getType())) +
                   ")";
        auto wrapped = referenceSource(e);
        if (isa<MaterializeTemporaryExpr>(wrapped))
            return "JppRef.cell(" + expr(e) + ")";
        if (auto call = dyn_cast<CallExpr>(bare(e)); call && productGet(call))
            return productReference(expr(call->getArg(0)), call->getArg(0)->getType(),
                                    productIndex(call));
        if (auto element = standardArrayElementReference(bare(e)); !element.empty())
            return element;
        if (auto call = dyn_cast<CXXMemberCallExpr>(bare(e));
            call && family(call->getImplicitObjectArgument()->getType()) == "reference_wrapper" &&
            (call->getMethodDecl()->getNameAsString() == "get" ||
             isa<CXXConversionDecl>(call->getMethodDecl())))
            return expr(call->getImplicitObjectArgument());
        if (auto assignment = dyn_cast<CXXOperatorCallExpr>(bare(e));
            assignment && assignment->getOperator() == OO_Equal && assignment->getNumArgs() == 2 &&
            family(assignment->getArg(0)->getType()) == "array")
            return assignStandardArray(assignment->getArg(0), assignment->getArg(1), true);
        if (auto assignment = dyn_cast<CXXOperatorCallExpr>(bare(e));
            assignment && assignment->getOperator() == OO_Equal && assignment->getNumArgs() == 2 &&
            product(assignment->getArg(0)->getType()))
            return "JppRef.object(" + assignProduct(assignment->getArg(0), assignment->getArg(1)) +
                   ")";
        if (auto call = dyn_cast<CallExpr>(bare(e)); call && returnsReference(call))
            return expression(call);
        if (auto compound = dyn_cast<CompoundAssignOperator>(bare(e)))
            return compoundAssignment(compound, true);
        if (auto update = dyn_cast<UnaryOperator>(bare(e));
            update && update->isPrefix() && update->isIncrementDecrementOp())
            return std::string(dataPointer(update->getSubExpr()->getType())
                                   ? "Jpp.updatedPointerReference("
                                   : "Jpp.updatedReference(") +
                   reference(update->getSubExpr()) + ", " + (update->isIncrementOp() ? "1" : "-1") +
                   ")";
        if (auto binary = dyn_cast<BinaryOperator>(bare(e))) {
            if (binary->getOpcode() == BO_Assign)
                return "Jpp.storeReference(" +
                       value(binary->getRHS(), binary->getLHS()->getType()) + ", " +
                       reference(binary->getLHS()) + ")";
            if (binary->getOpcode() == BO_Comma)
                return supplier([&] {
                    expressionStatement(binary->getLHS());
                    line("return " + reference(binary->getRHS()) + ";");
                });
        }
        if (auto assignment = dyn_cast<CXXOperatorCallExpr>(bare(e));
            assignment && assignment->getOperator() == OO_Equal && assignment->getNumArgs() == 2) {
            auto function = assignment->getDirectCallee();
            if (defaultedMove(function))
                return assignDefaultedMove(cast<CXXMethodDecl>(function), assignment->getArg(0),
                                           assignment->getArg(1), true);
            if (family(assignment->getArg(0)->getType()) == "vector" && function &&
                function->getNumParams() &&
                function->getParamDecl(0)->getType()->isRValueReferenceType())
                return "Jpp.moveAssignVectorRef(" + expr(assignment->getArg(1)) + ", " +
                       reference(assignment->getArg(0)) + ")";
            if (function && function->getReturnType()->isReferenceType() &&
                (!user(function) || function->isImplicit() || function->isDefaulted()))
                return "Jpp.assignReference(" +
                       value(assignment->getArg(1), assignment->getArg(0)->getType()) + ", " +
                       reference(assignment->getArg(0)) + ")";
        }
        if (auto call = dyn_cast<CallExpr>(bare(e));
            call && call->getDirectCallee() &&
            call->getDirectCallee()->getReturnType()->isReferenceType())
            if (auto r = record(call->getType()); r && (user(r) || isVec(call->getType())))
                return "JppRef.object(" + expr(call) + ")";
        if (auto c = dyn_cast<CallExpr>(bare(e));
            c && c->getDirectCallee() && !user(c->getDirectCallee())) {
            auto n = c->getDirectCallee()->getNameAsString();
            if ((n == "move" || n == "forward" || n == "as_const") && c->getNumArgs())
                return reference(c->getArg(0));
            if ((n == "min" || n == "max") && c->getNumArgs() >= 2 &&
                c->getDirectCallee()->getReturnType()->isReferenceType()) {
                auto item = clean(c->getDirectCallee()->getReturnType());
                auto predicate = c->getNumArgs() > 2
                                     ? comparatorPredicate(c->getArg(2)->getType(), item, item,
                                                           expr(c->getArg(2)))
                                     : comparisonPredicate(item, item);
                return "Jpp." + n + "Reference(" + reference(c->getArg(0)) + ", " +
                       reference(c->getArg(1)) + ", " + predicate + ")";
            }
            if (auto call = dyn_cast<CXXMemberCallExpr>(c)) {
                auto member = dyn_cast<MemberExpr>(bare(call->getCallee()));
                if (member) {
                    auto objectType = member->getBase()->getType();
                    if (objectType->isPointerType())
                        objectType = objectType->getPointeeType();
                    if (isVec(objectType) || isString(objectType) || isMap(objectType) ||
                        family(objectType) == "deque") {
                        auto object = expr(member->getBase()) + (member->isArrow() ? ".get()" : "");
                        if (n == "at" && isMap(objectType))
                            return "JppRef.mapAt(" + object + ", " + expr(c->getArg(0)) + ")";
                        if (n == "front" || n == "back")
                            return "JppRef." + n + "(" + object + ")";
                        if (n == "at" && c->getNumArgs())
                            return "JppRef.at(" + object + ", " + intArgument(c->getArg(0)) + ")";
                    }
                }
            }
        }
        if (auto c = dyn_cast<BinaryConditionalOperator>(bare(e)); c && c->isLValue())
            return supplier([&] {
                auto common = temporary("jppSelected");
                line("JppRef<" + boxed(c->getCommon()->getType()) + "> " + common + " = " +
                     reference(c->getCommon()) + ";");
                auto previous = opaqueValues;
                opaqueValues[c->getOpaqueValue()] = common + ".get()";
                line("return " + truth(c->getCond()) + " ? " + common + " : " +
                     reference(c->getFalseExpr()) + ";");
                opaqueValues = std::move(previous);
            });
        if (auto c = dyn_cast<ConditionalOperator>(bare(e)))
            return "(" + truth(c->getCond()) + " ? " + reference(c->getTrueExpr()) + " : " +
                   reference(c->getFalseExpr()) + ")";
        if (auto d = dyn_cast<DeclRefExpr>(bare(e));
            d && aliases.count(d->getDecl()) && aliases[d->getDecl()].ends_with(".get()")) {
            auto s = aliases[d->getDecl()];
            return s.substr(0, s.size() - 6);
        }
        if (primitiveArray(e->getType())) {
            if (auto v = variableRef(e); v && v->getType()->isReferenceType() && cell(v))
                return name(v);
            return "JppRef.at(" + arrayLocation(e) + ")";
        }
        if (auto v = variableRef(e)) {
            if (!user(v) && v->getName() == "ignore")
                return "JppRef.object(JppIgnore.INSTANCE)";
            if (!user(v) && ordering(v->getType()))
                return "JppRef.object(" + expr(e) + ")";
            if (!user(v) && stream(v->getType()))
                return "JppRef.object(" + expr(e) + ")";
            if (cell(v)) {
                if (auto a = aliases.find(v); a != aliases.end() && a->second.ends_with(".get()"))
                    return a->second.substr(0, a->second.size() - 6);
                return name(v);
            }
            if (v->hasGlobalStorage()) {
                auto value = temporary("jppValue");
                auto owner = v->isStaticDataMember()
                                 ? name(cast<CXXRecordDecl>(v->getDeclContext()))
                                 : options.className;
                return "JppRef.of(() -> " + expr(e) + ", " + value + " -> " + expr(e) + " = " +
                       value + ", " + owner + ".class, " + quote(name(v)) + ")";
            }
        }
        auto [base, index] = subscript(e);
        if (base && primitiveArray(bare(base)->getType()))
            return "JppRef.at(" + arrayDecay(bare(base)) + ".plus(" + intArgument(index) + "))";
        if (base && dataPointer(base->getType()) && !ctx.getAsArrayType(bare(base)->getType()))
            return "JppRef.at(" + expr(base) + ".plus(" + intArgument(index) + "))";
        if (base)
            return "JppRef.at(" + expr(bare(base)) + ", " +
                   (isMap(base->getType()) ? expr(index) : intArgument(index)) + ")";
        if (auto dereference = dyn_cast<UnaryOperator>(bare(e));
            dereference && dereference->getOpcode() == UO_Deref)
            return "JppRef.at(" + expr(dereference->getSubExpr()) + ")";
        if (auto dereference = dyn_cast<CXXOperatorCallExpr>(bare(e));
            dereference && dereference->getOperator() == OO_Star &&
            dereference->getNumArgs() == 1 && isIter(dereference->getArg(0)->getType()))
            return "JppRef.at(" + expr(dereference->getArg(0)) + ")";
        if (auto m = dyn_cast<MemberExpr>(bare(e))) {
            auto owner = m->getBase()->getType();
            if (owner->isPointerType())
                owner = owner->getPointeeType();
            if (family(owner) == "pair" && (m->getMemberDecl()->getName() == "first" ||
                                            m->getMemberDecl()->getName() == "second"))
                return productReference(expr(m->getBase()) + (m->isArrow() ? ".get()" : ""), owner,
                                        m->getMemberDecl()->getName() == "first" ? 0 : 1);
            auto base = isa<CXXThisExpr>(bare(m->getBase()))
                            ? (constructionThis.empty() ? "this" : constructionThis)
                            : expr(m->getBase());
            if (m->isArrow() && !isa<CXXThisExpr>(bare(m->getBase()))) {
                base += ".get()";
            }
            auto object = temporary("jppObject"), value = temporary("jppValue");
            auto member = object + "." + name(m->getMemberDecl());
            return "JppRef.member(" + base + ", " + quote(name(m->getMemberDecl())) + ", " +
                   object + " -> " + member + ", (" + object + ", " + value + ") -> " + member +
                   " = " + value + ")";
        }
        if (e->isLValue())
            fail(e, "this reference binding is not supported");
        return "JppRef.cell(" + expr(e) + ")";
    }
    std::string address(const Expr* e) {
        if (primitiveArray(e->getType()))
            return arrayLocation(e);
        if (auto dereference = dyn_cast<UnaryOperator>(bare(e));
            dereference && dereference->getOpcode() == UO_Deref)
            return "Jpp.pointer(" + expr(dereference->getSubExpr()) + ")";
        return "(" + reference(e) + ").pointer()";
    }
    // Keep the direct Java spelling only when this expression denotes a real
    // Java array. An array reached through malloc or an array reference is an
    // lvalue at an address; materializing a Java array would detach its writes.
    bool javaArray(const Expr* e) const {
        e = bare(e);
        if (!e || !ctx.getAsArrayType(e->getType()))
            return false;
        if (auto d = dyn_cast<DeclRefExpr>(e)) {
            if (auto v = dyn_cast<VarDecl>(d->getDecl()))
                return !v->getType()->isReferenceType();
            return false;
        }
        if (auto indexed = dyn_cast<ArraySubscriptExpr>(e))
            return javaArray(indexed->getBase());
        return isa<MemberExpr>(e) || isa<InitListExpr>(e) || isa<StringLiteral>(e);
    }
    std::string standardArrayElementReference(const Expr* e) {
        if (!e || !primitiveArray(e->getType()))
            return {};
        std::string location;
        if (auto index = dyn_cast<CXXOperatorCallExpr>(e);
            index && index->getOperator() == OO_Subscript && index->getNumArgs() == 2 &&
            family(index->getArg(0)->getType()) == "array")
            location =
                "JppRef.at(" + expr(index->getArg(0)) + ", " + intArgument(index->getArg(1)) + ")";
        if (auto call = dyn_cast<CXXMemberCallExpr>(e)) {
            auto argument = call->getImplicitObjectArgument();
            auto owner = clean(argument->getType());
            bool pointer = owner->isPointerType();
            if (pointer)
                owner = owner->getPointeeType();
            if (family(owner) == "array") {
                auto method = call->getMethodDecl()->getNameAsString();
                auto object = expr(argument) + (pointer ? ".get()" : "");
                if (method == "at" || method == "operator[]")
                    location = "JppRef.at(" + object + ", " + intArgument(call->getArg(0)) + ")";
                else if (method == "front" || method == "back")
                    location = "JppRef." + method + "(" + object + ")";
            }
        }
        return location.empty() ? ""
                                : "Jpp.arrayReference(" + location + ", " +
                                      memoryType(e->getType()) + ", " + targetLittleEndian() + ")";
    }
    std::string arrayLocation(const Expr* input) {
        auto e = bare(input);
        if (auto opaque = dyn_cast_or_null<OpaqueValueExpr>(e);
            opaque && opaqueReferences.count(opaque))
            return "(" + opaqueReferences.at(opaque) + ").pointer()";
        if (auto element = standardArrayElementReference(e); !element.empty())
            return "(" + element + ").pointer()";
        if (auto literal = dyn_cast<StringLiteral>(e)) {
            auto element = ctx.getAsArrayType(e->getType())->getElementType();
            if (!element->isCharType())
                fail(e, "this character literal array needs additional storage lowering");
            auto bytes = literal->getBytes().str();
            auto [storage, inserted] = literalArrays.try_emplace(bytes);
            if (inserted)
                storage->second = temporary("jppLiteral");
            return "Jpp.arrayAddress(" + storage->second + ", " + memoryType(e->getType()) + ", " +
                   targetLittleEndian() + ")";
        }
        if (auto call = dyn_cast<CallExpr>(e); call && returnsReference(call))
            return "(" + expression(call) + ").pointer()";
        if (auto call = dyn_cast<CallExpr>(e); call && call->getDirectCallee() &&
                                               !user(call->getDirectCallee()) &&
                                               call->getNumArgs()) {
            auto name = call->getDirectCallee()->getNameAsString();
            if (name == "move" || name == "forward" || name == "as_const")
                return arrayLocation(call->getArg(0));
        }
        if (auto d = dyn_cast<DeclRefExpr>(e)) {
            if (auto a = aliases.find(d->getDecl());
                a != aliases.end() && a->second.ends_with(".get()"))
                return "(" + a->second.substr(0, a->second.size() - 6) + ").pointer()";
            if (auto v = dyn_cast<VarDecl>(d->getDecl()); v && v->getType()->isReferenceType())
                return "(" + reference(e) + ").pointer()";
        }
        if (auto indexed = dyn_cast<ArraySubscriptExpr>(e))
            return expr(indexed->getBase()) + ".plus(" + intArgument(indexed->getIdx()) + ")";
        if (auto dereference = dyn_cast<UnaryOperator>(e);
            dereference && dereference->getOpcode() == UO_Deref)
            return "Jpp.pointer(" + expr(dereference->getSubExpr()) + ")";
        if (isa<AbstractConditionalOperator>(e))
            return "(" + reference(e) + ").pointer()";
        if (auto binary = dyn_cast<BinaryOperator>(e); binary && binary->getOpcode() == BO_Comma)
            return supplier([&] {
                expressionStatement(binary->getLHS());
                line("return " + arrayLocation(binary->getRHS()) + ";");
            });
        if (auto cast = dyn_cast<ExplicitCastExpr>(e))
            return "Jpp.<" + boxed(e->getType()) + ">typedPointer(" + address(cast->getSubExpr()) +
                   ", " + memoryType(e->getType()) + ", " + targetLittleEndian() + ")";
        if (javaArray(e) && !isa<StringLiteral>(e))
            return "Jpp.arrayAddress(" + expr(e) + ", " + memoryType(e->getType()) + ", " +
                   targetLittleEndian() + ")";
        fail(e, "this array expression needs additional address lowering");
    }
    std::string arrayDecay(const Expr* e) {
        auto array = ctx.getAsArrayType(e->getType());
        return "Jpp.<" + boxed(array->getElementType()) + ">arrayDecay(" + arrayLocation(e) + ")";
    }
    std::string booleanValue(const std::string& s, QualType t) {
        t = clean(t);
        if (t->isNullPtrType())
            return "Jpp.last(" + s + ", false)";
        if (t->isBooleanType())
            return s;
        if (t->isCharType() && ctx.getTypeSize(t) == 8)
            return "(((int)(char)(" + s + ") & 255) != 0)";
        if (big(t))
            return "((" + s + ").signum() != 0)";
        if (longDouble(t))
            return "!(" + s + ").isZero()";
        if (t->isArithmeticType() || t->isEnumeralType())
            return "((" + s + ") != 0)";
        if (t->isPointerType())
            return "(" + s + " != null)";
        if (stream(t))
            return "(" + s + ".good)";
        fail(SourceLocation(), "condition cannot be represented as a Java boolean");
    }
    bool foldedCondition(const Expr* e, bool& value) {
        return e && !e->HasSideEffects(ctx) && !expressionBounds(e, false) &&
               e->EvaluateAsBooleanCondition(value, ctx);
    }
    std::string truth(const Expr* e) {
        bool value;
        if (foldedCondition(e, value))
            return value ? "true" : "false";
        e = strip(e);
        if (auto cast = dyn_cast<ImplicitCastExpr>(e)) {
            if (cast->getCastKind() == CK_IntegralToBoolean ||
                cast->getCastKind() == CK_FloatingToBoolean ||
                cast->getCastKind() == CK_PointerToBoolean)
                return truth(cast->getSubExpr());
        }
        if ((e->getType()->isFunctionPointerType() || e->getType()->isFunctionType()) &&
            !e->HasSideEffects(ctx)) {
            bool result;
            if (e->EvaluateAsBooleanCondition(result, ctx))
                return result ? "true" : "false";
        }
        return booleanValue(expr(e), e->getType());
    }
    std::string loopCondition(const Expr* e) {
        bool value;
        // Java rejects a loop body guarded by a compile-time false expression.
        // Unboxing Boolean.FALSE keeps the C++ body legal and never executes it.
        if (foldedCondition(e, value) && !value)
            return "Boolean.FALSE";
        return truth(e);
    }
    std::string newArray(QualType t, const Expr* init, bool initializeObjects = true) {
        auto a = ctx.getAsArrayType(t);
        if (!a)
            return defaultValue(t);
        auto elem = a->getElementType();
        if (init) {
            if (isa<ArrayInitLoopExpr>(bare(init)) && primitiveArray(t))
                return expr(init);
            if (auto literal = dyn_cast<StringLiteral>(bare(init)); literal && elem->isCharType()) {
                auto size = dyn_cast<ConstantArrayType>(a);
                if (!size)
                    fail(init, "a string-initialized character array needs a fixed bound");
                return "Jpp.charArray(" + quote(literal->getBytes().str(), true) + ", " +
                       std::to_string(size->getSize().getZExtValue()) + ")";
            }
            auto list = dyn_cast<InitListExpr>(bare(init));
            if (list) {
                std::vector<std::string> values;
                unsigned n = list->getNumInits();
                for (unsigned i = 0; i < n; ++i)
                    values.push_back(i < list->getNumInits()
                                         ? (ctx.getAsArrayType(elem)
                                                ? newArray(elem, list->getInit(i))
                                                : value(list->getInit(i), elem, true))
                                         : defaultValue(elem));
                auto javaType = type(t), rawType = eraseTypeArguments(javaType);
                auto literal = "new " + rawType + " {" + join(values) + "}";
                if (rawType != javaType)
                    literal = "(" + javaType + ") " + literal;
                if (auto ca = dyn_cast<ConstantArrayType>(a);
                    ca && ca->getSize().getZExtValue() > n)
                    return "Jpp.arrayInit(" + newArray(t, nullptr) + ", " + literal + ")";
                return literal;
            }
        }
        std::string base = type(t), dims;
        while (base.ends_with("[]"))
            base.resize(base.size() - 2);
        QualType part = t;
        while (auto ar = ctx.getAsArrayType(part)) {
            dims += "[" + arrayBound(ar) + "]";
            part = ar->getElementType();
        }
        std::string allocation = "new " + base + dims;
        if (base.find('<') != std::string::npos) {
            allocation = "(" + type(t) + ") new " + eraseTypeArguments(base) + dims;
        }
        if ((initializeObjects && part->isRecordType()) || big(part) || longDouble(part))
            return "Jpp.objects(" + allocation + ", () -> " + defaultValue(part) + ")";
        return allocation;
    }
    bool stream(QualType t) const {
        auto n = family(t);
        return n.find("istream") != std::string::npos || n.find("ostream") != std::string::npos ||
               n.find("fstream") != std::string::npos || n.find("basic_ios") != std::string::npos;
    }
    std::string inputValue(QualType t, const std::string& source, const std::string& previous) {
        auto jt = type(t);
        if (jt == "int" || jt == "long") {
            auto bits = ctx.getTypeSize(clean(t));
            std::string suffix = bits <= 8    ? "Byte"
                                 : bits <= 16 ? "Short"
                                 : bits <= 32 ? "Int"
                                              : "Long";
            return source + ".next" + (clean(t)->isUnsignedIntegerType() ? "Unsigned" : "") +
                   suffix + "(" + previous + ")";
        }
        if (jt == "char")
            return source + ".nextChar(" + previous + ")";
        if (jt == "double")
            return source + ".nextDouble(" + previous + ")";
        if (longDouble(t))
            return "Jpp.readLongDouble(" + source + ", " + previous + ")";
        if (jt == "float")
            return source + ".nextFloat(" + previous + ")";
        if (jt == "boolean")
            return source + ".nextBoolean(" + previous + ")";
        if (isString(t))
            return source + ".nextString(" + previous + ")";
        if (big(t))
            return "new BigInteger(" + source + ".nextString().toString())";
        fail(SourceLocation(), "unsupported cin extraction type " + t.getAsString());
    }
    const Expr* inputRoot(const Expr* e) const {
        auto op = dyn_cast<CXXOperatorCallExpr>(bare(e));
        return op && op->getOperator() == OO_GreaterGreater && op->getNumArgs() == 2 &&
                       stream(op->getArg(0)->getType())
                   ? inputRoot(op->getArg(0))
                   : e;
    }
    std::string inputChain(const Expr* e, std::vector<std::string>& parts,
                           const std::string& sourceOverride = {}) {
        e = bare(e);
        if (auto op = dyn_cast<CXXOperatorCallExpr>(e);
            op && op->getOperator() == OO_GreaterGreater && op->getNumArgs() == 2 &&
            stream(op->getArg(0)->getType())) {
            auto source = inputChain(op->getArg(0), parts, sourceOverride);
            auto destination = op->getArg(1);
            if (addressHasSideEffects(destination)) {
                auto input = temporary("jppStream"), old = temporary("jppPrevious");
                parts.push_back("Jpp.extract(" + source + ", " + reference(destination) + ", (" +
                                input + ", " + old + ") -> " +
                                inputValue(destination->getType(), input, old) + ")");
            } else
                parts.push_back(assign(
                    destination, inputValue(destination->getType(), source, expr(destination))));
            return source;
        }
        return sourceOverride.empty() ? expr(e) : sourceOverride;
    }
    std::string inputExpression(const CXXOperatorCallExpr* op) {
        auto root = inputRoot(op);
        if (root->HasSideEffects(ctx))
            return supplier([&] {
                auto source = temporary("jppStream");
                line("JppInput " + source + " = " + expr(root) + ";");
                std::vector<std::string> a;
                inputChain(op, a, source);
                for (const auto& assignment : a)
                    line(assignment + ";");
                line("return " + source + ";");
            });
        std::vector<std::string> a;
        auto source = inputChain(op, a);
        return "Jpp.input(" + source + ", " + join(a) + ")";
    }
    std::string outputChain(const CXXOperatorCallExpr* op) {
        std::string left = expr(op->getArg(0));
        auto right = bare(op->getArg(1));
        if (longDouble(right->getType()))
            fail(right, "long double text formatting needs additional lowering");
        if (auto dr = dyn_cast<DeclRefExpr>(right)) {
            auto n = dr->getDecl()->getNameAsString();
            if (n == "endl")
                return left + ".endl()";
            if (n == "flush")
                return left + ".flush()";
            if (n == "fixed" || n == "scientific")
                return n == "fixed" ? left + ".fixed(true)" : left + ".scientific()";
            if (n == "hexfloat")
                return left + ".hexadecimal()";
            if (n == "defaultfloat")
                return left + ".fixed(false)";
            if (n == "boolalpha" || n == "noboolalpha")
                return left + ".boolalpha(" + (n == "boolalpha" ? "true" : "false") + ")";
            if (n == "unitbuf" || n == "nounitbuf")
                return left + ".unitbuf(" + (n == "unitbuf" ? "true" : "false") + ")";
        }
        if (auto call = dyn_cast<CallExpr>(right);
            call && call->getDirectCallee() &&
            call->getDirectCallee()->getNameAsString() == "setprecision")
            return left + ".precision(" + intArgument(call->getArg(0)) + ")";
        auto s = expr(op->getArg(1));
        auto t = op->getArg(1)->getType();
        if (family(t) == "mersenne_twister_engine")
            fail(op, "Mersenne Twister stream serialization needs additional lowering");
        if (family(t) == "time_point")
            fail(op, "calendar and time-point text output needs additional lowering");
        if (t->isPointerType() && t->getPointeeType()->isCharType())
            s = "Jpp.cString(" + s + ")";
        else if (unsignedLong(t))
            s = "Long.toUnsignedString(" + s + ")";
        else if (clean(t)->isUnsignedIntegerType() && ctx.getTypeSize(t) == 32)
            s = "Integer.toUnsignedString(" + s + ")";
        return left + ".print(" + s + ")";
    }
    std::pair<const Expr*, const Expr*> subscript(const Expr* e) const {
        e = bare(e);
        if (auto a = dyn_cast<ArraySubscriptExpr>(e))
            return {a->getBase(), a->getIdx()};
        if (auto a = dyn_cast<CXXOperatorCallExpr>(e); a && a->getOperator() == OO_Subscript)
            return {a->getArg(0), a->getArg(1)};
        return {};
    }
    std::string assign(const Expr* lhs, const std::string& rhs) {
        if (auto opaque = dyn_cast_or_null<OpaqueValueExpr>(bare(lhs));
            opaque && opaqueReferences.count(opaque))
            return reference(lhs) + ".set(" + rhs + ")";
        if (referenceMember(lhs))
            return reference(lhs) + ".set(" + rhs + ")";
        if (auto call = dyn_cast<CallExpr>(bare(lhs)); call && returnsReference(call))
            return reference(lhs) + ".set(" + rhs + ")";
        if (isa<AbstractConditionalOperator>(bare(lhs)))
            return reference(lhs) + ".set(" + rhs + ")";
        if (auto d = dyn_cast<DeclRefExpr>(bare(lhs));
            d && aliases.count(d->getDecl()) && aliases[d->getDecl()].ends_with(".get()"))
            return reference(lhs) + ".set(" + rhs + ")";
        if (auto v = variableRef(lhs); cell(v))
            return reference(lhs) + ".set(" + rhs + ")";
        auto [base, index] = subscript(lhs);
        if (base && primitiveArray(bare(base)->getType()) && !javaArray(bare(base)))
            return arrayDecay(bare(base)) + ".plus(" + intArgument(index) + ").put(" + rhs + ")";
        if (base && dataPointer(base->getType()) && !ctx.getAsArrayType(bare(base)->getType()))
            return expr(base) + ".plus(" + intArgument(index) + ").put(" + rhs + ")";
        if (base && ctx.getAsArrayType(bare(base)->getType()))
            return expr(bare(base)) + "[" + intArgument(index) + "] = " + rhs;
        if (base && (isVec(base->getType()) || family(base->getType()) == "deque"))
            return "Jpp.set(" + expr(base) + ", " + intArgument(index) + ", " + rhs + ")";
        if (base && (isMap(base->getType()) || isString(base->getType())))
            return expr(base) + ".put(" +
                   (isMap(base->getType()) ? expr(index) : intArgument(index)) + ", " + rhs + ")";
        if (auto u = dyn_cast<UnaryOperator>(bare(lhs));
            u && u->getOpcode() == UO_Deref &&
            (isIter(u->getSubExpr()->getType()) || dataPointer(u->getSubExpr()->getType())))
            return expr(u->getSubExpr()) + ".put(" + rhs + ")";
        return expr(lhs) + " = " + rhs;
    }
    std::string arithmetic(const std::string& lhs, QualType leftType, const std::string& rhs,
                           QualType rightType, std::string op) {
        bool comparison =
            op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=";
        if (longDouble(leftType) || longDouble(rightType)) {
            auto left = converted(lhs, leftType, ctx.LongDoubleTy);
            auto right = converted(rhs, rightType, ctx.LongDoubleTy);
            if (comparison)
                return "(" + left + ").test(" + right + ", " + quote(op) + ")";
            static const std::map<std::string, std::string> operations = {
                {"+", "add"}, {"-", "subtract"}, {"*", "multiply"}, {"/", "divide"}};
            if (auto operation = operations.find(op); operation != operations.end())
                return "(" + left + ")." + operation->second + "(" + right + ")";
            fail(SourceLocation(), "unsupported long double arithmetic operation " + op);
        }
        if (comparison && clean(leftType)->isCharType() && ctx.getTypeSize(clean(leftType)) == 8)
            return arithmetic(converted(lhs, leftType, ctx.IntTy), ctx.IntTy,
                              converted(rhs, rightType, ctx.IntTy), ctx.IntTy, op);
        if (big(leftType) || big(rightType)) {
            auto l = big(leftType) ? lhs : "BigInteger.valueOf(" + lhs + ")";
            auto r = big(rightType) ? rhs : "BigInteger.valueOf(" + rhs + ")";
            if (comparison)
                return "(" + l + ".compareTo(" + r + ") " + op + " 0)";
            static const std::map<std::string, std::string> ops = {
                {"+", "add"},        {"-", "subtract"},   {"*", "multiply"}, {"/", "divide"},
                {"%", "remainder"},  {"&", "and"},        {"|", "or"},       {"^", "xor"},
                {"<<", "shiftLeft"}, {">>", "shiftRight"}};
            if (auto it = ops.find(op); it != ops.end())
                return l + "." + it->second + "(" +
                       ((op == "<<" || op == ">>") ? converted(rhs, rightType, ctx.IntTy) : r) +
                       ")";
        }
        if (comparison && clean(leftType)->isUnsignedIntegerType() &&
            (ctx.getTypeSize(leftType) == 32 || ctx.getTypeSize(leftType) == 64))
            return "(" + std::string(unsignedLong(leftType) ? "Long" : "Integer") +
                   ".compareUnsigned(" + lhs + ", " + rhs + ") " + op + " 0)";
        if (comparison && clean(leftType)->isBooleanType() && clean(rightType)->isBooleanType())
            return "(Boolean.compare(" + lhs + ", " + rhs + ") " + op + " 0)";
        if ((op == "/" || op == "%") && clean(leftType)->isUnsignedIntegerType())
            return std::string(unsignedLong(leftType) ? "Long" : "Integer") + "." +
                   (op == "/" ? "divideUnsigned" : "remainderUnsigned") + "(" + lhs + ", " + rhs +
                   ")";
        if (op == ">>" && clean(leftType)->isUnsignedIntegerType())
            op = ">>>";
        if ((op == "==" || op == "!=") &&
            (clean(leftType)->isArithmeticType() || clean(leftType)->isEnumeralType()) &&
            (clean(rightType)->isArithmeticType() || clean(rightType)->isEnumeralType()))
            return "((" + type(leftType) + ")(" + lhs + ") " + op + " (" + type(rightType) + ")(" +
                   rhs + "))";
        return "(" + lhs + " " + op + " " + rhs + ")";
    }
    QualType commonArithmeticType(QualType left, QualType right) {
        left = clean(left);
        right = clean(right);
        if (!left->isArithmeticType() || !right->isArithmeticType())
            return {};
        if (left->isRealFloatingType() || right->isRealFloatingType()) {
            if (!left->isRealFloatingType())
                return right;
            if (!right->isRealFloatingType())
                return left;
            return ctx.getFloatingTypeOrder(left, right) >= 0 ? left : right;
        }
        if (ctx.isPromotableIntegerType(left))
            left = ctx.getPromotedIntegerType(left);
        if (ctx.isPromotableIntegerType(right))
            right = ctx.getPromotedIntegerType(right);
        if (left->isUnsignedIntegerType() == right->isUnsignedIntegerType())
            return ctx.getIntegerTypeOrder(left, right) >= 0 ? left : right;
        auto unsignedType = left->isUnsignedIntegerType() ? left : right;
        auto signedType = left->isUnsignedIntegerType() ? right : left;
        if (ctx.getIntegerTypeOrder(unsignedType, signedType) >= 0)
            return unsignedType;
        return ctx.getTypeSize(signedType) > ctx.getTypeSize(unsignedType)
                   ? signedType
                   : ctx.getCorrespondingUnsignedType(signedType);
    }
    std::string lessExpression(const std::string& left, QualType leftType, const std::string& right,
                               QualType rightType) {
        if (family(leftType) == "reference_wrapper")
            return lessExpression(left + ".get()", element(leftType), right, rightType);
        if (family(rightType) == "reference_wrapper")
            return lessExpression(left, leftType, right + ".get()", element(rightType));
        if (auto common = commonArithmeticType(leftType, rightType); !common.isNull())
            return arithmetic(converted(left, leftType, common), common,
                              converted(right, rightType, common), common, "<");
        if (dataPointer(leftType) && dataPointer(rightType))
            return "(Jpp.pointerDifference(" + left + ", " + right + ") < 0)";
        if (product(leftType) && product(rightType))
            return orderingExpression(left, leftType, right, rightType) + ".is_lt()";
        if ((isVec(leftType) || family(leftType) == "deque") &&
            (isVec(rightType) || family(rightType) == "deque"))
            return "Jpp.lexicographicalLess(" + left + ", " + right + ", " +
                   comparisonPredicate(element(leftType), element(rightType)) + ", " +
                   comparisonPredicate(element(rightType), element(leftType)) + ")";
        if (auto r = record(leftType); r && user(r))
            for (auto method : r->methods())
                if (method->isOverloadedOperator() && method->getOverloadedOperator() == OO_Less)
                    return booleanValue(left + "." + name(method) + "(" + right + ")",
                                        method->getReturnType());
        return "(Jpp.compare(" + left + ", " + right + ") < 0)";
    }
    std::string equalExpression(const std::string& left, QualType leftType,
                                const std::string& right, QualType rightType) {
        if (family(leftType) == "reference_wrapper")
            return equalExpression(left + ".get()", element(leftType), right, rightType);
        if (family(rightType) == "reference_wrapper")
            return equalExpression(left, leftType, right + ".get()", element(rightType));
        if (ordering(leftType) && ordering(rightType))
            return "(" + left + ".rank == " + right + ".rank)";
        if (auto common = commonArithmeticType(leftType, rightType); !common.isNull())
            return arithmetic(converted(left, leftType, common), common,
                              converted(right, rightType, common), common, "==");
        if (dataPointer(leftType) || dataPointer(rightType))
            return "Jpp.samePointer(" + left + ", " + right + ")";
        if (product(leftType) && product(rightType)) {
            auto a = productTypes(leftType), b = productTypes(rightType);
            if (homogeneousProduct(leftType) && homogeneousProduct(rightType)) {
                auto x = temporary("jppLeft"), y = temporary("jppRight");
                return "Jpp.equalProducts(" + left + ", " + right + ", (" + x + ", " + y + ") -> " +
                       equalExpression("((" + boxed(a[0]) + ")" + x + ")", a[0],
                                       "((" + boxed(b[0]) + ")" + y + ")", b[0]) +
                       ")";
            }
            std::vector<std::string> checks;
            for (unsigned i = 0; i < a.size(); ++i) {
                QualType lhsType, rhsType;
                auto lhs = productComparisonItem(left, leftType, i, lhsType);
                auto rhs = productComparisonItem(right, rightType, i, rhsType);
                checks.push_back(equalExpression(lhs, lhsType, rhs, rhsType));
            }
            return checks.empty() ? "true" : "(" + join(checks, " && ") + ")";
        }
        if ((isVec(leftType) || family(leftType) == "deque") &&
            (isVec(rightType) || family(rightType) == "deque")) {
            auto a = temporary("jppElement"), b = temporary("jppElement");
            return "Jpp.sequenceEqual(" + left + ", " + right + ", (" + a + ", " + b + ") -> " +
                   equalExpression(a, element(leftType), b, element(rightType)) + ")";
        }
        if (auto r = record(leftType); r && user(r))
            for (auto method : r->methods())
                if (method->isOverloadedOperator() &&
                    method->getOverloadedOperator() == OO_EqualEqual)
                    return booleanValue(left + "." + name(method) + "(" + right + ")",
                                        method->getReturnType());
        return "Jpp.valueEquals(" + left + ", " + right + ")";
    }
    std::string orderingExpression(const std::string& left, QualType leftType,
                                   const std::string& right, QualType rightType) {
        if (family(leftType) == "reference_wrapper")
            return orderingExpression(left + ".get()", element(leftType), right, rightType);
        if (family(rightType) == "reference_wrapper")
            return orderingExpression(left, leftType, right + ".get()", element(rightType));
        if (product(leftType) && product(rightType)) {
            auto a = productTypes(leftType), b = productTypes(rightType);
            auto x = temporary("jppLeft"), y = temporary("jppRight");
            if (homogeneousProduct(leftType) && homogeneousProduct(rightType))
                return "Jpp.orderProducts(" + left + ", " + right + ", (" + x + ", " + y + ") -> " +
                       orderingExpression("((" + boxed(a[0]) + ")" + x + ")", a[0],
                                          "((" + boxed(b[0]) + ")" + y + ")", b[0]) +
                       ")";
            std::string body;
            for (unsigned i = 0; i < a.size(); ++i) {
                auto order = temporary("jppOrder");
                QualType lhsType, rhsType;
                auto lhs = productComparisonItem(x, leftType, i, lhsType);
                auto rhs = productComparisonItem(y, rightType, i, rhsType);
                body += "JppOrdering " + order + " = " +
                        orderingExpression(lhs, lhsType, rhs, rhsType) + "; if (" + order +
                        ".rank != 0) return " + order + "; ";
            }
            return "Jpp.orderValues(" + left + ", " + right + ", (" + x + ", " + y + ") -> { " +
                   body + "return JppOrdering.EQUIVALENT; })";
        }
        if (auto r = record(leftType); r && user(r))
            for (auto method : r->methods())
                if (method->isOverloadedOperator() &&
                    method->getOverloadedOperator() == OO_Spaceship)
                    return left + "." + name(method) + "(" + right + ")";
        auto x = temporary("jppLeft"), y = temporary("jppRight");
        auto less = lessExpression(x, leftType, y, rightType);
        auto greater = lessExpression(y, rightType, x, leftType);
        std::string otherwise = "JppOrdering.EQUIVALENT";
        if (auto common = commonArithmeticType(leftType, rightType);
            !common.isNull() && common->isRealFloatingType())
            otherwise = equalExpression(x, leftType, y, rightType) +
                        " ? JppOrdering.EQUIVALENT : JppOrdering.UNORDERED";
        return "Jpp.orderValues(" + left + ", " + right + ", (" + x + ", " + y + ") -> " + less +
               " ? JppOrdering.LESS : " + greater + " ? JppOrdering.GREATER : " + otherwise + ")";
    }
    std::string comparisonPredicate(QualType leftType, QualType rightType, bool reverse = false,
                                    QualType parameterType = {}) {
        auto left = temporary("jppLeft"), right = temporary("jppRight");
        auto lhs = parameterType.isNull() ? left : converted(left, leftType, parameterType);
        auto rhs = parameterType.isNull() ? right : converted(right, rightType, parameterType);
        auto lhsType = parameterType.isNull() ? leftType : parameterType;
        auto rhsType = parameterType.isNull() ? rightType : parameterType;
        auto result = reverse ? lessExpression(rhs, rhsType, lhs, lhsType)
                              : lessExpression(lhs, lhsType, rhs, rhsType);
        return "(" + boxed(leftType) + " " + left + ", " + boxed(rightType) + " " + right +
               ") -> " + result;
    }
    std::string comparatorPredicate(QualType comparator, QualType leftType, QualType rightType,
                                    const std::string& initializer = {}) {
        auto kind = family(comparator);
        if (kind == "less" || kind == "greater") {
            auto parameter = element(comparator);
            auto predicate = comparisonPredicate(leftType, rightType, kind == "greater",
                                                 parameter->isVoidType() ? QualType{} : parameter);
            if (initializer.empty())
                return predicate;
            return supplier([&] {
                line("final " + type(comparator) + " " + temporary("jppComparator") + " = " +
                     initializer + ";");
                line("return (java.util.function.BiPredicate<" + boxed(leftType) + ", " +
                     boxed(rightType) + ">) (" + predicate + ");");
            });
        }
        auto prototype = callableType(comparator);
        if (prototype->getNumParams() != 2)
            fail(SourceLocation(), "comparison callback must accept two arguments");
        return supplier([&] {
            auto function = temporary("jppComparator");
            line("final " + type(comparator) + " " + function + " = " +
                 (initializer.empty() ? defaultValue(comparator) : initializer) + ";");
            std::vector<std::string> parameters, arguments;
            for (unsigned i = 0; i < 2; ++i) {
                auto parameter = temporary("jppOperand");
                auto type = i == 0 ? leftType : rightType;
                auto target = prototype->getParamType(i);
                parameters.push_back(boxed(type) + " " + parameter);
                auto argument = converted(parameter, type, target);
                if (referenceABI(target))
                    argument = "JppRef.cell(" + argument + ")";
                else if (copyType(target))
                    argument = "jppCopy(" + argument + ")";
                arguments.push_back(argument);
            }
            auto invocation =
                function + "." + callableMethod(comparator) + "(" + join(arguments) + ")";
            if (referenceABI(prototype->getReturnType()))
                invocation += ".get()";
            line("return (java.util.function.BiPredicate<" + boxed(leftType) + ", " +
                 boxed(rightType) + ">) ((" + join(parameters) + ") -> " +
                 booleanValue(invocation, prototype->getReturnType()) + ");");
        });
    }
    bool addressHasSideEffects(const Expr* e) {
        if (auto call = dyn_cast<CallExpr>(bare(e)); call && returnsReference(call))
            return true;
        if (auto [base, index] = subscript(e); base)
            return isMap(base->getType()) || base->HasSideEffects(ctx) ||
                   index->HasSideEffects(ctx);
        if (auto m = dyn_cast<MemberExpr>(bare(e)))
            return m->getBase()->HasSideEffects(ctx);
        return e->HasSideEffects(ctx);
    }
    std::string compoundAssignment(const CompoundAssignOperator* e, bool asReference = false) {
        if (dataPointer(e->getLHS()->getType())) {
            bool bytes = clean(e->getLHS()->getType())->getPointeeType()->isVoidType();
            auto left = temporary("jppPointer"), right = temporary("jppOffset");
            auto offset = e->getOpcode() == BO_SubAssign ? "-(" + right + ")" : right;
            return std::string(asReference ? "Jpp.compoundReference(" : "Jpp.compound(") +
                   intArgument(e->getRHS()) + ", " + reference(e->getLHS()) + ", (" + left + ", " +
                   right + ") -> " +
                   (bytes ? "Jpp.voidPlus(" + left + ", " + offset + ")"
                          : left + ".plus(" + offset + ")") +
                   ")";
        }
        auto lhs = e->getLHS(), rhs = e->getRHS();
        auto t = clean(lhs->getType());
        if (!t->isArithmeticType() && !t->isEnumeralType())
            fail(e, "compound assignment requires a supported arithmetic lvalue");
        auto op = e->getOpcodeStr().drop_back().str();
        auto computeLeft = e->getComputationLHSType(),
             computeResult = e->getComputationResultType();
        auto calculate = [&](const std::string& left, const std::string& right) {
            return converted(
                arithmetic(converted(left, t, computeLeft), computeLeft, right, rhs->getType(), op),
                computeResult, t);
        };
        auto dr = dyn_cast<DeclRefExpr>(bare(lhs));
        bool referenceAlias =
            dr && aliases.count(dr->getDecl()) && aliases[dr->getDecl()].ends_with(".get()");
        if (!asReference && !addressHasSideEffects(lhs) && !cell(variableRef(lhs)) &&
            !referenceAlias && !rhs->HasSideEffects(ctx)) {
            auto indexed = dyn_cast<ArraySubscriptExpr>(bare(lhs));
            bool javaLvalue =
                dr || isa<MemberExpr>(bare(lhs)) || (indexed && javaArray(indexed->getBase()));
            javaLvalue &= !referenceMember(lhs);
            if (javaLvalue && !t->isBooleanType() && !big(t) && !longDouble(t) &&
                ctx.hasSameType(t, clean(computeResult)) &&
                !(t->isUnsignedIntegerType() && (op == "/" || op == "%"))) {
                if (t->isUnsignedIntegerType() && op == ">>")
                    op = ">>>";
                return "(" + expr(lhs) + " " + op + "= " + expr(rhs) + ")";
            }
            return "(" + assign(lhs, calculate(expr(lhs), expr(rhs))) + ")";
        }
        // C++17 sequences the RHS before the lvalue. The helper also evaluates the address once.
        auto left = temporary("jppLeft"), right = temporary("jppRight");
        return std::string(asReference ? "Jpp.compoundReference(" : "Jpp.compound(") + expr(rhs) +
               ", " + reference(lhs) + ", (" + left + ", " + right + ") -> " +
               calculate(left, right) + ")";
    }
    std::string binary(const Expr* lhs, const Expr* rhs, std::string op, QualType result) {
        if ((ordering(lhs->getType()) || ordering(rhs->getType())) && op != "=" && op != ",") {
            bool left = ordering(lhs->getType()), right = ordering(rhs->getType());
            if (left && right)
                return "(" + expr(lhs) + ".rank " + op + " " + expr(rhs) + ".rank)";
            auto value = expr(left ? lhs : rhs);
            if (op == "<=>")
                return left ? "jppCopy(" + value + ")" : value + ".reversed()";
            if (!left) {
                if (op == "<")
                    op = ">";
                else if (op == ">")
                    op = "<";
                else if (op == "<=")
                    op = ">=";
                else if (op == ">=")
                    op = "<=";
            }
            return value + (op == "=="   ? ".is_eq()"
                            : op == "!=" ? ".is_neq()"
                            : op == "<"  ? ".is_lt()"
                            : op == "<=" ? ".is_lteq()"
                            : op == ">"  ? ".is_gt()"
                                         : ".is_gteq()");
        }
        bool relation =
            op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=";
        if (product(lhs->getType()) && product(rhs->getType()) && (relation || op == "<=>")) {
            auto left = expr(lhs), right = expr(rhs);
            if (op == "<=>")
                return orderingExpression(left, lhs->getType(), right, rhs->getType());
            if (op == "==" || op == "!=") {
                auto a = temporary("jppLeft"), b = temporary("jppRight");
                return std::string(op == "!=" ? "!" : "") + "Jpp.testProducts(" + left + ", " +
                       right + ", (" + a + ", " + b + ") -> " +
                       equalExpression(a, lhs->getType(), b, rhs->getType()) + ")";
            }
            return orderingExpression(left, lhs->getType(), right, rhs->getType()) +
                   (op == "<"    ? ".is_lt()"
                    : op == "<=" ? ".is_lteq()"
                    : op == ">"  ? ".is_gt()"
                                 : ".is_gteq()");
        }
        if (op == "<=>")
            return orderingExpression(expr(lhs), lhs->getType(), expr(rhs), rhs->getType());
        if ((chrono(lhs->getType()) || chrono(rhs->getType())) && op != "=" && op != ",") {
            bool leftPoint = family(lhs->getType()) == "time_point";
            bool rightPoint = family(rhs->getType()) == "time_point";
            bool leftDuration = family(lhs->getType()) == "duration";
            bool rightDuration = family(rhs->getType()) == "duration";
            auto left = expr(lhs), right = expr(rhs);
            bool comparison =
                op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=";
            if (comparison && ((leftPoint && rightPoint) || (leftDuration && rightDuration)))
                return "(" + left + ".compare(" + right + ") " + op + " 0)";
            if ((op == "+=" || op == "-=") && rightDuration) {
                return std::string(leftPoint ? "JppTimePoint.shiftAssign("
                                             : "JppDuration.addAssign(") +
                       right + ", " + left + ", " + (op == "-=" ? "true" : "false") + ")";
            }
            if (leftDuration && (op == "*=" || op == "/=" || op == "%="))
                return "JppDuration.scaleAssign(" + value(rhs, ctx.LongLongTy) + ", " + left +
                       ", " + quote(op.substr(0, 1)) + ")";
            if (leftDuration && rightDuration) {
                if (op == "+" || op == "-")
                    return left + ".add(" + right + ", " + (op == "-" ? "true" : "false") + ", " +
                           chronoPeriod(result) + ")";
                if (op == "/")
                    return converted(left + ".quotient(" + right + ")", ctx.LongLongTy, result);
                if (op == "%")
                    return left + ".remainder(" + right + ", " + chronoPeriod(result) + ")";
            }
            if (leftPoint && rightPoint && op == "-")
                return left + ".difference(" + right + ", " + chronoPeriod(result) + ")";
            if (leftPoint && rightDuration && (op == "+" || op == "-"))
                return left + ".shift(" + right + ", " + (op == "-" ? "true" : "false") + ", " +
                       chronoPeriod(result) + ")";
            if (leftDuration && rightPoint && op == "+")
                return right + ".shift(" + left + ", false, " + chronoPeriod(result) + ")";
            if (leftDuration && !chrono(rhs->getType()) && (op == "*" || op == "/" || op == "%")) {
                chronoPeriod(result);
                return left + ".scale(" + value(rhs, ctx.LongLongTy) + ", " + quote(op) + ")";
            }
            if (!chrono(lhs->getType()) && rightDuration && op == "*") {
                chronoPeriod(result);
                return right + ".scale(" + value(lhs, ctx.LongLongTy) + ", \"*\")";
            }
            fail(lhs, "this chrono operator needs additional lowering");
        }
        bool leftVoid =
            dataPointer(lhs->getType()) && clean(lhs->getType())->getPointeeType()->isVoidType();
        bool rightVoid =
            dataPointer(rhs->getType()) && clean(rhs->getType())->getPointeeType()->isVoidType();
        if ((op == "+" || op == "-") && leftVoid && rhs->getType()->isIntegerType())
            return "Jpp.voidPlus(" + expr(lhs) + ", " +
                   (op == "-" ? "-(" + intArgument(rhs) + ")" : intArgument(rhs)) + ")";
        if (op == "+" && rightVoid && lhs->getType()->isIntegerType())
            return "Jpp.voidPlus(" + expr(rhs) + ", " + intArgument(lhs) + ")";
        auto position = [&](QualType t) { return isIter(t) || dataPointer(t); };
        if ((op == "+=" || op == "-=") && isIter(lhs->getType()))
            return "Jpp.advance(" + std::string(op == "-=" ? "-(" : "(") + intArgument(rhs) +
                   "), " + expr(lhs) + ")";
        if (op == "+" && position(rhs->getType()) && lhs->getType()->isIntegerType())
            return asIterator(rhs) + ".plus(" + intArgument(lhs) + ")";
        if (op == "-" && position(lhs->getType()) && position(rhs->getType())) {
            if (dataPointer(lhs->getType()) && dataPointer(rhs->getType())) {
                auto difference = "Jpp.pointerDifference(" + expr(lhs) + ", " + expr(rhs) + ")";
                return ctx.getTypeSize(clean(result)) > 32 ? difference
                                                           : "(int)(" + difference + ")";
            }
            auto difference = asIterator(lhs) + ".minus(" + asIterator(rhs) + ")";
            return ctx.getTypeSize(clean(result)) > 32 ? "(long)(" + difference + ")" : difference;
        }
        if (op == "=") {
            if (product(lhs->getType()) && tupleLike(rhs->getType()))
                return assignProduct(lhs, rhs);
            if (clean(lhs->getType())->isRecordType())
                return "Jpp.assignRecord(" + value(rhs, lhs->getType(), false) + ", " +
                       reference(lhs) + ")";
            auto v = value(rhs, lhs->getType(), true);
            if (!isa<DeclRefExpr>(bare(lhs)) &&
                (addressHasSideEffects(lhs) || rhs->HasSideEffects(ctx)))
                return "Jpp.store(" + v + ", " + reference(lhs) + ")";
            return "(" + assign(lhs, v) + ")";
        }
        bool compound = op.size() > 1 && op.ends_with('=') && op != "==" && op != "!=" &&
                        op != "<=" && op != ">=";
        if (compound) {
            auto ref = dyn_cast<DeclRefExpr>(bare(lhs));
            bool alias =
                ref && aliases.count(ref->getDecl()) && aliases[ref->getDecl()].ends_with(".get()");
            if (subscript(lhs).first || cell(variableRef(lhs)) || alias || big(lhs->getType()) ||
                isString(lhs->getType())) {
                auto v = binary(lhs, rhs, op.substr(0, op.size() - 1), result);
                // Re-evaluating an indexed lvalue with side effects would change C++ behavior.
                auto [base, index] = subscript(lhs);
                if (index && index->HasSideEffects(ctx))
                    fail(lhs,
                         "compound subscript assignment with side effects requires a temporary");
                return "(" + assign(lhs, v) + ")";
            }
            return "(" + expr(lhs) + " " + op + " " + expr(rhs) + ")";
        }
        if (op == "," && (lhs->getType()->isVoidType() || rhs->getType()->isVoidType()))
            return sequence(lhs, rhs);
        if (op == ",")
            return "Jpp.last(" + expr(lhs) + ", " + expr(rhs) + ")";
        if (op == "&&" || op == "||")
            return "(" + truth(lhs) + " " + op + " " + truth(rhs) + ")";
        bool comparison =
            op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=";
        if (comparison && !isString(lhs->getType()) && !isString(rhs->getType()) &&
            (dataPointer(lhs->getType()) || dataPointer(rhs->getType()))) {
            if (op == "==" || op == "!=")
                return std::string(op == "!=" ? "!" : "") + "Jpp.samePointer(" + expr(lhs) + ", " +
                       expr(rhs) + ")";
            return "(" + expr(lhs) + ".minus(" + expr(rhs) + ") " + op + " 0)";
        }
        if (isIter(lhs->getType()) || isIter(rhs->getType())) {
            if (op == "==" || op == "!=")
                return (op == "!=" ? "!" : "") + expr(lhs) + ".same(" + expr(rhs) + ")";
            if (op == "-" && isIter(rhs->getType()))
                return type(result) == "long" ? "(long)(" + expr(lhs) + ".minus(" + expr(rhs) + "))"
                                              : expr(lhs) + ".minus(" + expr(rhs) + ")";
            if (op == "+" || op == "-")
                return expr(lhs) + ".plus(" + (op == "-" ? "-(" : "(") + intArgument(rhs) + "))";
            if (comparison)
                return "(" + expr(lhs) + ".minus(" + expr(rhs) + ") " + op + " 0)";
        }
        if (lhs->getType()->isPointerType() && (op == "+" || op == "-") &&
            rhs->getType()->isIntegerType())
            return asIterator(lhs) + ".plus(" + (op == "-" ? "-(" : "(") + intArgument(rhs) + "))";
        if (isString(lhs->getType()) || isString(rhs->getType())) {
            if (op == "+")
                return "Jpp.concat(" + expr(lhs) + ", " + expr(rhs) + ")";
            if (comparison)
                return "(Jpp.cString(" + expr(lhs) + ").compareTo(Jpp.cString(" + expr(rhs) +
                       ")) " + op + " 0)";
        }
        if ((op == "==" || op == "!=") && family(lhs->getType()) == "bitset")
            return std::string(op == "!=" ? "!" : "") + expr(lhs) + ".equals(" + expr(rhs) + ")";
        if ((op == "==" || op == "!=") && family(lhs->getType()) == "mersenne_twister_engine")
            return std::string(op == "!=" ? "!" : "") + expr(lhs) + ".sameState(" + expr(rhs) + ")";
        if ((op == "<" || op == ">") &&
            (isVec(lhs->getType()) || family(lhs->getType()) == "deque") &&
            (isVec(rhs->getType()) || family(rhs->getType()) == "deque"))
            return op == "<" ? lessExpression(expr(lhs), lhs->getType(), expr(rhs), rhs->getType())
                             : lessExpression(expr(rhs), rhs->getType(), expr(lhs), lhs->getType());
        if (comparison && (lhs->getType()->isRecordType() || rhs->getType()->isRecordType()))
            return "(Jpp.compare(" + expr(lhs) + ", " + expr(rhs) + ") " + op + " 0)";
        return arithmetic(expr(lhs), lhs->getType(), expr(rhs), rhs->getType(), op);
    }
    std::string construct(const CXXConstructExpr* c) {
        auto result = constructValue(c);
        return vectorStorage && family(c->getType()) == "vector"
                   ? "Jpp.movableVector(" + result + ")"
                   : result;
    }
    std::string constructValue(const CXXConstructExpr* c) {
        if (c->getConstructor()->isConsteval())
            fail(c, "consteval object construction needs additional constant-value lowering");
        auto t = c->getType();
        std::vector<const Expr*> args;
        for (auto a : c->arguments())
            if (user(c->getConstructor()) || !isa<CXXDefaultArgExpr>(a))
                args.push_back(a);
        if (c->getConstructor()->isCopyOrMoveConstructor() && !args.empty()) {
            auto constructor = c->getConstructor();
            if (family(t) == "vector" && constructor->isMoveConstructor())
                return "Jpp.moveVector(" + expr(args[0]) + ")";
            if (family(t) == "array" && constructor->isMoveConstructor())
                return moveStandardArray(args[0], t);
            if (product(t) && constructor->isMoveConstructor())
                return convertProduct(expr(args[0]), args[0]->getType(), t, args[0]->getValueKind(),
                                      c->getExprLoc());
            if (defaultedMove(constructor)) {
                requireDefaultedMove(constructor);
                return "new " + type(t) + "(" + expr(args[0]) + ", (JppMove)null)";
            }
            if (user(constructor) && !constructor->isImplicit() && !constructor->isDefaulted()) {
                std::vector<std::string> bound;
                for (unsigned i = 0; i < args.size(); ++i) {
                    auto target = constructor->getParamDecl(i)->getType();
                    bound.push_back(referenceABI(target) ? reference(args[i])
                                                         : value(args[i], target, true));
                }
                if (constructor->isMoveConstructor())
                    bound.push_back("(JppMove)null");
                return "new " + type(t) + "(" + join(bound) + ")";
            }
            return "jppCopy(" + expr(args[0]) + ")";
        }
        if (args.empty())
            return defaultValue(t);
        if (iotaView(t))
            return makeIota(t, args[0], args.size() > 1 ? args[1] : nullptr);
        auto n = family(t);
        if (chrono(t)) {
            if (args.size() != 1)
                fail(c, "this chrono constructor needs additional lowering");
            auto source = expr(args[0]);
            if (family(args[0]->getType()) == "time_point")
                source += ".time_since_epoch()";
            return chronoValue(t,
                               chrono(args[0]->getType()) ? source : value(args[0], ctx.LongLongTy),
                               chrono(args[0]->getType()));
        }
        if (n == "mersenne_twister_engine") {
            if (args.size() == 1 && family(args[0]->getType()) == "seed_seq")
                return mtEngine(t, expr(args[0]));
            if (args.size() != 1 || !clean(args[0]->getType())->isIntegerType())
                fail(c, "Mersenne Twister seed sequences need additional lowering");
            return mtEngine(t, value(args[0], ctx.UnsignedLongLongTy));
        }
        if (n == "seed_seq") {
            if (args.size() == 1)
                if (auto initializer = dyn_cast<CXXStdInitializerListExpr>(bare(args[0])))
                    if (auto list = dyn_cast<InitListExpr>(bare(initializer->getSubExpr()))) {
                        std::vector<std::string> words;
                        for (auto value : list->inits())
                            words.push_back(this->value(value, ctx.UnsignedIntTy));
                        return "new JppSeedSeq(new int[]{" + join(words) + "})";
                    }
            if (args.size() == 2) {
                auto word = temporary("jppWord");
                return "JppSeedSeq.from(" + asIterator(args[0]) + ", " + asIterator(args[1]) +
                       ", " + word + " -> " +
                       converted(word, iteratorElement(args[0]->getType()), ctx.UnsignedIntTy) +
                       ")";
            }
            fail(c, "unsupported seed-sequence constructor");
        }
        if (n == "bitset") {
            if (args[0]->getType()->isArithmeticType() || args[0]->getType()->isEnumeralType())
                return "new JppBitset(" + bitsetSize(t) + ", " +
                       value(args[0], ctx.UnsignedLongLongTy) + ")";
            bool string = isString(args[0]->getType());
            auto position =
                string && args.size() > 1 ? value(args[1], ctx.UnsignedLongLongTy) : "0L";
            unsigned countIndex = string ? 2 : 1;
            auto count =
                args.size() > countIndex ? value(args[countIndex], ctx.UnsignedLongLongTy) : "-1L";
            auto zero = args.size() > countIndex + 1 ? expr(args[countIndex + 1]) : "'0'";
            auto one = args.size() > countIndex + 2 ? expr(args[countIndex + 2]) : "'1'";
            return "new JppBitset(" + bitsetSize(t) + ", Jpp.cString(" + expr(args[0]) + "), " +
                   position + ", " + count + ", " + zero + ", " + one + ")";
        }
        if (fileStream(t)) {
            std::vector<std::string> a = {"Jpp.cString(" + expr(args[0]) + ")"};
            if (args.size() > 1)
                a.push_back(expr(args[1]));
            return "new " + type(t) + "(" + join(a) + ")";
        }
        if (isString(t)) {
            if (args.size() > 1 && isString(args[0]->getType())) {
                std::vector<std::string> positions;
                for (unsigned i = 1; i < args.size(); ++i)
                    positions.push_back(value(args[i], ctx.UnsignedLongLongTy));
                return "(" + expr(args[0]) + ").substr(" + join(positions) + ")";
            }
            std::vector<std::string> a;
            for (auto e : args)
                a.push_back(expr(e));
            if (dataPointer(args[0]->getType())) {
                a[0] = args.size() == 1 ? "Jpp.cString(" + a[0] + ")"
                                        : value(args[0], args[0]->getType());
                if (args.size() == 2 && dataPointer(args[1]->getType()))
                    a[1] = value(args[1], args[1]->getType());
            }
            if (args.size() == 2) {
                if (clean(args[0]->getType())->isIntegerType())
                    a[0] = value(args[0], ctx.UnsignedLongLongTy);
                else if (clean(args[1]->getType())->isIntegerType())
                    a[1] = value(args[1], ctx.UnsignedLongLongTy);
            }
            return "new JppString(" + join(a) + ")";
        }
        if (isVec(t)) {
            if (auto il = dyn_cast<CXXStdInitializerListExpr>(bare(args[0]))) {
                auto list = dyn_cast<InitListExpr>(bare(il->getSubExpr()));
                if (!list)
                    fail(c, "unrecognized initializer list");
                std::vector<std::string> a;
                for (auto e : list->inits())
                    a.push_back(value(e, element(t), true));
                return "new " + type(t) + "(Arrays.asList(" + join(a) + "))";
            }
            if (args.size() >= 2 &&
                (isIter(args[0]->getType()) || args[0]->getType()->isPointerType()))
                return "Jpp.vector(" + asIterator(args[0]) + ", " + asIterator(args[1]) + ")";
            if (args.size() > 1)
                return "Jpp.vector(" + intArgument(args[0]) + ", " +
                       value(args[1], element(t), false) + ")";
            return "Jpp.vectorDefault(" + intArgument(args[0]) + ", () -> " +
                   defaultValue(element(t)) + ")";
        }
        if (n == "deque") {
            if (isa<CXXStdInitializerListExpr>(bare(args[0])))
                return "new " + type(t) + "(" + expr(args[0]) + ")";
            if (args.size() >= 2 &&
                (isIter(args[0]->getType()) || dataPointer(args[0]->getType()))) {
                auto word = temporary("jppValue");
                return "Jpp.dequeFrom(" + asIterator(args[0]) + ", " + asIterator(args[1]) + ", " +
                       word + " -> " +
                       converted(word, iteratorElement(args[0]->getType()), element(t)) + ")";
            }
            if (args.size() == 2)
                return "Jpp.dequeFill(" + intArgument(args[0]) + ", " + value(args[1], element(t)) +
                       ")";
            return "Jpp.dequeDefault(" + intArgument(args[0]) + ", () -> " +
                   defaultValue(element(t)) + ")";
        }
        if (product(t)) {
            auto elements = productTypes(t);
            bool converting =
                args.size() == 1 && tupleLike(args[0]->getType()) && elements.size() != 1;
            if (args.size() == 1 && tupleLike(args[0]->getType()))
                if (auto primary = c->getConstructor()->getPrimaryTemplate();
                    primary && primary->getTemplatedDecl()->getNumParams()) {
                    auto parameter = primary->getTemplatedDecl()
                                         ->getParamDecl(0)
                                         ->getType()
                                         .getNonReferenceType();
                    if (auto specialization = parameter->getAs<TemplateSpecializationType>())
                        if (auto declaration =
                                specialization->getTemplateName().getAsTemplateDecl())
                            converting |= declaration->getName() == "tuple" ||
                                          declaration->getName() == "pair";
                }
            if (converting)
                return convertProduct(expr(args[0]), args[0]->getType(), t, args[0]->getValueKind(),
                                      c->getExprLoc());
            if (args.size() != elements.size())
                fail(c, "this tuple constructor needs additional lowering");
            std::vector<std::string> a;
            for (unsigned i = 0; i < args.size(); ++i)
                a.push_back(productArgument(args[i], elements[i]));
            return productConstruct(t, a);
        }
        if (n == "reference_wrapper")
            return reference(args[0]);
        if (orderTree(t)) {
            bool range =
                args.size() >= 2 && (isIter(args[0]->getType()) || dataPointer(args[0]->getType()));
            auto created = defaultValue(t);
            auto comparatorIndex = range ? 2U : 0U;
            if (args.size() > comparatorIndex) {
                if (args.size() != comparatorIndex + 1)
                    fail(c, "unsupported policy-tree constructor");
                // Only the stateless standard comparison policies are accepted.
                created = "Jpp.last(" + expr(args[comparatorIndex]) + ", " + created + ")";
            }
            if (range) {
                auto item = temporary("jppKey");
                return "Jpp.orderSetFrom(" + created + ", " + asIterator(args[0]) + ", " +
                       asIterator(args[1]) + ", " + item + " -> " +
                       converted(item, iteratorElement(args[0]->getType()), element(t)) + ")";
            }
            return created;
        }
        if (n == "function")
            return converted(scalarCopyClosure(record(args[0]->getType()))
                                 ? constructElement(clean(args[0]->getType()), args[0])
                                 : expr(args[0]),
                             args[0]->getType(), t);
        if (isIter(t))
            return converted(expr(args[0]), args[0]->getType(), t);
        if (isMap(t)) {
            bool range =
                args.size() > 1 && (isIter(args[0]->getType()) || dataPointer(args[0]->getType()));
            bool list = isa<CXXStdInitializerListExpr>(bare(args[0]));
            auto comparatorIndex = range ? 2U : list ? 1U : 0U;
            auto created = defaultValue(t);
            if (args.size() > comparatorIndex) {
                if (n != "map")
                    fail(c, "unordered-map bucket, hash, and equality constructors need "
                            "additional lowering");
                auto comparator = args[comparatorIndex];
                created = "new " + type(t) + "(true, () -> " + defaultValue(element(t, 1)) +
                          ", Jpp.comparator(" +
                          comparatorPredicate(
                              comparator->getType(), element(t), element(t),
                              storedElement(comparator, clean(comparator->getType()), false)) +
                          "))";
            }
            if (range) {
                auto source = iteratorElement(args[0]->getType());
                auto entry = temporary("jppEntry");
                auto pair =
                    "new JppPair<" + boxed(element(t)) + ", " + boxed(element(t, 1)) + ">(" +
                    converted(productRead(entry, source, 0), element(source), element(t)) + ", " +
                    converted(productRead(entry, source, 1), element(source, 1), element(t, 1)) +
                    ")";
                return "Jpp.mapFrom(" + created + ", " + asIterator(args[0]) + ", " +
                       asIterator(args[1]) + ", " + entry + " -> " + pair + ")";
            }
            if (list)
                return "Jpp.mapFrom(" + created + ", " + expr(args[0]) + ")";
            return created;
        }
        if (isSet(t)) {
            bool range = args.size() > 1 &&
                         (isIter(args[0]->getType()) || args[0]->getType()->isPointerType());
            bool list = isa<CXXStdInitializerListExpr>(bare(args[0]));
            auto comparatorIndex = range ? 2U : list ? 1U : 0U;
            auto created = defaultValue(t);
            if (args.size() > comparatorIndex) {
                auto comparator = args[comparatorIndex];
                created = "new " + type(t) + "(" +
                          (family(t) == "multiset" || family(t) == "unordered_multiset" ? "true"
                                                                                        : "false") +
                          ", Jpp.comparator(" +
                          comparatorPredicate(comparator->getType(), element(t), element(t),
                                              expr(comparator)) +
                          "))";
            }
            if (range)
                return "Jpp.setFrom(" + created + ", " + asIterator(args[0]) + ", " +
                       asIterator(args[1]) + ")";
            if (list)
                return "Jpp.setFrom(" + created + ", " + expr(args[0]) + ")";
            return created;
        }
        if (n == "priority_queue") {
            bool range =
                args.size() > 1 && (isIter(args[0]->getType()) || dataPointer(args[0]->getType()));
            auto comparatorIndex = range ? 2U : 0U;
            auto item = element(t);
            auto created = defaultValue(t);
            if (args.size() > comparatorIndex) {
                auto comparator = args[comparatorIndex];
                created = "new " + type(t) + "(Jpp.heapComparator(" +
                          comparatorPredicate(
                              comparator->getType(), item, item,
                              storedElement(comparator, clean(comparator->getType()), false)) +
                          "))";
            }
            if (args.size() > comparatorIndex + 1)
                created = "Jpp.heapFrom(" + created + ", " + expr(args[comparatorIndex + 1]) + ")";
            if (range) {
                auto value = temporary("jppValue");
                created = "Jpp.heapFrom(" + created + ", " + asIterator(args[0]) + ", " +
                          asIterator(args[1]) + ", " + value + " -> " +
                          converted(value, iteratorElement(args[0]->getType()), item) + ")";
            }
            return created;
        }
        if (n == "greater" || n == "less")
            return "(a, b) -> Jpp.compare(a, b) " + std::string(n == "less" ? "<" : ">") + " 0";
        std::vector<std::string> a;
        for (unsigned i = 0; i < args.size(); ++i) {
            QualType target = i < c->getConstructor()->getNumParams()
                                  ? c->getConstructor()->getParamDecl(i)->getType()
                                  : args[i]->getType();
            a.push_back(user(c->getConstructor()) && referenceABI(target)
                            ? reference(args[i])
                            : value(args[i], target, true));
        }
        return "new " + type(t) + "(" + join(a) + ")";
    }
    std::string storedElement(const Expr* e, QualType target, bool move) {
        if (!copyType(target))
            return value(e, target);
        if (move)
            return constructElement(target, e);
        return "jppCopy(" + value(e, target) + ")";
    }
    std::string asIterator(const Expr* e) {
        if (primitiveArray(e->getType()))
            return arrayDecay(e);
        if (ctx.getAsArrayType(e->getType()))
            return "Jpp.begin(" + expr(e) + ")";
        return expr(e);
    }
    std::string elementConstruction(QualType target, const std::vector<const Expr*>& args) {
        if (args.empty())
            return defaultValue(target);
        if (!clean(target)->isRecordType() && args.size() == 1)
            return value(args[0], target, true);
        std::vector<std::string> values;
        if (product(target)) {
            std::vector<Expr*> arguments;
            for (auto argument : args)
                arguments.push_back(const_cast<Expr*>(argument));
            return expr(resolveConstruction(target, arguments, args[0]->getExprLoc()));
        } else {
            auto r = record(target);
            const CXXConstructorDecl* constructor = nullptr;
            if (r)
                for (auto ctor : r->ctors())
                    if (ctor->getMinRequiredArguments() <= args.size() &&
                        ctor->getNumParams() >= args.size() && !ctor->isCopyOrMoveConstructor()) {
                        if (constructor)
                            fail(args[0], "emplacement needs constructor overload resolution");
                        constructor = ctor;
                    }
            auto field = r ? r->field_begin() : CXXRecordDecl::field_iterator();
            for (unsigned i = 0; i < args.size(); ++i) {
                auto type = constructor ? constructor->getParamDecl(i)->getType()
                            : r && r->isAggregate() && field != r->field_end()
                                ? (*field++)->getType()
                                : args[i]->getType();
                values.push_back(value(args[i], type, true));
            }
        }
        return "new " + type(target) + "(" + join(values) + ")";
    }
    std::string bitCast(const Expr* value, QualType target) {
        auto from = clean(value->getType());
        target = clean(target);
        if (ctx.hasSameType(from, target) &&
            (from->isArithmeticType() || from->isEnumeralType() || from->isPointerType()))
            return expr(value);
        if (from->isSpecificBuiltinType(BuiltinType::Float) && target->isIntegerType() &&
            ctx.getTypeSize(target) == 32)
            return "Float.floatToRawIntBits(" + expr(value) + ")";
        if (from->isSpecificBuiltinType(BuiltinType::Double) && target->isIntegerType() &&
            ctx.getTypeSize(target) == 64)
            return "Double.doubleToRawLongBits(" + expr(value) + ")";
        if (target->isSpecificBuiltinType(BuiltinType::Float) && from->isIntegerType() &&
            ctx.getTypeSize(from) == 32)
            return "Float.intBitsToFloat(" + expr(value) + ")";
        if (target->isSpecificBuiltinType(BuiltinType::Double) && from->isIntegerType() &&
            ctx.getTypeSize(from) == 64)
            return "Double.longBitsToDouble(" + expr(value) + ")";
        fail(value, "this bit_cast needs a storage layout backend");
    }
    std::string constantValue(const APValue& evaluated, QualType target) {
        auto t = clean(target);
        if (!t->isArithmeticType() && !t->isEnumeralType())
            return {};
        if (evaluated.isInt()) {
            return integerLiteral(evaluated.getInt(), t);
        }
        if (evaluated.isFloat()) {
            const auto& value = evaluated.getFloat();
            if (longDouble(t))
                return extendedConstant(value);
            auto destination = type(t);
            if (destination != "float" && destination != "double")
                return {};
            if (value.isFinite()) {
                llvm::SmallString<40> buffer;
                value.toString(buffer);
                std::string literal = buffer.str().str();
                if (literal.find_first_of(".eE") == std::string::npos)
                    literal += ".0";
                return literal + (destination == "float" ? "f" : "");
            }
            auto bits = value.bitcastToAPInt();
            return destination == "float"
                       ? "Float.intBitsToFloat(" +
                             std::to_string(static_cast<int32_t>(bits.getZExtValue())) + ")"
                       : "Double.longBitsToDouble(" + std::to_string(bits.getSExtValue()) + "L)";
        }
        return {};
    }
    std::string constantValue(const Expr* expression, bool required = false) {
        auto t = clean(expression->getType());
        if (!t->isArithmeticType() && !t->isEnumeralType())
            return {};
        Expr::EvalResult result;
        if (!expression->EvaluateAsRValue(result, ctx, required) || result.HasSideEffects)
            return {};
        return constantValue(result.Val, t);
    }
    std::string call(const CallExpr* c) {
        if (auto saved = checkpointValues.find(c); saved != checkpointValues.end())
            return saved->second;
        auto f = c->getDirectCallee();
        if (f && f->isConsteval()) {
            if (auto constant = constantValue(c, true); !constant.empty())
                return constant;
            fail(c, "this consteval result needs additional constant-value lowering");
        }
        if (f && !user(f))
            if (auto owner = dyn_cast<CXXRecordDecl>(f->getDeclContext());
                owner && owner->getName() == "numeric_limits")
                if (auto constant = constantValue(c); !constant.empty())
                    return constant;
        if (f && !user(f))
            if (auto owner = dyn_cast<CXXRecordDecl>(f->getDeclContext());
                owner && owner->getName() == "mersenne_twister_engine" &&
                (f->getNameAsString() == "min" || f->getNameAsString() == "max")) {
                auto width =
                    templateArgs(ctx.getRecordType(owner))[1].getAsIntegral().getZExtValue();
                auto result = integerLiteral(f->getNameAsString() == "min"
                                                 ? llvm::APInt(64, 0)
                                                 : llvm::APInt::getLowBitsSet(64, width),
                                             c->getType());
                if (auto member = dyn_cast<MemberExpr>(bare(c->getCallee()));
                    member && member->getBase()->HasSideEffects(ctx))
                    return "Jpp.last(" + expr(member->getBase()) + ", " + result + ")";
                return result;
            }
        if (f && !user(f))
            if (auto owner = dyn_cast<CXXRecordDecl>(f->getDeclContext())) {
                auto clock = owner->getNameAsString(), method = f->getNameAsString();
                if ((clock == "system_clock" || clock == "steady_clock") && method == "now")
                    return chronoPeriod(c->getType()) == "1L, 1000000000L"
                               ? "JppTimePoint.now(" +
                                     std::string(clock == "steady_clock" ? "true" : "false") + ")"
                               : chronoValue(
                                     c->getType(),
                                     "JppTimePoint.now(" +
                                         std::string(clock == "steady_clock" ? "true" : "false") +
                                         ").time_since_epoch()",
                                     true);
                if (clock == "system_clock" && method == "from_time_t")
                    return chronoValue(c->getType(),
                                       "new JppDuration(" + value(c->getArg(0), ctx.LongLongTy) +
                                           ", 1L, 1L)",
                                       true);
                if (clock == "system_clock" && method == "to_time_t")
                    return converted(expr(c->getArg(0)) +
                                         ".time_since_epoch().convert(1L, 1L, \"cast\").count()",
                                     ctx.LongLongTy, c->getType());
                if (chrono(ctx.getRecordType(owner)) &&
                    (method == "zero" || method == "min" || method == "max")) {
                    auto result = chronoValue(c->getType(), method == "zero"  ? "0L"
                                                            : method == "min" ? "Long.MIN_VALUE"
                                                                              : "Long.MAX_VALUE");
                    if (auto member = dyn_cast<MemberExpr>(bare(c->getCallee()));
                        member && member->getBase()->HasSideEffects(ctx))
                        result = "Jpp.last(" + expr(member->getBase()) + ", " + result + ")";
                    return result;
                }
            }
        if (auto m = dyn_cast<CXXMemberCallExpr>(c)) {
            auto me = dyn_cast<MemberExpr>(bare(m->getCallee()));
            if (!me)
                fail(c, "unsupported member call");
            auto obj = me->getBase();
            auto ot = obj->getType();
            auto sourceType = bare(obj)->getType();
            if (sourceType->isPointerType())
                sourceType = sourceType->getPointeeType();
            if (orderTree(sourceType))
                ot = sourceType;
            if (ot->isPointerType())
                ot = ot->getPointeeType();
            if (f && user(f)) {
                auto method = cast<CXXMethodDecl>(f);
                if (defaultedMove(method) && c->getNumArgs() == 1)
                    requireDefaultedMove(method);
                auto allocatedName = method->getParent()->isLambda() ? "call" : name(f);
                auto object = isa<CXXThisExpr>(bare(obj))
                                  ? (constructionThis.empty() ? "this" : constructionThis)
                              : me->isArrow() ? expr(obj) + ".get()"
                                              : expr(obj);
                return object + "." + allocatedName + "(" + arguments(c, f) + ")";
            }
            auto n = me->getMemberDecl()->getNameAsString();
            if (orderTree(ot)) {
                requireOrderTree(ot);
                auto object = expr(obj) + (me->isArrow() ? ".get()" : "");
                if (n == "order_of_key")
                    return integerResult(c, object + ".order_of_key(" +
                                                value(c->getArg(0), element(ot)) + ")");
                if (n == "find_by_order")
                    return object + ".find_by_order(" +
                           value(c->getArg(0), ctx.UnsignedLongLongTy) + ")";
                if (n == "size")
                    return integerResult(c, object + ".size()");
                if (n == "empty" || n == "clear" || n == "begin" || n == "end" || n == "rbegin" ||
                    n == "rend")
                    return object + "." + n + "()";
                if (n == "insert" || n == "find" || n == "lower_bound" || n == "upper_bound") {
                    if (c->getNumArgs() != 1)
                        fail(c, "this policy-tree insertion needs additional range lowering");
                    return object + "." + n + "(" + value(c->getArg(0), element(ot)) + ")";
                }
                if (n == "erase" && c->getNumArgs() == 1)
                    return object + ".erase(" + expr(c->getArg(0)) + ")";
                if (n == "swap" && c->getNumArgs() == 1)
                    return object + ".swap(" + expr(c->getArg(0)) + ")";
                fail(c, "unsupported policy-tree member '" + n + "'");
            }
            if (iotaView(ot)) {
                auto object = expr(obj) + (me->isArrow() ? ".get()" : "");
                if (isa<CXXConversionDecl>(me->getMemberDecl()) && c->getType()->isBooleanType())
                    return "!" + object + ".empty()";
                if (n == "begin" || n == "end" || n == "empty" || n == "front" || n == "back")
                    return object + "." + n + "()";
                if (n == "size")
                    return converted(object + ".size()", ctx.Int128Ty, c->getType());
                fail(c, "this iota member needs additional range lowering");
            }
            if (ordering(ot) && ordering(c->getType()) &&
                isa<CXXConversionDecl>(me->getMemberDecl()))
                return "jppCopy(" + expr(obj) + (me->isArrow() ? ".get()" : "") + ")";
            if (family(ot) == "reference_wrapper" &&
                (n == "get" || isa<CXXConversionDecl>(me->getMemberDecl()))) {
                auto object = expr(obj) + (me->isArrow() ? ".get()" : "");
                return referenceABI(f->getReturnType()) ? object : object + ".get()";
            }
            if (product(ot) && n == "swap")
                return swapProduct(m->getImplicitObjectArgument(), c->getArg(0));
            if (family(ot) == "array" && n == "swap")
                return swapProduct(m->getImplicitObjectArgument(), c->getArg(0));
            if (family(ot) == "vector" && n == "swap")
                return "Jpp.swapVectors(" + expr(obj) + (me->isArrow() ? ".get()" : "") + ", " +
                       expr(c->getArg(0)) + ")";
            if (chrono(ot)) {
                auto object = me->isArrow() ? expr(obj) + ".get()" : expr(obj);
                if (n == "count" || n == "time_since_epoch")
                    return object + "." + n + "()";
                fail(c, "unsupported chrono member " + n);
            }
            if (family(ot) == "mersenne_twister_engine") {
                auto object = me->isArrow() ? expr(obj) + ".get()" : expr(obj);
                if (n == "operator()")
                    return converted(object + ".next()", ctx.UnsignedLongLongTy, c->getType());
                if (n == "operator=")
                    return object + ".jppAssignFrom(" + expr(c->getArg(0)) + ")";
                if (n == "discard")
                    return object + ".discard(" + value(c->getArg(0), ctx.UnsignedLongLongTy) + ")";
                if (n == "seed") {
                    if (c->getNumArgs() && family(c->getArg(0)->getType()) == "seed_seq")
                        return object + ".seed(" + expr(c->getArg(0)) + ")";
                    if (c->getNumArgs() && !clean(c->getArg(0)->getType())->isIntegerType())
                        fail(c, "Mersenne Twister seed sequences need additional lowering");
                    return object + ".seed(" +
                           (c->getNumArgs() ? value(c->getArg(0), ctx.UnsignedLongLongTy)
                                            : "5489L") +
                           ")";
                }
                fail(c, "unsupported Mersenne Twister member " + n);
            }
            if (family(ot) == "seed_seq") {
                auto object = me->isArrow() ? expr(obj) + ".get()" : expr(obj);
                if (n == "size")
                    return integerResult(c, object + ".size()");
                if (n == "generate" || n == "param") {
                    auto target = iteratorElement(c->getArg(0)->getType());
                    auto word = temporary("jppWord");
                    auto arguments = asIterator(c->getArg(0));
                    if (n == "generate")
                        arguments += ", " + asIterator(c->getArg(1));
                    return object + "." + n + "(" + arguments + ", " + word + " -> " +
                           converted(word, ctx.UnsignedIntTy, target) + ")";
                }
                fail(c, "unsupported seed-sequence member " + n);
            }
            if (bitReference(ot)) {
                auto object = me->isArrow() ? expr(obj) + ".get()" : expr(obj);
                if (n == "operator bool")
                    return object + ".get()";
                if (n == "operator~")
                    return "!" + object + ".get()";
                if (n == "flip")
                    return object + ".flip()";
                fail(c, "unsupported bitset reference operation " + n);
            }
            if (family(ot) == "bitset") {
                auto object = me->isArrow() ? expr(obj) + ".get()" : expr(obj);
                if (n == "count" || n == "size")
                    return integerResult(c, object + "." + n + "()");
                if (n == "_Find_first")
                    return integerResult(c, object + ".findFirst()");
                if (n == "_Find_next")
                    return integerResult(c, object + ".findNext(" +
                                                value(c->getArg(0), ctx.UnsignedLongLongTy) + ")");
                if (n == "any" || n == "all" || n == "none")
                    return object + "." + n + "()";
                if (n == "to_ulong" || n == "to_ullong")
                    return converted(object + ".toLong(" +
                                         std::to_string(ctx.getTypeSize(c->getType())) + ")",
                                     ctx.UnsignedLongLongTy, c->getType());
                if (n == "set" || n == "reset" || n == "flip" || n == "test") {
                    std::vector<std::string> args;
                    for (unsigned i = 0; i < c->getNumArgs(); ++i)
                        if (!isa<CXXDefaultArgExpr>(c->getArg(i)))
                            args.push_back(i == 0 ? value(c->getArg(i), ctx.UnsignedLongLongTy)
                                                  : truth(c->getArg(i)));
                    return object + "." + n + "(" + join(args) + ")";
                }
                if (n == "to_string") {
                    auto zero = c->getNumArgs() ? expr(c->getArg(0)) : "'0'";
                    auto one = c->getNumArgs() > 1 ? expr(c->getArg(1)) : "'1'";
                    return object + ".to_string(" + zero + ", " + one + ")";
                }
                fail(c, "unsupported bitset operation " + n);
            }
            if (n == "sync_with_stdio")
                return "Jpp.last(" + expr(obj) + ", JppIoState.sync(" + truth(c->getArg(0)) + "))";
            if (stream(ot)) {
                auto object = me->isArrow() ? expr(obj) + ".get()" : expr(obj);
                if (n == "tie")
                    return object + ".tie(" + (c->getNumArgs() ? expr(c->getArg(0)) : "") + ")";
                if (n == "operator bool")
                    return object + ".good";
                if (n == "good")
                    return "(" + object + ".good && !" + object + ".ended)";
                if (n == "fail" || n == "bad")
                    return "!" + object + ".good";
                if (n == "eof")
                    return object + ".ended";
                if (n == "flush")
                    return object + ".flush()";
                if (n == "close" || n == "is_open")
                    return object + "." + n + "()";
                if (n == "open") {
                    auto path = "Jpp.cString(" + expr(c->getArg(0)) + ")";
                    if (c->getNumArgs() > 1 && !isa<CXXDefaultArgExpr>(c->getArg(1)))
                        path += ", " + expr(c->getArg(1));
                    return object + ".open(" + path + ")";
                }
                if (n == "clear")
                    return object + ".clear()";
                if (n == "ignore") {
                    std::vector<std::string> a;
                    for (auto arg : c->arguments())
                        if (!isa<CXXDefaultArgExpr>(arg))
                            a.push_back(expr(arg));
                    return object + ".ignore(" + join(a) + ")";
                }
                fail(c, "unsupported stream operation " + n);
            }
            std::vector<std::string> args;
            for (unsigned i = 0; i < c->getNumArgs(); ++i) {
                auto a = c->getArg(i);
                if (isa<CXXDefaultArgExpr>(a))
                    continue;
                QualType target =
                    f && i < f->getNumParams() ? f->getParamDecl(i)->getType() : a->getType();
                args.push_back(f && user(f) && referenceABI(target) ? reference(a)
                                                                    : value(a, target, true));
            }
            auto o = me->isArrow() ? expr(obj) + ".get()" : expr(obj);
            if (isMap(ot) && n == "erase" && c->getNumArgs()) {
                if (c->getNumArgs() == 2)
                    return o + ".eraseRange(" + join(args) + ")";
                if (isIter(c->getArg(0)->getType()) &&
                    !ctx.hasSameType(clean(c->getArg(0)->getType()), clean(element(ot))))
                    return o + ".eraseIterator(" + join(args) + ")";
            }
            if (n == "size" || n == "length" || n == "capacity")
                return integerResult(c, o + "." + n + "()");
            bool deque = family(ot) == "queue" || family(ot) == "deque" || family(ot) == "stack";
            bool heap = family(ot) == "priority_queue";
            if ((deque || heap || isVec(ot)) && c->getNumArgs() == 1 &&
                (n == "push" || n == "push_back" || n == "push_front")) {
                bool move = f && f->getNumParams() &&
                            f->getParamDecl(0)->getType()->isRValueReferenceType();
                args[0] = storedElement(c->getArg(0), element(ot), move);
            }
            if (n == "emplace" || n == "emplace_back" || n == "emplace_front") {
                std::vector<const Expr*> values;
                for (auto arg : c->arguments())
                    if (!isa<CXXDefaultArgExpr>(arg))
                        values.push_back(arg);
                if (isMap(ot)) {
                    if (values.size() == 2)
                        return o + ".emplace(" + value(values[0], element(ot), true) + ", " +
                               value(values[1], element(ot, 1), true) + ")";
                } else {
                    auto created = elementConstruction(element(ot), values);
                    if (isSet(ot)) {
                        return o +
                               (family(c->getType()) == "pair" ? ".insert(" : ".insertMultiple(") +
                               created + ")";
                    }
                    if (heap)
                        return o + ".add(" + created + ")";
                    if (deque)
                        return o + (n == "emplace_front" ? ".addFirst(" : ".addLast(") + created +
                               ")";
                    if (isVec(ot) && n == "emplace_back")
                        return "Jpp.append(" + o + ", " + created + ")";
                }
            }
            if (n == "empty")
                return isVec(ot) || deque || heap ? o + ".isEmpty()" : o + ".empty()";
            if (deque) {
                if (n == "push" || n == "push_back")
                    return o + ".addLast(" + join(args) + ")";
                if (n == "push_front")
                    return o + ".addFirst(" + join(args) + ")";
                if (n == "front")
                    return o + ".getFirst()";
                if (n == "back" || n == "top")
                    return o + ".getLast()";
                if (n == "pop_front")
                    return o + ".removeFirst()";
                if (n == "pop_back" || (n == "pop" && family(ot) == "stack"))
                    return o + ".removeLast()";
                if (n == "pop")
                    return o + ".removeFirst()";
            }
            if (heap) {
                if (n == "push")
                    return o + ".add(" + join(args) + ")";
                if (n == "pop")
                    return o + ".remove()";
                if (n == "top")
                    return o + ".element()";
            }
            if (n == "push_back" && isVec(ot))
                return o + ".add(" + join(args) + ")";
            if ((isVec(ot) || family(ot) == "deque") &&
                (n == "begin" || n == "end" || n == "cbegin" || n == "cend" || n == "rbegin" ||
                 n == "rend"))
                return "Jpp." +
                       std::string(n == "cbegin" ? "begin"
                                   : n == "cend" ? "end"
                                                 : n) +
                       "(" + o + ")";
            if (isVec(ot) && n == "front")
                return o + ".get(0)";
            if (isVec(ot) && n == "back")
                return o + ".get(" + o + ".size() - 1)";
            if (isVec(ot) && n == "pop_back")
                return o + ".remove(" + o + ".size() - 1)";
            if ((isVec(ot) || family(ot) == "deque") &&
                (n == "erase" || n == "insert" || n == "assign")) {
                if (n == "assign" && !args.empty() && !isIter(c->getArg(0)->getType()))
                    args[0] = intArgument(c->getArg(0));
                return "Jpp." + n + "(" + o + ", " + join(args) + ")";
            }
            if (n == "emplace_back") {
                auto et = element(ot);
                if (et->isRecordType())
                    return o + ".add(new " + type(et) + "(" + join(args) + "))";
                return o + ".add(" + join(args) + ")";
            }
            if (n == "resize" && (isVec(ot) || family(ot) == "deque")) {
                if (c->getNumArgs() > 1 && !isa<CXXDefaultArgExpr>(c->getArg(1)))
                    return "Jpp.resize(" + o + ", " + intArgument(c->getArg(0)) + ", " +
                           value(c->getArg(1), element(ot), false) + ")";
                return "Jpp.resizeDefault(" + o + ", " + intArgument(c->getArg(0)) + ", () -> " +
                       defaultValue(element(ot)) + ")";
            }
            if (n == "reserve" && isVec(ot))
                return o + ".ensureCapacity(" + intArgument(c->getArg(0)) + ")";
            if (n == "reserve" && isString(ot))
                return o + ".reserve(" +
                       (c->getNumArgs() ? value(c->getArg(0), ctx.UnsignedLongLongTy) : "0L") + ")";
            if (n == "resize" && isString(ot))
                return o + ".resize(" + value(c->getArg(0), ctx.UnsignedLongLongTy) + ", " +
                       (c->getNumArgs() > 1 ? expr(c->getArg(1)) : "'\\0'") + ")";
            if (n == "reserve" && family(ot).starts_with("unordered_"))
                return "Jpp.ignore(" + o + ", " + expr(c->getArg(0)) + ")";
            if (n == "fill" && family(ot) == "array")
                return "Jpp.fill(" + o + ", " + args[0] + ")";
            if (n == "at" && (isVec(ot) || isString(ot) || family(ot) == "deque"))
                return o + ".get(" + intArgument(c->getArg(0)) + ")";
            if (isString(ot) && (n == "append" || n == "assign")) {
                for (unsigned i = 0; i < args.size(); ++i) {
                    auto argument = c->getArg(i);
                    if (clean(argument->getType())->isIntegerType() &&
                        !(i == 1 && clean(c->getArg(0)->getType())->isIntegerType()))
                        args[i] = value(argument, ctx.UnsignedLongLongTy);
                    else if (args.size() > 1 && dataPointer(argument->getType()))
                        args[i] = value(argument, argument->getType());
                }
            }
            if (n == "substr" || n == "find") {
                if (isString(ot)) {
                    for (size_t i = (n == "substr" ? 0 : 1); i < args.size(); ++i)
                        args[i] = value(c->getArg(i), ctx.UnsignedLongLongTy);
                    if (n == "find" && args.size() == 3)
                        args[0] = value(c->getArg(0), c->getArg(0)->getType());
                }
            }
            if ((n == "c_str" || n == "data") && isString(ot))
                return o + ".c_str()";
            if (n == "capacity" && isString(ot))
                return o + ".capacity()";
            if (n == "data" && isVec(ot))
                return "Jpp.pointer(Jpp.begin(" + o + "))";
            if (n == "pop" && family(ot) == "stack")
                return o + ".pop_back()";
            static const std::set<std::string> methods = {
                "size",        "length", "empty",      "clear",     "front",      "back",
                "top",         "push",   "pop",        "push_back", "push_front", "pop_back",
                "pop_front",   "begin",  "end",        "cbegin",    "cend",       "rbegin",
                "rend",        "erase",  "insert",     "count",     "contains",   "lower_bound",
                "upper_bound", "assign", "substr",     "find",      "append",     "at",
                "swap",        "merge",  "equal_range"};
            if (!methods.count(n))
                fail(c, "unsupported library member '" + n + "'");
            if (n == "cbegin")
                n = "begin";
            if (n == "cend")
                n = "end";
            auto result = o + "." + n + "(" + join(args) + ")";
            return n == "count" ? integerResult(c, result) : result;
        }
        if (!f) {
            auto prototype = callableType(c->getCallee()->getType());
            std::vector<std::string> args;
            for (unsigned i = 0; i < c->getNumArgs(); ++i) {
                auto arg = c->getArg(i);
                auto target =
                    i < prototype->getNumParams() ? prototype->getParamType(i) : arg->getType();
                args.push_back(referenceABI(target) ? reference(arg) : value(arg, target, true));
            }
            return expr(c->getCallee()) + ".call(" + join(args) + ")";
        }
        auto n = f->getNameAsString();
        if ((!user(f) || f->getBuiltinID()) &&
            (n == "is_constant_evaluated" || n == "__builtin_is_constant_evaluated"))
            return "false";
        if (user(f) && !f->getBuiltinID()) {
            std::string prefix;
            if (auto method = dyn_cast<CXXMethodDecl>(f); method && method->isStatic()) {
                prefix = name(method->getParent()) + ".";
                if (auto member = dyn_cast<MemberExpr>(bare(c->getCallee()));
                    member && member->getBase()->HasSideEffects(ctx))
                    return supplier([&] {
                        expressionStatement(member->getBase());
                        auto invocation = prefix + name(f) + "(" + arguments(c, f) + ")";
                        if (c->getType()->isVoidType()) {
                            line(invocation + ";");
                            line("return null;");
                        } else
                            line("return " + invocation + ";");
                    });
            } else if (!isa<CXXMethodDecl>(f))
                prefix = options.className + ".";
            return prefix + name(f) + "(" + arguments(c, f) + ")";
        }
        auto memoryOperation = n.starts_with("__builtin_") ? n.substr(10) : n;
        if (memoryOperation == "strlen")
            return converted("Jpp.strlen(" + expr(c->getArg(0)) + ")", ctx.UnsignedLongLongTy,
                             c->getType());
        if (isa<UserDefinedLiteral>(c) && family(c->getType()) == "duration" &&
            c->getNumArgs() == 1)
            return chronoValue(c->getType(), value(c->getArg(0), ctx.LongLongTy));
        if (isa<UserDefinedLiteral>(c) && family(c->getType()) == "duration") {
            Expr::EvalResult evaluated;
            if (c->EvaluateAsRValue(evaluated, ctx) && evaluated.Val.isStruct() &&
                evaluated.Val.getStructNumFields() == 1 && evaluated.Val.getStructField(0).isInt())
                return chronoValue(
                    c->getType(),
                    integerLiteral(evaluated.Val.getStructField(0).getInt(), ctx.LongLongTy));
            fail(c, "this duration literal needs additional constant-value lowering");
        }
        if (c->getNumArgs() && chrono(c->getArg(0)->getType())) {
            auto source = expr(c->getArg(0));
            if (n == "sleep_for" &&
                f->getQualifiedNameAsString().find("this_thread") != std::string::npos)
                return "JppTimePoint.sleep(" + source + ")";
            if (n == "abs" && family(c->getArg(0)->getType()) == "duration")
                return source + ".absolute()";
            if (n == "duration_cast" || n == "time_point_cast" || n == "floor" || n == "ceil" ||
                n == "round") {
                if (family(c->getArg(0)->getType()) == "time_point")
                    source += ".time_since_epoch()";
                return chronoValue(c->getType(), source, true,
                                   n == "floor" || n == "ceil" || n == "round" ? n : "cast");
            }
        }
        if (memoryOperation == "memcpy" || memoryOperation == "memmove" ||
            memoryOperation == "memcmp")
            return "Jpp." + memoryOperation + "(" + expr(c->getArg(0)) + ", " + expr(c->getArg(1)) +
                   ", " + value(c->getArg(2), ctx.UnsignedLongLongTy) + ")";
        if (memoryOperation == "memset" || memoryOperation == "memchr")
            return "Jpp." + memoryOperation + "(" + expr(c->getArg(0)) + ", " +
                   intArgument(c->getArg(1)) + ", " + value(c->getArg(2), ctx.UnsignedLongLongTy) +
                   ")";
        if (memoryOperation == "malloc")
            return "Jpp.malloc(" + value(c->getArg(0), ctx.UnsignedLongLongTy) + ", " +
                   targetLittleEndian() + ")";
        if (memoryOperation == "calloc")
            return "Jpp.calloc(" + value(c->getArg(0), ctx.UnsignedLongLongTy) + ", " +
                   value(c->getArg(1), ctx.UnsignedLongLongTy) + ", " + targetLittleEndian() + ")";
        if (memoryOperation == "realloc")
            return "Jpp.realloc(" + expr(c->getArg(0)) + ", " +
                   value(c->getArg(1), ctx.UnsignedLongLongTy) + ", " + targetLittleEndian() + ")";
        if (memoryOperation == "free")
            return "Jpp.free(" + expr(c->getArg(0)) + ")";
        if (n == "sync_with_stdio") {
            auto invocation = "JppIoState.sync(" + truth(c->getArg(0)) + ")";
            if (auto member = dyn_cast<MemberExpr>(bare(c->getCallee()));
                member && member->getBase()->HasSideEffects(ctx))
                return "Jpp.last(" + expr(member->getBase()) + ", " + invocation + ")";
            return invocation;
        }
        if (n == "__builtin_expect" || n == "__builtin_expect_with_probability")
            return expr(c->getArg(0));
        if (n == "__builtin_unreachable" || n == "__builtin_trap")
            return "Jpp.trap()";
        if (n.starts_with("__builtin_")) {
            auto op = n.substr(10);
            std::string cls = op.ends_with("ll") || op.ends_with("l") ? "Long" : "Integer";
            if (op.starts_with("popcount"))
                return cls + ".bitCount(" + expr(c->getArg(0)) + ")";
            if (op.starts_with("clz"))
                return cls + ".numberOfLeadingZeros(" + expr(c->getArg(0)) + ")";
            if (op.starts_with("ctz"))
                return cls + ".numberOfTrailingZeros(" + expr(c->getArg(0)) + ")";
            if (op.starts_with("parity"))
                return "(" + cls + ".bitCount(" + expr(c->getArg(0)) + ") & 1)";
            if (op == "bswap16")
                return "Short.toUnsignedInt(Short.reverseBytes((short)(" + expr(c->getArg(0)) +
                       ")))";
            if (op.starts_with("bswap"))
                return std::string(op == "bswap64" ? "Long" : "Integer") + ".reverseBytes(" +
                       expr(c->getArg(0)) + ")";
            if (op == "constant_p") {
                Expr::EvalResult r;
                return c->getArg(0)->EvaluateAsRValue(r, ctx) ? "1" : "0";
            }
            fail(c, "unsupported compiler intrinsic '" + n + "'");
        }
        if (n == "printf") {
            for (auto argument : c->arguments())
                if (longDouble(argument->getType()))
                    fail(c, "long double formatted output needs additional lowering");
            std::vector<std::string> a = {"Jpp.cString(" + expr(c->getArg(0)) + ")"};
            for (unsigned i = 1; i < c->getNumArgs(); ++i) {
                auto arg = c->getArg(i);
                auto address = dyn_cast<UnaryOperator>(bare(arg));
                a.push_back(address && address->getOpcode() == UO_AddrOf
                                ? reference(address->getSubExpr())
                                : expr(arg));
            }
            return "jppOut.printf(" + join(a) + ")";
        }
        if (n == "scanf") {
            std::vector<std::string> a = {"Jpp.cString(" + expr(c->getArg(0)) + ")"};
            for (unsigned i = 1; i < c->getNumArgs(); ++i) {
                auto arg = bare(c->getArg(i));
                const Expr* destination = nullptr;
                if (auto address = dyn_cast<UnaryOperator>(arg);
                    address && address->getOpcode() == UO_AddrOf)
                    destination = address->getSubExpr();
                auto array = ctx.getAsArrayType(arg->getType());
                auto target = destination                   ? destination->getType()
                              : array                       ? array->getElementType()
                              : dataPointer(arg->getType()) ? arg->getType()->getPointeeType()
                                                            : QualType();
                if (target.isNull())
                    fail(arg, "scanf destination needs an address or an array");
                if (array && target->isCharType()) {
                    a.push_back("Jpp.scanChars(" + expr(arg) + ")");
                    continue;
                }
                if (!destination && target->isCharType()) {
                    a.push_back("Jpp.scanChars(" + expr(arg) + ")");
                    continue;
                }
                auto ref = destination ? reference(destination)
                           : array     ? "JppRef.at(" + expr(arg) + ", 0)"
                                       : "JppRef.at(" + expr(arg) + ")";
                auto jt = type(target);
                if (jt == "char")
                    a.push_back("Jpp.scanChar(" + ref + ")");
                else if (jt == "int")
                    a.push_back("Jpp.scanInt(" + ref + ", " +
                                std::to_string(ctx.getTypeSize(target)) + ", " +
                                (target->isUnsignedIntegerType() ? "true" : "false") + ")");
                else if (jt == "long")
                    a.push_back("Jpp.scanLong(" + ref + ")");
                else if (jt == "double" || jt == "float")
                    a.push_back("Jpp.scan" + std::string(jt == "double" ? "Double" : "Float") +
                                "(" + ref + ")");
                else
                    fail(arg, "unsupported scanf destination type");
            }
            return "new JppScanner(jppIn).scan(" + join(a) + ")";
        }
        if ((n == "fread" || n == "fwrite") && c->getNumArgs() == 4) {
            auto stream = dyn_cast<DeclRefExpr>(bare(c->getArg(3)));
            auto target =
                stream && !user(stream->getDecl()) ? stream->getDecl()->getNameAsString() : "";
            auto javaStream = n == "fread" && target == "stdin"     ? "jppIn"
                              : n == "fwrite" && target == "stdout" ? "jppOut"
                              : n == "fwrite" && target == "stderr" ? "jppErr"
                                                                    : "";
            if (!*javaStream)
                fail(c, n + " needs additional C file-handle lowering for this stream");
            return integerResult(c, "Jpp." + n + "(" + expr(c->getArg(0)) + ", " +
                                        value(c->getArg(1), ctx.UnsignedLongLongTy) + ", " +
                                        value(c->getArg(2), ctx.UnsignedLongLongTy) + ", " +
                                        javaStream + ")");
        }
        if (n == "freopen") {
            auto target = dyn_cast<DeclRefExpr>(bare(c->getArg(2)));
            auto streamName = target ? target->getDecl()->getNameAsString() : "";
            if (streamName == "stdin" || streamName == "stdout" || streamName == "stderr")
                return "Jpp.reopen" + std::string(streamName == "stdin" ? "Input" : "Output") +
                       "(" +
                       (streamName == "stdin"    ? "jppIn"
                        : streamName == "stdout" ? "jppOut"
                                                 : "jppErr") +
                       ", Jpp.cString(" + expr(c->getArg(0)) + "), Jpp.cString(" +
                       expr(c->getArg(1)) + "))";
            fail(c, "freopen needs a supported C standard stream");
        }
        if (n == "puts" || n == "putchar" || n == "getchar")
            return "Jpp." + n + "(" + arguments(c) + ")";
        if (n == "exit")
            return "Jpp.exit(" + intArgument(c->getArg(0)) + ")";
        if (n == "_Exit")
            return "Runtime.getRuntime().halt(" + intArgument(c->getArg(0)) + ")";
        if (n == "unreachable")
            return "Jpp.trap()";
        if (n == "bit_cast")
            return bitCast(c->getArg(0), c->getType());
        if (n == "tuple_cat")
            return supplier([&] {
                auto targets = productTypes(c->getType());
                std::vector<std::string> values;
                for (auto argument : c->arguments()) {
                    if (!tupleLike(argument->getType()))
                        fail(argument, "this tuple-like source needs additional lowering");
                    auto object = temporary("jppTuple");
                    line("final " + type(argument->getType()) + " " + object + " = " +
                         expr(argument) + ";");
                    auto elements = productTypes(argument->getType());
                    for (unsigned i = 0; i < elements.size(); ++i) {
                        auto element = productElement(object, argument->getType(), i,
                                                      argument->getValueKind(), c->getExprLoc());
                        values.push_back(productArgument(element, targets.at(values.size())));
                    }
                }
                line("return " + productConstruct(c->getType(), values) + ";");
            });
        if (n == "apply" || n == "make_from_tuple")
            return supplier([&] {
                auto argument = c->getArg(n == "apply" ? 1 : 0);
                if (!tupleLike(argument->getType()))
                    fail(argument, "this tuple-like argument needs additional lowering");
                auto tuple = temporary("jppTuple");
                auto function = temporary("jppFunction");
                if (n == "apply") {
                    if (auto name = dyn_cast<DeclRefExpr>(bare(c->getArg(0)));
                        name && isa<FunctionDecl>(name->getDecl()))
                        line("final " + type(c->getArg(0)->getType()) + " " + function + " = " +
                             functionPointer(cast<FunctionDecl>(name->getDecl()),
                                             ctx.getPointerType(c->getArg(0)->getType())) +
                             ";");
                    else
                        line("final " + type(c->getArg(0)->getType()) + " " + function + " = " +
                             expr(c->getArg(0)) + ";");
                }
                line("final " + type(argument->getType()) + " " + tuple + " = " + expr(argument) +
                     ";");
                std::vector<Expr*> arguments;
                for (unsigned i = 0; i < productTypes(argument->getType()).size(); ++i)
                    arguments.push_back(productElement(tuple, argument->getType(), i,
                                                       argument->getValueKind(), c->getExprLoc()));
                Expr* invocation;
                if (n == "apply") {
                    auto callable = c->getArg(0);
                    auto opaque =
                        mappedValue(callable->getType(), callable->getValueKind(), function,
                                    "JppRef.object(" + function + ")", c->getExprLoc());
                    Sema::ContextRAII context(sema, ctx.getTranslationUnitDecl());
                    auto resolved = sema.BuildCallExpr(nullptr, opaque, c->getExprLoc(), arguments,
                                                       c->getExprLoc());
                    if (resolved.isInvalid())
                        fail(c, "cannot resolve this tuple invocation");
                    invocation = resolved.get();
                } else {
                    auto target = f->getTemplateSpecializationArgs()->get(0).getAsType();
                    invocation = resolveConstruction(target, arguments, c->getExprLoc());
                }
                if (c->getType()->isVoidType()) {
                    expressionStatement(invocation);
                    line("return null;");
                } else
                    line("return " +
                         (referenceABI(f->getReturnType()) ? reference(invocation)
                                                           : expr(invocation)) +
                         ";");
            });
        if (n == "__assert_fail")
            return "Jpp.assertFail()";
        if (n == "_setjmp" || n == "setjmp")
            fail(c, "this setjmp context needs additional continuation lowering");
        if (n == "longjmp" || n == "_longjmp")
            return "Jpp.longJump(" + expr(c->getArg(0)) + ", " + value(c->getArg(1), ctx.IntTy) +
                   ")";
        if (n == "make_pair" || n == "make_tuple" || n == "tie" || n == "forward_as_tuple") {
            auto elements = productTypes(c->getType());
            std::vector<std::string> values;
            for (unsigned i = 0; i < c->getNumArgs(); ++i)
                values.push_back(productArgument(c->getArg(i), elements[i]));
            return productConstruct(c->getType(), values);
        }
        if (n == "ref" || n == "cref")
            return family(c->getArg(0)->getType()) == "reference_wrapper" ? expr(c->getArg(0))
                                                                          : reference(c->getArg(0));
        if ((n == "is_eq" || n == "is_neq" || n == "is_lt" || n == "is_lteq" || n == "is_gt" ||
             n == "is_gteq") &&
            c->getNumArgs() == 1 && ordering(c->getArg(0)->getType()))
            return expr(c->getArg(0)) + "." + n + "()";
        if (productGet(c)) {
            auto index = productIndex(c);
            auto t = c->getArg(0)->getType();
            return referenceABI(f->getReturnType()) ? productReference(expr(c->getArg(0)), t, index)
                                                    : productRead(expr(c->getArg(0)), t, index);
        }
        if (n == "swap" && product(c->getArg(0)->getType()))
            return swapProduct(c->getArg(0), c->getArg(1));
        if (n == "swap" && family(c->getArg(0)->getType()) == "array")
            return swapProduct(c->getArg(0), c->getArg(1));
        if (n == "swap" && family(c->getArg(0)->getType()) == "vector")
            return "Jpp.swapVectors(" + expr(c->getArg(0)) + ", " + expr(c->getArg(1)) + ")";
        if (n == "swap" && ctx.getAsArrayType(c->getArg(0)->getType()))
            return swapArrays(c->getArg(0), c->getArg(1));
        if (n == "swap" && isMap(c->getArg(0)->getType()))
            return expr(c->getArg(0)) + ".swap(" + expr(c->getArg(1)) + ")";
        if (n == "swap" && orderTree(c->getArg(0)->getType()))
            return "Jpp.swapOrderSets(" + expr(c->getArg(0)) + ", " + expr(c->getArg(1)) + ")";
        if (n == "swap" && family(c->getArg(0)->getType()) == "deque")
            return expr(c->getArg(0)) + ".swap(" + expr(c->getArg(1)) + ")";
        if (n == "swap" && family(c->getArg(0)->getType()) == "bitset")
            return "JppBitset.swap(" + expr(c->getArg(0)) + ", " + expr(c->getArg(1)) + ")";
        if (n == "swap")
            if (auto r = record(c->getArg(0)->getType()); r && user(r))
                return swapObjects(c->getArg(0), c->getArg(1));
        if (n == "swap")
            return "Jpp.swap(" + reference(c->getArg(0)) + ", " + reference(c->getArg(1)) + ")";
        if (n == "flush")
            return expr(c->getArg(0)) + ".flush()";
        if (n == "isdigit")
            return "Jpp.isdigit(" + expr(c->getArg(0)) + ")";
        if (n == "move" || n == "forward" || n == "as_const")
            return expr(c->getArg(0));
        if (n == "to_string" && c->getNumArgs() && longDouble(c->getArg(0)->getType()))
            fail(c, "long double text formatting needs additional lowering");
        if (n == "to_string")
            return "new JppString(" + expr(c->getArg(0)) + ")";
        if (n == "to_underlying")
            return value(c->getArg(0), c->getType());
        if (n == "stoi" || n == "stol" || n == "stoll" || n == "stod" || n == "stof")
            return std::string(n == "stoi"   ? "Integer.parseInt"
                               : n == "stod" ? "Double.parseDouble"
                               : n == "stof" ? "Float.parseFloat"
                                             : "Long.parseLong") +
                   "(" + expr(c->getArg(0)) + ".toString())";
        if (n == "getline" && c->getNumArgs() >= 2 && isString(c->getArg(1)->getType())) {
            auto destination = c->getArg(1);
            auto delimiter = c->getNumArgs() > 2 ? expr(c->getArg(2)) : "'\\n'";
            auto read = [&](const std::string& source) {
                if (addressHasSideEffects(destination)) {
                    auto input = temporary("jppStream"), old = temporary("jppPrevious");
                    return "Jpp.extract(" + source + ", " + reference(destination) + ", (" + input +
                           ", " + old + ") -> " + input + ".line(" + old + ", " + delimiter + "))";
                }
                return assign(destination,
                              source + ".line(" + expr(destination) + ", " + delimiter + ")");
            };
            if (c->getArg(0)->HasSideEffects(ctx))
                return supplier([&] {
                    auto source = temporary("jppStream");
                    line("JppInput " + source + " = " + expr(c->getArg(0)) + ";");
                    line(read(source) + ";");
                    line("return " + source + ";");
                });
            auto source = expr(c->getArg(0));
            return "Jpp.input(" + source + ", " + read(source) + ")";
        }
        if (n == "begin" || n == "end" || n == "cbegin" || n == "cend") {
            auto a = c->getArg(0);
            auto method = (n == "begin" || n == "cbegin") ? "begin" : "end";
            if (primitiveArray(a->getType())) {
                auto item = ctx.getAsArrayType(a->getType())->getElementType();
                return "Jpp.<" + boxed(item) + ">" +
                       (std::string(method) == "begin" ? "arrayDecay(" : "arrayEnd(") +
                       arrayLocation(a) + ")";
            }
            return ctx.getAsArrayType(bare(a)->getType()) || isVec(a->getType())
                       ? "Jpp." + std::string(method) + "(" + expr(a) + ")"
                       : expr(a) + "." + method + "()";
        }
        if ((n == "size" || n == "ssize" || n == "data") && c->getNumArgs() == 1 &&
            primitiveArray(c->getArg(0)->getType())) {
            auto array = c->getArg(0);
            return n == "data" ? arrayDecay(array)
                               : integerResult(c, "Jpp.arrayLength(" + arrayLocation(array) + ")");
        }
        if (n == "distance")
            return integerResult(c, asIterator(c->getArg(1)) + ".minus(" +
                                        asIterator(c->getArg(0)) + ")");
        if (n == "next" || n == "prev")
            return expr(c->getArg(0)) + ".plus(" + std::string(n == "prev" ? "-(" : "(") +
                   (c->getNumArgs() > 1 && !isa<CXXDefaultArgExpr>(c->getArg(1))
                        ? intArgument(c->getArg(1))
                        : "1") +
                   "))";
        if (n == "advance")
            return expr(c->getArg(0)) + ".advance(" + intArgument(c->getArg(1)) + ")";
        if (c->getNumArgs() && longDouble(c->getArg(0)->getType()) && n != "min" && n != "max" &&
            n != "clamp") {
            auto argument = expr(c->getArg(0));
            auto operation = n;
            if (operation.starts_with("__builtin_"))
                operation.erase(0, 10);
            static const std::set<std::string> extendedMath = {
                "fabsl",      "sqrtl",  "floorl",  "ceill",     "truncl", "roundl",  "rintl",
                "nearbyintl", "ldexpl", "scalbnl", "copysignl", "frexpl", "lroundl", "llroundl"};
            if (extendedMath.count(operation))
                operation.pop_back();
            if (operation == "isnan" || operation == "isinf" || operation == "isfinite" ||
                operation == "isnormal") {
                auto result = operation == "isnormal" ? "(" + argument + ").isNormal()"
                                                      : "(" + argument + ").special " +
                                                            (operation == "isfinite" ? "== 0"
                                                             : operation == "isnan"  ? "== 2"
                                                                                     : "== 1");
                return converted("(" + result + ")", ctx.BoolTy, c->getType());
            }
            if (operation == "signbit")
                return converted("(" + argument + ").negative", ctx.BoolTy, c->getType());
            if (operation == "fabs" || operation == "abs")
                return "(" + argument + ").abs()";
            if (operation == "sqrt")
                return "(" + argument + ").sqrt()";
            if (operation == "floor" || operation == "ceil" || operation == "trunc" ||
                operation == "round" || operation == "rint" || operation == "nearbyint" ||
                operation == "lround" || operation == "llround") {
                auto mode = operation == "floor"                              ? "1"
                            : operation == "ceil"                             ? "2"
                            : operation == "trunc"                            ? "0"
                            : operation == "rint" || operation == "nearbyint" ? "4"
                                                                              : "3";
                return converted("(" + argument + ").integral(" + mode + ")", ctx.LongDoubleTy,
                                 c->getType());
            }
            if (operation == "ldexp" || operation == "scalbn")
                return "(" + argument + ").scaleB(" + value(c->getArg(1), ctx.IntTy) + ")";
            if (operation == "frexp")
                return "Jpp.longDoubleFrexp(" + argument + ", " + expr(c->getArg(1)) + ")";
            if (operation == "copysign")
                return "(" + argument + ").copySign(" + value(c->getArg(1), ctx.LongDoubleTy) + ")";
            fail(c, "this long double library operation needs additional lowering: " + n);
        }
        if (n == "log2" || n == "round" || n == "roundf" || n == "lround" || n == "llround" ||
            n == "rint" || n == "nearbyint" || n == "trunc") {
            std::string result;
            auto runtimeType =
                clean(c->getArg(0)->getType())->isSpecificBuiltinType(BuiltinType::Float) &&
                        n != "log2"
                    ? ctx.FloatTy
                    : ctx.DoubleTy;
            auto argument = value(c->getArg(0), runtimeType);
            if (n == "log2")
                result = "Jpp.log2(" + argument + ")";
            else if (n == "rint" || n == "nearbyint")
                result = "Jpp.rint(" + argument + ")";
            else if (n == "trunc")
                result = "Jpp.trunc(" + argument + ")";
            else
                result = "Jpp.round(" + argument + ")";
            return converted(result, runtimeType, c->getType());
        }
        if (n == "isnan" || n == "isinf" || n == "isfinite") {
            auto cls = clean(c->getArg(0)->getType())->isSpecificBuiltinType(BuiltinType::Float)
                           ? "Float"
                           : "Double";
            auto argument = big(c->getArg(0)->getType()) ? value(c->getArg(0), ctx.DoubleTy)
                                                         : expr(c->getArg(0));
            auto result = std::string(cls) + "." +
                          (n == "isnan"   ? "isNaN"
                           : n == "isinf" ? "isInfinite"
                                          : "isFinite") +
                          "(" + argument + ")";
            return converted(result, ctx.BoolTy, c->getType());
        }
        if (n == "signbit") {
            auto arg = c->getArg(0);
            if (big(arg->getType()) && !clean(arg->getType())->isUnsignedIntegerType())
                return "(" + expr(arg) + ".signum() < 0)";
            if (clean(arg->getType())->isIntegerType())
                return clean(arg->getType())->isUnsignedIntegerType()
                           ? "Jpp.last(" + expr(arg) + ", false)"
                           : "(" + expr(arg) + " < 0)";
            auto cls = clean(arg->getType())->isSpecificBuiltinType(BuiltinType::Float) ? "Float"
                                                                                        : "Double";
            return "(" + std::string(cls) +
                   (std::string(cls) == "Float" ? ".floatToRawIntBits(" : ".doubleToRawLongBits(") +
                   expr(arg) + ") < 0)";
        }
        if (n == "ldexp" || n == "scalbn" || n == "nextafter" || n == "copysign" ||
            n == "remainder") {
            auto operation = n == "nextafter"   ? "nextAfter"
                             : n == "copysign"  ? "copySign"
                             : n == "remainder" ? "IEEEremainder"
                                                : "scalb";
            auto result = "Math." + std::string(operation) + "(" + arguments(c) + ")";
            return clean(c->getType())->isSpecificBuiltinType(BuiltinType::Float)
                       ? "(float)(" + result + ")"
                       : result;
        }
        if (n == "labs" || n == "llabs")
            n = "abs";
        static const std::set<std::string> maths = {
            "abs", "fabs", "sqrt",  "sqrtl", "pow", "powl", "floor", "ceil",  "round",
            "log", "log2", "log10", "exp",   "sin", "cos",  "tan",   "atan2", "hypot"};
        if (maths.count(n)) {
            if (n == "fabs")
                n = "abs";
            if (n == "sqrtl")
                n = "sqrt";
            if (n == "powl")
                n = "pow";
            std::vector<std::string> arguments;
            bool integralAbs = n == "abs" && clean(c->getType())->isIntegerType();
            for (auto arg : c->arguments())
                arguments.push_back(value(
                    arg,
                    integralAbs ? arg->getType()
                    : n == "abs" && clean(c->getType())->isSpecificBuiltinType(BuiltinType::Float)
                        ? ctx.FloatTy
                        : ctx.DoubleTy));
            if (integralAbs && big(c->getArg(0)->getType()))
                return arguments[0] + ".abs()";
            auto result = "Math." + n + "(" + join(arguments) + ")";
            return clean(c->getType())->isSpecificBuiltinType(BuiltinType::Float)
                       ? "(float)(" + result + ")"
                       : result;
        }
        if (n == "__gcd")
            n = "gcd";
        if (n == "popcount")
            return std::string(ctx.getTypeSize(c->getArg(0)->getType()) > 32 ? "Long" : "Integer") +
                   ".bitCount(" + expr(c->getArg(0)) + ")";
        if (n == "countl_zero" || n == "countr_zero")
            return std::string(ctx.getTypeSize(c->getArg(0)->getType()) > 32 ? "Long" : "Integer") +
                   "." + (n == "countl_zero" ? "numberOfLeadingZeros" : "numberOfTrailingZeros") +
                   "(" + expr(c->getArg(0)) + ")";
        if (n == "bit_ceil")
            return "Jpp.bitCeil(" + expr(c->getArg(0)) + ")";
        if (n == "bit_floor")
            return std::string(ctx.getTypeSize(c->getArg(0)->getType()) > 32 ? "Long" : "Integer") +
                   ".highestOneBit(" + expr(c->getArg(0)) + ")";
        if (n == "bit_width") {
            bool wide = ctx.getTypeSize(c->getArg(0)->getType()) > 32;
            return "(" + std::string(wide ? "64 - Long" : "32 - Integer") +
                   ".numberOfLeadingZeros(" + expr(c->getArg(0)) + "))";
        }
        if (n == "has_single_bit")
            return "(" +
                   std::string(ctx.getTypeSize(c->getArg(0)->getType()) > 32 ? "Long" : "Integer") +
                   ".bitCount(" + expr(c->getArg(0)) + ") == 1)";
        if (n == "back_inserter")
            return "Jpp.backInserter(" + expr(c->getArg(0)) + ")";
        if (n == "copy" || n == "copy_backward" || n == "copy_n") {
            auto source = iteratorElement(c->getArg(0)->getType());
            auto target = iteratorElement(c->getArg(2)->getType());
            auto item = temporary("jppValue");
            return "Jpp." + n + "(" + asIterator(c->getArg(0)) + ", " +
                   (n == "copy_n" ? intArgument(c->getArg(1)) : asIterator(c->getArg(1))) + ", " +
                   asIterator(c->getArg(2)) + ", " + item + " -> " +
                   converted(item, source, target) + ")";
        }
        if (n == "merge") {
            auto a = iteratorElement(c->getArg(0)->getType());
            auto b = iteratorElement(c->getArg(2)->getType());
            auto target = iteratorElement(c->getArg(4)->getType());
            auto first = temporary("jppFirst"), second = temporary("jppSecond");
            return "Jpp.merge(" + asIterator(c->getArg(0)) + ", " + asIterator(c->getArg(1)) +
                   ", " + asIterator(c->getArg(2)) + ", " + asIterator(c->getArg(3)) + ", " +
                   asIterator(c->getArg(4)) + ", " + first + " -> " + converted(first, a, target) +
                   ", " + second + " -> " + converted(second, b, target) +
                   (c->getNumArgs() > 5 ? ", " + expr(c->getArg(5)) : "") + ")";
        }
        if (n == "for_each") {
            auto callback = c->getArg(2);
            auto prototype = callableType(callback->getType());
            auto target = prototype->getParamType(0);
            auto fn = temporary("jppFunction"), position = temporary("jppPosition");
            auto argument = referenceABI(target)
                                ? "JppRef.at(" + position + ")"
                                : converted(position + ".get()",
                                            iteratorElement(c->getArg(0)->getType()), target);
            if (copyType(target))
                argument = "jppCopy(" + argument + ")";
            return "Jpp.for_each(" + asIterator(c->getArg(0)) + ", " + asIterator(c->getArg(1)) +
                   ", " + expr(callback) + ", (" + fn + ", " + position + ") -> " + fn + "." +
                   callableMethod(callback->getType()) + "(" + argument + "))";
        }
        if (n == "lower_bound" || n == "upper_bound" || n == "binary_search") {
            auto item = iteratorElement(c->getArg(0)->getType());
            auto key = c->getArg(2)->getType();
            auto callback = c->getNumArgs() > 3 ? c->getArg(3) : nullptr;
            auto predicate = [&](bool reverse, const std::string& initializer = {}) {
                auto left = reverse ? key : item, right = reverse ? item : key;
                return callback
                           ? comparatorPredicate(callback->getType(), left, right,
                                                 initializer.empty() ? expr(callback) : initializer)
                           : comparisonPredicate(left, right);
            };
            auto arguments = asIterator(c->getArg(0)) + ", " + asIterator(c->getArg(1)) + ", " +
                             expr(c->getArg(2));
            if (n != "binary_search")
                return "Jpp." + n + "(" + arguments + ", " + predicate(n == "upper_bound") + ")";
            if (!callback)
                return "Jpp.binary_search(" + arguments + ", " + predicate(false) + ", " +
                       predicate(true) + ")";
            return supplier([&] {
                auto comparator = temporary("jppCompare");
                line("final " + type(callback->getType()) + " " + comparator + " = " +
                     expr(callback) + ";");
                line("return Jpp.binary_search(" + arguments + ", " + predicate(false, comparator) +
                     ", " + predicate(true, comparator) + ");");
            });
        }
        static const std::set<std::string> algorithms = {
            "sort",        "stable_sort",   "reverse",    "fill", "lower_bound",
            "upper_bound", "binary_search", "unique",     "find", "count",
            "min_element", "max_element",   "accumulate", "iota", "next_permutation"};
        if (algorithms.count(n)) {
            if ((n == "sort" || n == "stable_sort" || n == "reverse") && c->getNumArgs() >= 2) {
                auto a = dyn_cast<CXXMemberCallExpr>(bare(c->getArg(0))),
                     b = dyn_cast<CXXMemberCallExpr>(bare(c->getArg(1)));
                if (a && b && a->getMethodDecl()->getNameAsString() == "begin" &&
                    b->getMethodDecl()->getNameAsString() == "end" &&
                    isVec(a->getImplicitObjectArgument()->getType()) &&
                    !a->getImplicitObjectArgument()->HasSideEffects(ctx) &&
                    expr(a->getImplicitObjectArgument()) == expr(b->getImplicitObjectArgument())) {
                    auto v = expr(a->getImplicitObjectArgument());
                    if (n == "reverse")
                        return "Jpp.reverseList(" + v + ")";
                    auto item = iteratorElement(c->getArg(0)->getType());
                    auto predicate = c->getNumArgs() > 2
                                         ? comparatorPredicate(c->getArg(2)->getType(), item, item,
                                                               expr(c->getArg(2)))
                                         : comparisonPredicate(item, item);
                    return "Jpp.sortList(" + v + ", Jpp.comparator(" + predicate + "))";
                }
            }
            std::vector<std::string> args;
            for (unsigned i = 0; i < c->getNumArgs(); ++i)
                if (!isa<CXXDefaultArgExpr>(c->getArg(i))) {
                    auto arg = c->getArg(i);
                    if (i < 2)
                        args.push_back(asIterator(arg));
                    else if (i == 2 && (n == "sort" || n == "stable_sort")) {
                        auto item = iteratorElement(c->getArg(0)->getType());
                        args.push_back(comparatorPredicate(arg->getType(), item, item, expr(arg)));
                    } else if (n == "fill") {
                        auto first = bare(c->getArg(0));
                        QualType elem;
                        if (ctx.getAsArrayType(first->getType()))
                            elem = ctx.getAsArrayType(first->getType())->getElementType();
                        else if (first->getType()->isPointerType())
                            elem = first->getType()->getPointeeType();
                        else if (isIter(first->getType())) {
                            elem = element(first->getType());
                            if (elem->isPointerType())
                                elem = elem->getPointeeType();
                        }
                        args.push_back(elem.isNull() ? expr(arg) : value(arg, elem, true));
                    } else
                        args.push_back(expr(arg));
                }
            if (n == "stable_sort")
                n = "sort";
            if (n == "sort" && args.size() == 2) {
                auto item = iteratorElement(c->getArg(0)->getType());
                args.push_back(comparisonPredicate(item, item));
            }
            auto result = "Jpp." + n + "(" + join(args) + ")";
            return n == "count" ? integerResult(c, result) : result;
        }
        if (n == "min" || n == "max" || n == "clamp") {
            bool list = family(c->getArg(0)->getType()) == "initializer_list";
            unsigned comparatorIndex = list ? 1 : n == "clamp" ? 3 : 2;
            auto item = clean(c->getType());
            auto predicate = c->getNumArgs() > comparatorIndex
                                 ? comparatorPredicate(c->getArg(comparatorIndex)->getType(), item,
                                                       item, expr(c->getArg(comparatorIndex)))
                                 : comparisonPredicate(item, item);
            std::vector<std::string> args;
            for (unsigned i = 0; i < comparatorIndex; ++i)
                args.push_back(expr(c->getArg(i)));
            args.push_back(predicate);
            return "Jpp." + n + "(" + join(args) + ")";
        }
        if (n == "gcd" || n == "lcm")
            return "Jpp." + n + "(" + arguments(c) + ")";
        fail(c, "unsupported library function '" + f->getQualifiedNameAsString() + "'");
    }
    std::string arguments(const CallExpr* c, const FunctionDecl* f = nullptr) {
        std::vector<std::string> a;
        for (unsigned i = 0; i < c->getNumArgs(); ++i) {
            auto e = c->getArg(i);
            auto target = f && i < f->getNumParams() ? f->getParamDecl(i)->getType() : e->getType();
            a.push_back(f && referenceABI(target) ? reference(e) : value(e, target, f != nullptr));
        }
        return join(a);
    }
    std::string expr(const Expr* input) {
        struct Depth {
            unsigned& value;
            Depth(unsigned& value) : value(value) {
                ++value;
            }
            ~Depth() {
                --value;
            }
        } depth(expressionDepth);
        if (expressionDepth == 1 && expressionBounds(input, false)) {
            auto previous = arrayBounds;
            auto result = supplier([&] {
                captureExpressionBounds(input);
                if (input->getType()->isVoidType()) {
                    line(exprValue(input) + ";");
                    line("return null;");
                } else
                    line("return " + exprValue(input) + ";");
            });
            arrayBounds = std::move(previous);
            return result;
        }
        return exprValue(input);
    }
    std::string exprValue(const Expr* input) {
        if (auto call = dyn_cast_or_null<CallExpr>(strip(input)); call && productGet(call))
            return productRead(expr(call->getArg(0)), call->getArg(0)->getType(),
                               productIndex(call));
        auto result = expression(input);
        if (auto call = dyn_cast_or_null<CallExpr>(strip(input)); call && returnsReference(call))
            result = "(" + result + ").get()";
        auto source = input ? strip(input) : nullptr;
        bool pointerProducer = isa_and_nonnull<CallExpr>(source) ||
                               isa_and_nonnull<CXXNewExpr>(source) ||
                               isa_and_nonnull<ExplicitCastExpr>(source);
        if (auto cast = dyn_cast_or_null<ImplicitCastExpr>(source))
            pointerProducer = (cast->getCastKind() == CK_ArrayToPointerDecay &&
                               !primitiveArray(cast->getSubExpr()->getType())) ||
                              cast->getCastKind() == CK_BitCast;
        if (auto unary = dyn_cast_or_null<UnaryOperator>(source))
            pointerProducer =
                unary->getOpcode() == UO_AddrOf && !primitiveArray(unary->getSubExpr()->getType());
        if (pointerProducer && dataPointer(input->getType())) {
            auto element = clean(input->getType())->getPointeeType();
            if (element->isVoidType())
                return "Jpp.voidPointer(" + result + ")";
            if (!element->isVoidType() && !element->isPointerType())
                if (auto layout = memoryType(element); !layout.empty())
                    result = "Jpp.<" + boxed(element) + ">typedPointer(" + result + ", " + layout +
                             ", " + targetLittleEndian() + ")";
        }
        return result;
    }
    std::string expression(const Expr* input) {
        if (!input)
            return "";
        auto e = strip(input);
        if (isa<OffsetOfExpr>(e)) {
            Expr::EvalResult result;
            if (e->EvaluateAsInt(result, ctx))
                return integerLiteral(result.Val.getInt(), e->getType());
            fail(e, "runtime offsetof indices need additional layout lowering");
        }
        if (auto x = dyn_cast<IntegerLiteral>(e)) {
            return integerLiteral(x->getValue(), x->getType());
        }
        if (auto x = dyn_cast<FloatingLiteral>(e)) {
            if (longDouble(x->getType()))
                return extendedConstant(x->getValue());
            llvm::SmallString<40> b;
            x->getValue().toString(b);
            std::string v = b.str().str();
            if (v.find_first_of(".eE") == std::string::npos)
                v += ".0";
            if (type(x->getType()) == "float")
                v += 'f';
            return v;
        }
        if (auto x = dyn_cast<CharacterLiteral>(e)) {
            auto c = x->getValue();
            if (type(x->getType()) != "char")
                return std::to_string(static_cast<int32_t>(c));
            if (ctx.getTypeSize(x->getType()) == 8)
                c &= 255;
            if (c == '\n')
                return "'\\n'";
            if (c == '\r')
                return "'\\r'";
            if (c == '\t')
                return "'\\t'";
            if (c == '\'')
                return "'\\\''";
            if (c == '\\')
                return "'\\\\'";
            if (c == 0)
                return "'\\0'";
            if (c < 32 || c > 126)
                return "(char)" + std::to_string(c);
            return std::string("'") + static_cast<char>(c) + "'";
        }
        if (auto x = dyn_cast<StringLiteral>(e))
            return quote(x->getBytes().str(), true);
        if (auto x = dyn_cast<CXXBoolLiteralExpr>(e))
            return x->getValue() ? "true" : "false";
        if (isa<CXXNullPtrLiteralExpr>(e) || isa<GNUNullExpr>(e))
            return "null";
        if (auto x = dyn_cast<DeclRefExpr>(e)) {
            if (aliases.count(x->getDecl()))
                return aliases[x->getDecl()];
            auto d = x->getDecl();
            if (auto function = dyn_cast<FunctionDecl>(d))
                return functionPointer(function, ctx.getPointerType(e->getType()));
            auto n = d->getNameAsString();
            if (!user(d)) {
                auto factory = rangeFactory(d->getType());
                if (!factory.empty() && isa<VarDecl>(d))
                    return factory + ".INSTANCE";
                if (n == "ignore")
                    return "JppIgnore.INSTANCE";
                if (ordering(d->getType())) {
                    auto category = family(d->getType());
                    category = category.substr(0, category.find('_'));
                    std::transform(category.begin(), category.end(), category.begin(), ::toupper);
                    auto value = n;
                    std::transform(value.begin(), value.end(), value.begin(), ::toupper);
                    return "JppOrdering." + category + "_" + value;
                }
                if (n == "cin")
                    return "jppIn";
                if (n == "cout")
                    return "jppOut";
                if (n == "cerr")
                    return "jppErr";
                if (n == "clog")
                    return "jppLog";
                if (n == "npos")
                    return "-1L";
                if (auto en = dyn_cast<EnumConstantDecl>(d))
                    return number(en->getInitVal());
                if (auto v = dyn_cast<VarDecl>(d); v && v->isConstexpr() && v->hasInit())
                    if (auto evaluated = v->evaluateValue())
                        if (auto constant = constantValue(*evaluated, v->getType());
                            !constant.empty())
                            return constant;
                if (auto v = dyn_cast<VarDecl>(d);
                    v && v->hasInit() &&
                    v->getQualifiedNameAsString().starts_with("std::ios_base::")) {
                    Expr::EvalResult evaluated;
                    if (v->getInit()->EvaluateAsInt(evaluated, ctx))
                        return number(evaluated.Val.getInt());
                }
                fail(e, "unsupported library value '" + d->getQualifiedNameAsString() + "'");
            }
            if (auto v = dyn_cast<VarDecl>(d); v && v->isStaticDataMember())
                return name(cast<CXXRecordDecl>(v->getDeclContext())) + "." + name(v);
            if (auto v = dyn_cast<VarDecl>(d); v && (v->isFileVarDecl() || v->hasExternalStorage()))
                return options.className + "." + name(v) + (cell(v) ? ".get()" : "");
            if (auto v = dyn_cast<VarDecl>(d); cell(v))
                return name(d) + ".get()";
            if (isa<EnumConstantDecl>(d))
                return options.className + "." + name(d);
            return name(d);
        }
        if (auto x = dyn_cast<ImplicitCastExpr>(e)) {
            auto sub = x->getSubExpr();
            switch (x->getCastKind()) {
            case CK_ArrayToPointerDecay:
                if (primitiveArray(sub->getType()))
                    return arrayDecay(sub);
                return "Jpp.pointer(Jpp.begin(" + expr(sub) + "))";
            case CK_NullToPointer:
                return sub->HasSideEffects(ctx) ? "Jpp.last(" + expr(sub) + ", null)" : "null";
            case CK_FunctionToPointerDecay:
                if (auto d = dyn_cast<DeclRefExpr>(bare(sub)); d && isa<FunctionDecl>(d->getDecl()))
                    return functionPointer(cast<FunctionDecl>(d->getDecl()), x->getType());
                return expr(sub);
            case CK_IntegralToBoolean:
            case CK_FloatingToBoolean:
            case CK_PointerToBoolean:
                return truth(sub);
            case CK_IntegralCast:
            case CK_IntegralToFloating:
            case CK_FloatingToIntegral:
            case CK_FloatingCast:
                return converted(expr(sub), sub->getType(), x->getType());
            default:
                return expr(sub);
            }
        }
        if (auto x = dyn_cast<BuiltinBitCastExpr>(e))
            return bitCast(x->getSubExpr(), x->getType());
        if (auto x = dyn_cast<ExplicitCastExpr>(e)) {
            if (x->getCastKind() == CK_NullToPointer)
                return x->getSubExpr()->HasSideEffects(ctx)
                           ? "Jpp.last(" + expr(x->getSubExpr()) + ", null)"
                           : "null";
            if (x->getCastKind() == CK_IntegralToPointer) {
                if (!dataPointer(x->getType()))
                    fail(e, "integer-derived function pointers need additional address lowering");
                return "Jpp.<" +
                       (x->getType()->getPointeeType()->isVoidType()
                            ? std::string("Object")
                            : boxed(x->getType()->getPointeeType())) +
                       ">integerPointer(" + ordinal(x->getSubExpr()) + ", " +
                       std::to_string(ctx.getTypeSize(x->getType())) + ")";
            }
            if (x->getCastKind() == CK_PointerToIntegral)
                fail(e, "converting pointer addresses to integers needs byte-addressed storage");
            if (dataPointer(x->getType()) && dataPointer(x->getSubExpr()->getType())) {
                auto from = clean(x->getSubExpr()->getType()->getPointeeType());
                auto to = clean(x->getType()->getPointeeType());
                if (to->isVoidType() ||
                    (!to->isPointerType() && !memoryType(to).empty() &&
                     (from->isVoidType() || (!from->isPointerType() && !memoryType(from).empty()))))
                    return expr(x->getSubExpr());
                if (x->getCastKind() == CK_BitCast && !from->isVoidType() &&
                    !ctx.hasSameType(from, to))
                    fail(
                        e,
                        "pointer reinterpretation of this object layout needs additional lowering");
            }
            if (x->getType()->isVoidType() && x->getSubExpr()->getType()->isVoidType())
                return expr(x->getSubExpr());
            if (x->getType()->isVoidType() && primitiveArray(x->getSubExpr()->getType()) &&
                !isa<StringLiteral>(bare(x->getSubExpr())))
                return "Jpp.ignore(" + arrayLocation(x->getSubExpr()) + ")";
            if (x->getType()->isVoidType())
                return "Jpp.ignore(" + expr(x->getSubExpr()) + ")";
            if (x->getType()->isBooleanType())
                return truth(x->getSubExpr());
            if (x->getType()->isArithmeticType() || x->getType()->isEnumeralType())
                return value(x->getSubExpr(), x->getType());
            if (clean(x->getType())->isRecordType() &&
                ctx.hasSameType(clean(x->getType()), clean(x->getSubExpr()->getType())))
                return expr(x->getSubExpr());
            return "((" + type(x->getType()) + ")(" + expr(x->getSubExpr()) + "))";
        }
        if (auto x = dyn_cast<CompoundAssignOperator>(e))
            return compoundAssignment(x);
        if (auto x = dyn_cast<OpaqueValueExpr>(e)) {
            if (auto it = opaqueValues.find(x); it != opaqueValues.end())
                return it->second;
            fail(e, "an opaque GNU expression value has no binding");
        }
        if (auto copy = dyn_cast<ArrayInitLoopExpr>(e); copy && primitiveArray(copy->getType()))
            return "Jpp.arrayCopy(" + newArray(copy->getType(), nullptr) + ", " +
                   arrayLocation(copy->getCommonExpr()->getSourceExpr()) + ", " +
                   memoryType(copy->getType()) + ", " + targetLittleEndian() + ")";
        if (auto x = dyn_cast<BinaryConditionalOperator>(e)) {
            if (x->isLValue())
                return reference(x) + ".get()";
            return supplier([&] {
                auto common = temporary("jppSelected");
                line(type(x->getCommon()->getType()) + " " + common + " = " + expr(x->getCommon()) +
                     ";");
                auto previous = opaqueValues;
                opaqueValues[x->getOpaqueValue()] = common;
                line("return " + truth(x->getCond()) + " ? " + expr(x->getTrueExpr()) + " : " +
                     expr(x->getFalseExpr()) + ";");
                opaqueValues = std::move(previous);
            });
        }
        if (auto x = dyn_cast<StmtExpr>(e)) {
            std::function<void(const Stmt*, unsigned, unsigned)> validate = [&](const Stmt* s,
                                                                                unsigned loops,
                                                                                unsigned switches) {
                if (!s || isa<LambdaExpr>(s))
                    return;
                if (isa<ReturnStmt>(s) || isa<GotoStmt>(s) || isa<IndirectGotoStmt>(s))
                    fail(s, "control transfer out of a GNU statement expression requires "
                            "additional lowering");
                if ((isa<BreakStmt>(s) && !loops && !switches) || (isa<ContinueStmt>(s) && !loops))
                    fail(s, "a GNU statement expression cannot yet break or continue an "
                            "enclosing loop");
                const Stmt* loopBody = nullptr;
                if (auto loop = dyn_cast<ForStmt>(s))
                    loopBody = loop->getBody();
                else if (auto loop = dyn_cast<WhileStmt>(s))
                    loopBody = loop->getBody();
                else if (auto loop = dyn_cast<DoStmt>(s))
                    loopBody = loop->getBody();
                else if (auto loop = dyn_cast<CXXForRangeStmt>(s))
                    loopBody = loop->getBody();
                auto selection = dyn_cast<SwitchStmt>(s);
                for (auto child : s->children())
                    validate(child, loops + (child && child == loopBody),
                             switches + (selection && child == selection->getBody()));
            };
            validate(x->getSubStmt(), 0, 0);
            return supplier([&] { bodyStatements(x->getSubStmt(), true); });
        }
        if (auto x = dyn_cast<BinaryOperator>(e))
            return binary(x->getLHS(), x->getRHS(), x->getOpcodeStr().str(), x->getType());
        if (auto x = dyn_cast<CXXRewrittenBinaryOperator>(e))
            return binary(x->getLHS(), x->getRHS(), x->getOpcodeStr().str(), x->getType());
        if (auto x = dyn_cast<UnaryOperator>(e)) {
            auto a = x->getSubExpr();
            auto s = expr(a);
            if (x->getOpcode() == UO_LNot)
                return "(!" + truth(a) + ")";
            if (x->getOpcode() == UO_Plus && dataPointer(a->getType()))
                return "Jpp.pointer(" + s + ")";
            if (x->getOpcode() == UO_Deref && a->getType()->isFunctionPointerType())
                return s;
            if (x->getOpcode() == UO_Deref && (isIter(a->getType()) || dataPointer(a->getType())))
                return isa<CXXThisExpr>(bare(a))
                           ? (constructionThis.empty() ? "this" : constructionThis)
                           : s + ".get()";
            if (x->getOpcode() == UO_AddrOf && a->getType()->isFunctionType()) {
                if (auto d = dyn_cast<DeclRefExpr>(bare(a)))
                    return functionPointer(cast<FunctionDecl>(d->getDecl()), x->getType());
                return expr(a);
            }
            if (x->getOpcode() == UO_AddrOf)
                return address(a);
            if (x->isIncrementDecrementOp()) {
                int delta = x->isIncrementOp() ? 1 : -1;
                if (longDouble(a->getType()))
                    return "Jpp.updateLongDouble(" + reference(a) + ", " + std::to_string(delta) +
                           ", " + (x->isPostfix() ? "true" : "false") + ")";
                if (referenceMember(a))
                    return "Jpp.update(" + reference(a) + ", " + std::to_string(delta) + ", " +
                           (x->isPostfix() ? "true" : "false") + ")";
                if (dataPointer(a->getType())) {
                    bool bytes = clean(a->getType())->getPointeeType()->isVoidType();
                    return std::string(bytes ? "Jpp.updateVoidPointer(" : "Jpp.updatePointer(") +
                           reference(a) + ", " + std::to_string(delta) + ", " +
                           (x->isPostfix() ? "true" : "false") + ")";
                }
                if (isa<AbstractConditionalOperator>(bare(a)))
                    return "Jpp.update(" + reference(a) + ", " + std::to_string(delta) + ", " +
                           (x->isPostfix() ? "true" : "false") + ")";
                if (auto d = dyn_cast<DeclRefExpr>(bare(a));
                    d && aliases.count(d->getDecl()) && aliases[d->getDecl()].ends_with(".get()"))
                    return "Jpp.update(" + reference(a) + ", " + std::to_string(delta) + ", " +
                           (x->isPostfix() ? "true" : "false") + ")";
                if (cell(variableRef(a)))
                    return "Jpp.update(" + reference(a) + ", " + std::to_string(delta) + ", " +
                           (x->isPostfix() ? "true" : "false") + ")";
                if (isIter(a->getType()))
                    return s + (x->isPostfix() ? ".post(" : ".advance(") + std::to_string(delta) +
                           ")";
                if (auto dereference = dyn_cast<UnaryOperator>(bare(a));
                    dereference && dereference->getOpcode() == UO_Deref &&
                    (dataPointer(dereference->getSubExpr()->getType()) ||
                     isIter(dereference->getSubExpr()->getType())))
                    return "Jpp.update(" + reference(a) + ", " + std::to_string(delta) + ", " +
                           (x->isPostfix() ? "true" : "false") + ")";
                if (auto [base, index] = subscript(a);
                    base &&
                    (isVec(base->getType()) || family(base->getType()) == "deque" ||
                     isMap(base->getType()) || isString(base->getType()) ||
                     (primitiveArray(bare(base)->getType()) && !javaArray(bare(base))) ||
                     (dataPointer(base->getType()) && !ctx.getAsArrayType(bare(base)->getType()))))
                    return "Jpp.update(" + reference(a) + ", " + std::to_string(delta) + ", " +
                           (x->isPostfix() ? "true" : "false") + ")";
                if (big(a->getType())) {
                    if (x->isPostfix())
                        fail(e, "postfix update of a container element is not supported in "
                                "expressions");
                    return assign(a,
                                  big(a->getType())
                                      ? s + (delta == 1 ? ".add" : ".subtract") + "(BigInteger.ONE)"
                                      : "(" + s + (delta == 1 ? " + 1" : " - 1") + ")");
                }
                auto op = std::string(delta == 1 ? "++" : "--");
                return x->isPostfix() ? s + op : op + s;
            }
            if (big(a->getType())) {
                if (x->getOpcode() == UO_Minus)
                    return s + ".negate()";
                if (x->getOpcode() == UO_Not)
                    return s + ".not()";
            }
            if (longDouble(a->getType()) && x->getOpcode() == UO_Minus)
                return "(" + s + ").negate()";
            if (x->getOpcode() == UO_Deref)
                fail(e, "raw pointer dereference is not supported");
            return "(" + UnaryOperator::getOpcodeStr(x->getOpcode()).str() + "(" + s + "))";
        }
        if (auto x = dyn_cast<ConditionalOperator>(e); x && x->getType()->isVoidType())
            return supplier([&] {
                expressionStatement(x);
                line("return null;");
            });
        if (auto x = dyn_cast<ConditionalOperator>(e))
            return "(" + truth(x->getCond()) + " ? " + expr(x->getTrueExpr()) + " : " +
                   expr(x->getFalseExpr()) + ")";
        if (auto x = dyn_cast<ArraySubscriptExpr>(e)) {
            auto index = intArgument(x->getIdx());
            if (isa<StringLiteral>(bare(x->getBase())))
                return expr(bare(x->getBase())) + ".charAt(" + index + ")";
            if (primitiveArray(bare(x->getBase())->getType()) && !javaArray(bare(x->getBase())))
                return arrayDecay(bare(x->getBase())) + ".plus(" + index + ").get()";
            if (ctx.getAsArrayType(bare(x->getBase())->getType()))
                return expr(bare(x->getBase())) + "[" + index + "]";
            return expr(x->getBase()) + ".plus(" + index + ").get()";
        }
        if (auto x = dyn_cast<MemberExpr>(e)) {
            if (isa<CXXThisExpr>(bare(x->getBase())))
                return (constructionThis.empty() ? "this" : constructionThis) + "." +
                       name(x->getMemberDecl());
            auto base = expr(x->getBase());
            if (x->isArrow())
                base += ".get()";
            if (family(x->getBase()->getType()) == "in_fun_result" ||
                (x->isArrow() &&
                 family(x->getBase()->getType()->getPointeeType()) == "in_fun_result"))
                return base + "." + x->getMemberDecl()->getNameAsString();
            if (!user(x->getMemberDecl())) {
                auto n = x->getMemberDecl()->getNameAsString();
                if (n == "first" || n == "second") {
                    auto owner = x->getBase()->getType();
                    if (owner->isPointerType())
                        owner = owner->getPointeeType();
                    return family(owner) == "pair" ? productRead(base, owner, n == "first" ? 0 : 1)
                                                   : base + "." + n;
                }
                if (n == "npos")
                    return "-1L";
                fail(e, "unsupported library field " + n);
            }
            return base + "." + name(x->getMemberDecl());
        }
        if (auto x = dyn_cast<CXXOperatorCallExpr>(e)) {
            auto op = x->getOperator();
            if (op == OO_Call && x->getNumArgs()) {
                auto factory = rangeFactory(x->getArg(0)->getType());
                std::string result;
                if (factory == "JppIotaFactory") {
                    if (x->getNumArgs() != 3)
                        fail(x, "unbounded iota needs additional range lowering");
                    result = makeIota(x->getType(), x->getArg(1), x->getArg(2));
                } else if (factory == "JppRangesForEach") {
                    if (x->getNumArgs() != 4 || !iotaView(x->getArg(1)->getType()))
                        fail(x, "this ranges::for_each overload needs additional range lowering");
                    if (family(x->getArg(3)->getType()) != "identity")
                        fail(x, "this ranges::for_each projection needs additional lowering");
                    if (x->getArg(3)->HasSideEffects(ctx))
                        fail(x, "evaluating this projection needs additional range lowering");
                    auto callback = x->getArg(2);
                    auto callbackType = x->getDirectCallee()->getParamDecl(1)->getType();
                    if (ownsDestructor(callbackType) ||
                        callbackType.isDestructedType() != QualType::DK_none)
                        fail(callback, "callable destruction needs additional lifetime lowering");
                    if (auto closure = record(callbackType); closure && closure->isLambda()) {
                        bool copied = false, capturedThis = false;
                        for (auto capture : closure->captures()) {
                            if (capture.getCaptureKind() == LCK_StarThis)
                                fail(callback,
                                     "this closure copy needs additional callable lowering");
                            capturedThis |= capture.capturesThis();
                            if (capture.capturesVariable()) {
                                copied |= capture.getCaptureKind() == LCK_ByCopy;
                                if (auto variable = dyn_cast<VarDecl>(capture.getCapturedVar());
                                    variable && variable->isInitCapture())
                                    fail(callback,
                                         "init captures need additional callable lowering");
                            }
                        }
                        if (copied && !scalarCopyClosure(closure)) {
                            if (capturedThis || !isa<LambdaExpr>(bare(callback)))
                                fail(callback,
                                     "this closure copy needs additional callable lowering");
                            for (auto field : closure->fields())
                                if (!field->getType()->isReferenceType() &&
                                    !field->getType().isTriviallyCopyableType(ctx))
                                    fail(callback,
                                         "captured value moves need additional callable lowering");
                        }
                    }
                    auto fn = temporary("jppFunction"), item = temporary("jppItem");
                    auto movedFn = temporary("jppMovedFunction");
                    auto moved =
                        constructElement(callbackType, mappedValue(callbackType, VK_XValue, movedFn,
                                                                   "", x->getExprLoc()));
                    auto callable = mappedValue(callbackType, VK_LValue, fn, "", x->getExprLoc());
                    std::vector<Expr*> arguments{mappedValue(iotaElement(x->getArg(1)->getType()),
                                                             VK_PRValue, item, "",
                                                             x->getExprLoc())};
                    Sema::ContextRAII context(sema, semanticContext());
                    auto invocation = sema.BuildCallExpr(nullptr, callable, x->getExprLoc(),
                                                         arguments, x->getExprLoc());
                    if (invocation.isInvalid())
                        fail(callback, "cannot resolve this range callback invocation");
                    result = "JppIota.forEach(" + expr(x->getArg(1)) + ", " +
                             value(callback, callbackType, true) + ", " + movedFn + " -> " + moved +
                             ", (" + fn + ", " + item + ") -> " + expression(invocation.get()) +
                             ")";
                }
                if (!result.empty())
                    return x->getArg(0)->HasSideEffects(ctx)
                               ? "Jpp.last(" + expr(x->getArg(0)) + ", " + result + ")"
                               : result;
            }
            if (x->getNumArgs() && iotaPositionOwner(x->getArg(0)->getType()) &&
                (!x->getDirectCallee() || !user(x->getDirectCallee()))) {
                auto object = expr(x->getArg(0));
                if (op == OO_Star)
                    return object + ".get()";
                if (op == OO_Subscript)
                    return object + ".get(" + ordinal(x->getArg(1)) + ")";
                if (op == OO_PlusPlus || op == OO_MinusMinus)
                    return object + (x->getNumArgs() > 1 ? ".post(" : ".advance(") +
                           (op == OO_MinusMinus ? "BigInteger.ONE.negate()" : "BigInteger.ONE") +
                           ")";
                if (op == OO_Plus || op == OO_Minus || op == OO_PlusEqual || op == OO_MinusEqual) {
                    if (iotaPositionOwner(x->getArg(1)->getType()))
                        return converted(object + ".position.subtract(" + expr(x->getArg(1)) +
                                             ".position)",
                                         ctx.Int128Ty, x->getType());
                    auto distance = ordinal(x->getArg(1));
                    if (op == OO_Minus || op == OO_MinusEqual)
                        distance = "(" + distance + ").negate()";
                    return object +
                           (op == OO_PlusEqual || op == OO_MinusEqual ? ".advance(" : ".plus(") +
                           distance + ")";
                }
                if (op == OO_EqualEqual || op == OO_ExclaimEqual || op == OO_Less ||
                    op == OO_LessEqual || op == OO_Greater || op == OO_GreaterEqual)
                    return "(" + object + ".position.compareTo(" + expr(x->getArg(1)) +
                           ".position) " + getOperatorSpelling(op) + " 0)";
                if (op == OO_Spaceship)
                    return "new JppOrdering(Integer.signum(" + object + ".position.compareTo(" +
                           expr(x->getArg(1)) + ".position)))";
            }
            if (op == OO_Plus && x->getNumArgs() == 2 &&
                iotaPositionOwner(x->getArg(1)->getType()) &&
                (!x->getDirectCallee() || !user(x->getDirectCallee())))
                return expr(x->getArg(1)) + ".plus(" + ordinal(x->getArg(0)) + ")";
            if (op == OO_Subscript && x->getNumArgs() == 2 && iotaView(x->getArg(0)->getType()) &&
                (!x->getDirectCallee() || !user(x->getDirectCallee())))
                return expr(x->getArg(0)) + ".get(" + ordinal(x->getArg(1)) + ")";
            if (op == OO_Equal && x->getNumArgs() == 2 &&
                family(x->getArg(0)->getType()) == "vector" && x->getDirectCallee() &&
                x->getDirectCallee()->getNumParams() &&
                x->getDirectCallee()->getParamDecl(0)->getType()->isRValueReferenceType())
                return "Jpp.moveAssignVector(" + expr(x->getArg(1)) + ", " + expr(x->getArg(0)) +
                       ")";
            if (op == OO_Call && x->getNumArgs() &&
                family(x->getArg(0)->getType()) == "reference_wrapper") {
                auto object = x->getArg(0);
                auto wrapper = expr(object);
                auto callable = mappedValue(element(object->getType()), VK_LValue,
                                            wrapper + ".get()", wrapper, x->getExprLoc());
                std::vector<Expr*> arguments;
                for (unsigned i = 1; i < x->getNumArgs(); ++i)
                    arguments.push_back(const_cast<Expr*>(x->getArg(i)));
                Sema::ContextRAII context(sema, ctx.getTranslationUnitDecl());
                auto invocation = sema.BuildCallExpr(nullptr, callable, x->getExprLoc(), arguments,
                                                     x->getExprLoc());
                if (invocation.isInvalid())
                    fail(x, "cannot resolve this reference-wrapper invocation");
                return expression(invocation.get());
            }
            if (op == OO_Equal && x->getNumArgs() == 2 && tupleLike(x->getArg(0)->getType()))
                return assignProduct(x->getArg(0), x->getArg(1));
            if (op == OO_Equal && x->getNumArgs() == 2 && defaultedMove(x->getDirectCallee()))
                return assignDefaultedMove(cast<CXXMethodDecl>(x->getDirectCallee()), x->getArg(0),
                                           x->getArg(1));
            if (op == OO_Equal && x->getNumArgs() == 2 &&
                family(x->getArg(0)->getType()) == "_Swallow_assign")
                return "Jpp.last(" + expr(x->getArg(1)) + ", " + expr(x->getArg(0)) + ")";
            if (auto function = x->getDirectCallee();
                function && user(function) && !function->isImplicit() && !function->isDefaulted() &&
                !(isa<CXXMethodDecl>(function) &&
                  cast<CXXMethodDecl>(function)->getParent()->isLambda())) {
                bool method = isa<CXXMethodDecl>(function);
                std::vector<std::string> arguments;
                for (unsigned i = method ? 1 : 0; i < x->getNumArgs(); ++i) {
                    auto argument = x->getArg(i);
                    auto target = function->getParamDecl(i - (method ? 1 : 0))->getType();
                    arguments.push_back(referenceABI(target) ? reference(argument)
                                                             : value(argument, target, true));
                }
                return (method ? expr(x->getArg(0)) + "." : "") + name(function) + "(" +
                       join(arguments) + ")";
            }
            if (op == OO_Call && x->getNumArgs() &&
                family(x->getArg(0)->getType()) == "mersenne_twister_engine")
                return converted(expr(x->getArg(0)) + ".next()", ctx.UnsignedLongLongTy,
                                 x->getType());
            if (op == OO_LessLess && x->getNumArgs() == 2 && stream(x->getArg(0)->getType()))
                return outputChain(x);
            if (op == OO_GreaterGreater && x->getNumArgs() == 2 && stream(x->getArg(0)->getType()))
                return inputExpression(x);
            if (x->getNumArgs() && family(x->getArg(0)->getType()) == "duration") {
                auto object = expr(x->getArg(0));
                if (op == OO_Minus && x->getNumArgs() == 1)
                    return object + ".negate()";
                if (op == OO_Plus && x->getNumArgs() == 1)
                    return "jppCopy(" + object + ")";
                if (op == OO_PlusPlus || op == OO_MinusMinus)
                    return object + ".update(" + (op == OO_PlusPlus ? "1" : "-1") + ", " +
                           (x->getNumArgs() > 1 ? "true" : "false") + ")";
            }
            if (x->getNumArgs() && bitReference(x->getArg(0)->getType())) {
                if (op == OO_Equal && x->getNumArgs() == 2)
                    return "JppBitRef.assign(" + expr(x->getArg(1)) + ", " + expr(x->getArg(0)) +
                           ")";
                if (op == OO_Tilde)
                    return "!" + expr(x->getArg(0)) + ".get()";
            }
            if (x->getNumArgs() && family(x->getArg(0)->getType()) == "bitset") {
                auto object = expr(x->getArg(0));
                if (op == OO_Tilde)
                    return object + ".complement()";
                if (op == OO_Subscript)
                    return object + (bitReference(x->getType()) ? ".ref(" : ".get(") +
                           value(x->getArg(1), ctx.UnsignedLongLongTy) + ")";
                if (op == OO_LessLess || op == OO_GreaterGreater)
                    return object + (op == OO_LessLess ? ".shiftLeft(" : ".shiftRight(") +
                           value(x->getArg(1), ctx.UnsignedLongLongTy) + ")";
                std::string operation;
                if (op == OO_Amp || op == OO_AmpEqual)
                    operation = "and";
                if (op == OO_Pipe || op == OO_PipeEqual)
                    operation = "or";
                if (op == OO_Caret || op == OO_CaretEqual)
                    operation = "xor";
                if (!operation.empty()) {
                    if (isCompoundAssignmentOperator(op))
                        return "JppBitset." + operation + "Assign(" + expr(x->getArg(1)) + ", " +
                               object + ")";
                    return object + "." + operation + "(" + expr(x->getArg(1)) + ")";
                }
                if (op == OO_LessLessEqual || op == OO_GreaterGreaterEqual)
                    return std::string("JppBitset.") +
                           (op == OO_LessLessEqual ? "shiftLeftAssign(" : "shiftRightAssign(") +
                           value(x->getArg(1), ctx.UnsignedLongLongTy) + ", " + object + ")";
            }
            if (op == OO_Subscript) {
                auto b = x->getArg(0);
                auto i = x->getArg(1);
                return expr(b) + ".get(" + (isMap(b->getType()) ? expr(i) : intArgument(i)) + ")";
            }
            if (op == OO_Call) {
                auto object = x->getArg(0);
                auto f = x->getDirectCallee();
                if (family(object->getType()) == "less" || family(object->getType()) == "greater") {
                    auto left = x->getArg(1), right = x->getArg(2);
                    auto parameter = element(object->getType());
                    auto lhs = parameter->isVoidType() ? expr(left) : value(left, parameter);
                    auto rhs = parameter->isVoidType() ? expr(right) : value(right, parameter);
                    auto lhsType = parameter->isVoidType() ? left->getType() : parameter;
                    auto rhsType = parameter->isVoidType() ? right->getType() : parameter;
                    auto comparison = family(object->getType()) == "less"
                                          ? lessExpression(lhs, lhsType, rhs, rhsType)
                                          : lessExpression(rhs, rhsType, lhs, lhsType);
                    return object->HasSideEffects(ctx)
                               ? "Jpp.last(" + expr(object) + ", " + comparison + ")"
                               : comparison;
                }
                std::vector<std::string> a;
                for (unsigned i = 1; i < x->getNumArgs(); ++i) {
                    auto e = x->getArg(i);
                    auto target = f && i <= f->getNumParams() ? f->getParamDecl(i - 1)->getType()
                                                              : e->getType();
                    a.push_back(referenceABI(target) ? reference(e) : value(e, target, true));
                }
                auto r = record(object->getType());
                auto method = f && user(f) && r && !r->isLambda()
                                  ? name(f)
                                  : callableMethod(object->getType());
                return expr(object) + "." + method + "(" + join(a) + ")";
            }
            if (op == OO_PlusPlus || op == OO_MinusMinus) {
                auto a = x->getArg(0);
                auto delta = op == OO_PlusPlus ? "1" : "-1";
                if (isIter(a->getType()))
                    return expr(a) + (x->getNumArgs() > 1 ? ".post(" : ".advance(") + delta + ")";
            }
            if (op == OO_Arrow && x->getNumArgs() == 1 && isIter(x->getArg(0)->getType()))
                return "Jpp.pointer(" + expr(x->getArg(0)) + ")";
            if (x->getNumArgs() == 2) {
                if (auto f = x->getDirectCallee();
                    f && user(f) && isa<CXXMethodDecl>(f) &&
                    (op != OO_Equal || (!f->isImplicit() && !f->isDefaulted())))
                    return expr(x->getArg(0)) + "." + name(f) + "(" + expr(x->getArg(1)) + ")";
                return binary(x->getArg(0), x->getArg(1), getOperatorSpelling(op), x->getType());
            }
            if (op == OO_Star && x->getNumArgs() == 1)
                return expr(x->getArg(0)) + ".get()";
            fail(e, "unsupported overloaded operator");
        }
        if (auto x = dyn_cast<CallExpr>(e))
            return call(x);
        if (auto x = dyn_cast<CXXNewExpr>(e)) {
            if (x->getNumPlacementArgs())
                fail(e, "placement new needs additional object lifetime lowering");
            auto allocated = x->getAllocatedType();
            auto count = x->isArray() ? intArgument(*x->getArraySize()) : "1";
            auto index = temporary("jppIndex");
            std::string init = defaultValue(allocated);
            if (auto initializer = x->getInitializer()) {
                if (x->isArray()) {
                    auto list = dyn_cast<InitListExpr>(bare(initializer));
                    if (!list)
                        fail(e, "this array new initializer needs additional lowering");
                    for (unsigned i = list->getNumInits(); i-- > 0;)
                        init = "(" + index + " == " + std::to_string(i) + " ? " +
                               value(list->getInit(i), allocated, true) + " : " + init + ")";
                } else
                    init = value(initializer, allocated, true);
            }
            return "Jpp.allocate(" + count + ", " + index + " -> " + init + ")";
        }
        if (auto x = dyn_cast<CXXDeleteExpr>(e))
            return "Jpp.delete(" + expr(x->getArgument()) + ")";
        if (auto x = dyn_cast<CXXConstructExpr>(e))
            return construct(x);
        if (auto x = dyn_cast<CXXScalarValueInitExpr>(e))
            return defaultValue(x->getType());
        if (auto x = dyn_cast<ImplicitValueInitExpr>(e))
            return defaultValue(x->getType());
        if (auto x = dyn_cast<InitListExpr>(e)) {
            auto t = x->getType();
            std::vector<std::string> a;
            if (t->isArithmeticType() || t->isEnumeralType()) {
                if (x->getNumInits() == 0)
                    return defaultValue(t);
                if (x->getNumInits() == 1)
                    return value(x->getInit(0), t);
            }
            if (family(t) == "less" || family(t) == "greater")
                return defaultValue(t);
            if (ctx.getAsArrayType(t))
                return newArray(t, x);
            if (product(t)) {
                auto elements = productTypes(t);
                for (unsigned i = 0; i < elements.size(); ++i)
                    a.push_back(i < x->getNumInits() ? productArgument(x->getInit(i), elements[i])
                                                     : defaultValue(elements[i]));
                return productConstruct(t, a);
            }
            if (family(t) == "array") {
                if (templateArgs(t)[1].getAsIntegral().isZero())
                    return defaultValue(t);
                const InitListExpr* elements = x;
                if (x->getNumInits() == 1)
                    if (auto nested = dyn_cast<InitListExpr>(bare(x->getInit(0)));
                        nested && ctx.getAsArrayType(nested->getType()))
                        elements = nested;
                if (!templateArgs(t)[1].getAsIntegral().isZero())
                    for (auto element : elements->inits())
                        if (!(isa<ImplicitValueInitExpr>(element) &&
                              ctx.getAsArrayType(element->getType())))
                            a.push_back(value(element, this->element(t), true));
                if (a.size() == templateArgs(t)[1].getAsIntegral().getZExtValue())
                    return "new " + type(t) + "(Arrays.<" + boxed(element(t)) + ">asList(" +
                           join(a) + "))";
                return "Jpp.vectorInitialized(" + number(templateArgs(t)[1].getAsIntegral()) +
                       ", () -> " + defaultValue(element(t)) + ", Arrays.<" + boxed(element(t)) +
                       ">asList(" + join(a) + "))";
            }
            if (isVec(t)) {
                for (auto v : x->inits())
                    a.push_back(value(v, element(t), true));
                auto result = "new " + type(t) + "(Arrays.asList(" + join(a) + "))";
                return vectorStorage && family(t) == "vector" ? "Jpp.movableVector(" + result + ")"
                                                              : result;
            }
            if (auto r = record(t)) {
                if (x->getNumInits() == 1 &&
                    ctx.hasSameType(clean(x->getInit(0)->getType()), clean(t)))
                    return "jppCopy(" + expr(x->getInit(0)) + ")";
                if (user(r) && r->isAggregate() &&
                    std::any_of(x->inits().begin(), x->inits().end(),
                                [](const Expr* init) { return isa<CXXDefaultInitExpr>(init); }))
                    return supplier([&] {
                        auto object = temporary("jppAggregate");
                        line(type(t) + " " + object + " = new " + type(t) +
                             "((JppUninitialized)null);");
                        unsigned i = 0;
                        for (auto field : r->fields()) {
                            auto init = i < x->getNumInits() ? x->getInit(i) : nullptr;
                            auto previous = constructionThis;
                            if (init && isa<CXXDefaultInitExpr>(init))
                                constructionThis = object;
                            line(object + "." + name(field) + " = " +
                                 (init ? value(init, field->getType(), true)
                                       : defaultValue(field->getType())) +
                                 ";");
                            constructionThis = std::move(previous);
                            ++i;
                        }
                        line("return " + object + ";");
                    });
                unsigned i = 0;
                for (auto field : r->fields()) {
                    a.push_back(i < x->getNumInits() ? value(x->getInit(i), field->getType(), true)
                                                     : defaultValue(field->getType()));
                    ++i;
                }
                return "new " + type(t) + "(" + join(a) + ")";
            }
            fail(e, "unsupported initializer list");
        }
        if (auto x = dyn_cast<CXXStdInitializerListExpr>(e)) {
            auto il = dyn_cast<InitListExpr>(bare(x->getSubExpr()));
            if (!il)
                fail(e, "unsupported initializer list storage");
            std::vector<std::string> a;
            for (auto v : il->inits())
                a.push_back(expr(v));
            return "new ArrayList<>(Arrays.asList(" + join(a) + "))";
        }
        if (auto x = dyn_cast<UnaryExprOrTypeTraitExpr>(e)) {
            if (x->getKind() != UETT_SizeOf && x->getKind() != UETT_AlignOf &&
                x->getKind() != UETT_PreferredAlignOf)
                fail(e, "unsupported type trait");
            if (auto constant = constantValue(x); !constant.empty())
                return constant;
            auto t = x->getTypeOfArgument();
            if (t->isVoidType())
                return "1L";
            if (x->getKind() == UETT_SizeOf && t->isVariableArrayType()) {
                std::vector<std::string> factors;
                while (auto a = ctx.getAsArrayType(t)) {
                    factors.push_back("(long)(" + arrayBound(a) + ")");
                    t = a->getElementType();
                }
                factors.push_back(std::to_string(ctx.getTypeSizeInChars(t).getQuantity()) + "L");
                return "(" + join(factors, " * ") + ")";
            }
            return std::to_string(x->getKind() == UETT_SizeOf
                                      ? ctx.getTypeSizeInChars(t).getQuantity()
                                  : x->getKind() == UETT_PreferredAlignOf
                                      ? ctx.getPreferredTypeAlignInChars(t).getQuantity()
                                      : ctx.getTypeAlignInChars(t).getQuantity()) +
                   "L";
        }
        if (auto x = dyn_cast<CXXNoexceptExpr>(e))
            return x->getValue() ? "true" : "false";
        if (auto x = dyn_cast<TypeTraitExpr>(e))
            return x->getValue() ? "true" : "false";
        if (isa<CXXThisExpr>(e))
            return "Jpp.pointerValue(" + (constructionThis.empty() ? "this" : constructionThis) +
                   ")";
        if (auto x = dyn_cast<LambdaExpr>(e))
            return lambda(x);
        if (auto x = dyn_cast<SubstNonTypeTemplateParmExpr>(e))
            return expr(x->getReplacement());
        fail(e, std::string("unsupported expression ") + e->getStmtClassName());
    }
    bool scalarCopyClosure(const CXXRecordDecl* closure) const {
        if (!closure || !closure->isLambda())
            return false;
        bool copied = false;
        for (auto capture : closure->captures()) {
            if (capture.capturesThis())
                return false;
            if (!capture.capturesVariable())
                continue;
            auto variable = dyn_cast<VarDecl>(capture.getCapturedVar());
            if (!variable || variable->isInitCapture())
                return false;
            if (capture.getCaptureKind() == LCK_ByCopy) {
                copied = true;
                if (!clean(variable->getType())->isScalarType())
                    return false;
            }
        }
        return copied;
    }
    std::string lambda(const LambdaExpr* e) {
        auto f = lambdaOperator(e->getLambdaClass());
        bool copyable = scalarCopyClosure(e->getLambdaClass());
        std::vector<std::string> params;
        for (auto p : f->parameters()) {
            auto base = identifier(p->getNameAsString());
            if (std::any_of(names.begin(), names.end(),
                            [&](const auto& n) { return n.first != p && n.second == base; }))
                names[p] = temporary(base + "_lambda");
            params.push_back(parameterType(p->getType()) + " " +
                             (cells.count(p) && !references.count(p) ? "jppParam_" : "") + name(p));
        }
        auto oldAliases = aliases;
        std::vector<std::string> captures, captureCopies;
        for (auto capture : e->captures())
            if (capture.capturesVariable() && capture.getCaptureKind() == LCK_ByCopy) {
                auto v = capture.getCapturedVar();
                auto snapshot = temporary("jppCapture");
                auto outer = aliases.count(v)
                                 ? aliases[v]
                                 : (cell(dyn_cast<VarDecl>(v)) ? name(v) + ".get()" : name(v));
                if (copyable) {
                    auto storage = "JppRef<" + boxed(v->getType()) + "> " + snapshot;
                    captures.push_back(storage + " = JppRef.cell(jppCopy(" + outer + "));");
                    captureCopies.push_back(snapshot + " = JppRef.cell(jppCopy(" + snapshot +
                                            ".get()));");
                } else if (primitiveArray(v->getType())) {
                    auto layout = memoryType(v->getType());
                    auto location = v->getType()->isReferenceType()
                                        ? "(" + outer.substr(0, outer.size() - 6) + ").pointer()"
                                        : "Jpp.arrayAddress(" + outer + ", " + layout + ", " +
                                              targetLittleEndian() + ")";
                    captures.push_back("final JppRef<" + boxed(v->getType()) + "> " + snapshot +
                                       " = JppRef.at(Jpp.arrayAddress(Jpp.arrayCopy(" +
                                       newArray(clean(v->getType()), nullptr) + ", " + location +
                                       ", " + layout + ", " + targetLittleEndian() + "), " +
                                       layout + ", " + targetLittleEndian() + "));");
                } else
                    captures.push_back("final JppRef<" + boxed(v->getType()) + "> " + snapshot +
                                       " = JppRef.cell(jppCopy(" + outer + "));");
                aliases[v] = snapshot + ".get()";
            }
        // Render into the same stream, then extract the body for the surrounding expression.
        auto old = out.str();
        auto oldIndent = indent;
        auto oldCurrent = current;
        auto oldBounds = arrayBounds;
        auto oldEnclosing = enclosingFunctions.size();
        if (oldCurrent)
            enclosingFunctions.push_back(oldCurrent);
        current = f;
        out.str("");
        out.clear();
        indent = 0;
        line("{");
        ++indent;
        for (auto p : f->parameters())
            if (cells.count(p) && !references.count(p))
                line("final JppRef<" + boxed(p->getType()) + "> " + name(p) +
                     " = JppRef.cell(jppParam_" + name(p) + ");");
        for (auto p : f->parameters())
            captureArrayBounds(p->getOriginalType());
        if (auto b = dyn_cast<CompoundStmt>(f->getBody()))
            bodyStatements(b);
        else
            stmt(f->getBody());
        --indent;
        line("}");
        auto body = out.str();
        if (!body.empty() && body.back() == '\n')
            body.pop_back();
        out.str(old);
        out.clear();
        out.seekp(0, std::ios_base::end);
        indent = oldIndent;
        current = oldCurrent;
        arrayBounds = std::move(oldBounds);
        enclosingFunctions.resize(oldEnclosing);
        aliases = std::move(oldAliases);
        if (copyable) {
            auto interface = functionType(f->getType()->getAs<FunctionProtoType>());
            closureInterfaces.insert(interface);
            return "new " + interface + "Closure() { " + join(captures, " ") +
                   " protected void jppCopyCaptures() { " + join(captureCopies, " ") +
                   " } public " + parameterType(f->getReturnType()) + " call(" + join(params) +
                   ") " + body + " }";
        }
        if (!captures.empty())
            return "new " + functionType(f->getType()->getAs<FunctionProtoType>()) + "() { " +
                   join(captures, " ") + " public " + parameterType(f->getReturnType()) + " call(" +
                   join(params) + ") " + body + " }";
        return "(" + functionType(f->getType()->getAs<FunctionProtoType>()) + ") ((" +
               join(params) + ") -> " + body + ")";
    }
    std::string initialValue(const VarDecl* v) {
        auto t = v->getType();
        std::string init;
        if (auto e = v->getInit()) {
            if (clean(t)->isArithmeticType() &&
                (v->isConstexpr() || v->hasConstantInitialization()))
                if (auto evaluated = v->evaluateValue())
                    init = constantValue(*evaluated, t);
            if (init.empty())
                init = ctx.getAsArrayType(t) ? newArray(t, e) : value(e, t, true);
        } else
            init = defaultValue(t);
        return init;
    }
    std::string variable(const VarDecl* v, bool member = false) {
        auto t = v->getType();
        captureArrayBounds(t);
        ArrayBoundScope initializerBounds(arrayBounds);
        captureExpressionBounds(v->getInit());
        if (v->isStaticLocal()) {
            auto storage = staticStorage.at(v->getCanonicalDecl());
            if (t->isReferenceType()) {
                auto binding = reference(v->getInit());
                if (fileStream(t) && isa<MaterializeTemporaryExpr>(referenceSource(v->getInit())))
                    binding = "JppStaticResources.temporary(" + binding + ")";
                return "final JppRef<" + boxed(t) + "> " + name(v) + " = " + storage +
                       ".initialize(() -> " + binding + ").get()";
            }
            return "final JppRef<" + boxed(t) + "> " + name(v) + " = " + storage + "; " + storage +
                   ".initialize(() -> " + initialValue(v) + ")";
        }
        if (auto d = dyn_cast<DecompositionDecl>(v))
            return decomposition(d, member);
        if (referenceABI(t))
            return "final JppRef<" + boxed(t) + "> " + name(v) + " = " + reference(v->getInit());
        if (t->isReferenceType() && cells.count(v))
            return "final JppRef<" + boxed(t) + "> " + name(v) + " = " + reference(v->getInit());
        auto init = initialValue(v);
        if (member && fileStream(t) && !t->isReferenceType())
            init = "JppStaticResources.track(" + init + ")";
        if (cells.count(v)) {
            if (v->getInit() && (selfInitialized.count(v) || family(t) == "function" ||
                                 (record(t) && record(t)->isLambda())))
                return "final JppRef<" + boxed(t) + "> " + name(v) + " = JppRef.cell(null); " +
                       name(v) + ".set(" + init + ")";
            return "final JppRef<" + boxed(t) + "> " + name(v) + " = JppRef.cell(" + init + ")";
        }
        return type(t) + " " + name(v) + " = " + init;
    }
    std::string bindingReference(const std::string& object, QualType t, unsigned index) {
        t = clean(t);
        if (product(t))
            return productReference(object, t, index);
        if (primitiveArray(t)) {
            auto item = ctx.getAsArrayType(t)->getElementType();
            return "JppRef.at(Jpp.<" + boxed(item) + ">arrayDecay(Jpp.arrayAddress(" + object +
                   ", " + memoryType(t) + ", " + targetLittleEndian() + ")).plus(" +
                   std::to_string(index) + "))";
        }
        if (ctx.getAsArrayType(t) || isVec(t))
            return "JppRef.at(" + object + ", " + std::to_string(index) + ")";
        std::string field;
        if (auto r = record(t); r && user(r)) {
            auto selected = r->field_begin();
            std::advance(selected, index);
            field = name(*selected);
        } else
            fail(SourceLocation(), "structured binding supports pairs, arrays, and user structs");
        auto base = temporary("jppObject"), value = temporary("jppValue");
        return "JppRef.member(" + object + ", " + quote(field) + ", " + base + " -> " + base + "." +
               field + ", (" + base + ", " + value + ") -> " + base + "." + field + " = " + value +
               ")";
    }
    std::string decomposition(const DecompositionDecl* v, bool member) {
        if (member)
            fail(v->getLocation(), "global structured bindings are not supported");
        auto temp = "jppBinding" + std::to_string(++serial);
        auto t = v->getType();
        if (t->isReferenceType() && primitiveArray(t)) {
            auto item = ctx.getAsArrayType(clean(t))->getElementType();
            std::string s =
                "final JppPtr<" + boxed(item) + "> " + temp + " = " + arrayDecay(v->getInit());
            unsigned index = 0;
            for (auto binding : v->bindings()) {
                s += "; final JppRef<" + boxed(binding->getType()) + "> " + name(binding) +
                     " = JppRef.at(" + temp + ".plus(" + std::to_string(index++) + "))";
                aliases[binding] = name(binding) + ".get()";
            }
            return s;
        }
        std::string s = type(t) + " " + temp + " = " + value(v->getInit(), t, true);
        unsigned i = 0;
        for (auto b : v->bindings()) {
            s += "; final JppRef<" + boxed(b->getType()) + "> " + name(b) + " = " +
                 bindingReference(temp, t, i++);
            aliases[b] = name(b) + ".get()";
        }
        return s;
    }
    std::string statementValue(const Expr* e) {
        if (primitiveArray(e->getType()) && e->isGLValue() && !isa<StringLiteral>(bare(e)))
            return "Jpp.ignore(" + arrayLocation(e) + ")";
        auto s = expr(e);
        auto b = bare(e);
        bool assignment = false;
        if (auto bin = dyn_cast<BinaryOperator>(b))
            assignment = bin->isAssignmentOp();
        if (auto op = dyn_cast<CXXOperatorCallExpr>(b))
            assignment =
                op->getOperator() == OO_Equal || isCompoundAssignmentOperator(op->getOperator());
        auto enclosed = [&] {
            if (!s.starts_with('(') || !s.ends_with(')'))
                return false;
            unsigned depth = 0;
            char quoted = 0;
            bool escaped = false;
            for (size_t i = 0; i < s.size(); ++i) {
                auto c = s[i];
                if (quoted) {
                    if (escaped)
                        escaped = false;
                    else if (c == '\\')
                        escaped = true;
                    else if (c == quoted)
                        quoted = 0;
                } else if (c == '\'' || c == '"')
                    quoted = c;
                else if (c == '(')
                    ++depth;
                else if (c == ')' && --depth == 0 && i + 1 != s.size())
                    return false;
            }
            return depth == 0 && !quoted;
        };
        while (assignment && enclosed())
            s = s.substr(1, s.size() - 2);
        if (assignment || e->getType()->isVoidType())
            return s;
        if (auto unary = dyn_cast<UnaryOperator>(b); unary && unary->isIncrementDecrementOp())
            return s;
        // Java permits method calls and class construction as statement expressions. Other
        // discarded C++ values still need to be evaluated, including in a for-loop update.
        if ((isa<CallExpr>(b) || isa<CXXConstructExpr>(b) || isa<StmtExpr>(b) ||
             isa<BinaryConditionalOperator>(b)) &&
            s.ends_with(')') && !s.starts_with('(') && !s.starts_with('!'))
            return s;
        return "Jpp.ignore(" + s + ")";
    }
    std::string supplier(const std::function<void()>& emit) {
        auto old = out.str();
        auto oldIndent = indent;
        auto oldEnclosing = enclosingFunctions.size();
        if (current)
            enclosingFunctions.push_back(current);
        out.str("");
        out.clear();
        indent = 0;
        line("{");
        ++indent;
        emit();
        --indent;
        line("}");
        auto body = out.str();
        out.str(old);
        out.clear();
        out.seekp(0, std::ios_base::end);
        indent = oldIndent;
        enclosingFunctions.resize(oldEnclosing);
        return "Jpp.sequence(() -> " + body + ")";
    }
    std::string sequence(const Expr* first, const Expr* second) {
        return supplier([&] {
            expressionStatement(first);
            if (second->getType()->isVoidType()) {
                expressionStatement(second);
                line("return null;");
            } else
                line("return " + expr(second) + ";");
        });
    }
    void expressionStatement(const Expr* e) {
        ArrayBoundScope statementBounds(arrayBounds);
        captureExpressionBounds(e);
        auto b = bare(e);
        if (auto c = dyn_cast<ConditionalOperator>(b); c && c->getType()->isVoidType()) {
            line("if (" + truth(c->getCond()) + ")");
            block(c->getTrueExpr());
            line("else");
            block(c->getFalseExpr());
            return;
        }
        if (auto op = dyn_cast<CXXOperatorCallExpr>(b);
            op && op->getOperator() == OO_GreaterGreater && op->getNumArgs() == 2 &&
            stream(op->getArg(0)->getType())) {
            if (inputRoot(op)->HasSideEffects(ctx)) {
                line(inputExpression(op) + ";");
                return;
            }
            std::vector<std::string> parts;
            inputChain(op, parts);
            for (auto& p : parts)
                line(p + ";");
            return;
        }
        if (auto bin = dyn_cast<BinaryOperator>(b); bin && bin->getOpcode() == BO_Comma) {
            expressionStatement(bin->getLHS());
            expressionStatement(bin->getRHS());
            return;
        }
        if (auto u = dyn_cast<UnaryOperator>(b);
            u && u->isIncrementDecrementOp() && subscript(u->getSubExpr()).first &&
            !ctx.getAsArrayType(bare(subscript(u->getSubExpr()).first)->getType()) &&
            !subscript(u->getSubExpr()).first->getType()->isPointerType()) {
            auto a = u->getSubExpr();
            auto [base, index] = subscript(a);
            if (index->HasSideEffects(ctx)) {
                line(expr(e) + ";");
                return;
            }
            std::string v;
            if (big(a->getType()))
                v = expr(a) + (u->isIncrementOp() ? ".add" : ".subtract") + "(BigInteger.ONE)";
            else
                v = "(" + expr(a) + (u->isIncrementOp() ? " + 1" : " - 1") + ")";
            line(assign(a, v) + ";");
            return;
        }
        auto s = statementValue(e);
        if (s == "Jpp.ignore()")
            return;
        if (auto bin = dyn_cast<BinaryOperator>(b); bin && bin->getOpcode() == BO_Assign) {
            auto [base, index] = subscript(bin->getLHS());
            if (base && isVec(base->getType()) && s.starts_with("Jpp.set("))
                s = expr(base) + ".set(" + intArgument(index) + ", " +
                    value(bin->getRHS(), bin->getLHS()->getType(), true) + ")";
        }
        line(s + ";");
        if (auto call = dyn_cast<CallExpr>(b);
            call && call->getDirectCallee() && call->getDirectCallee()->isNoReturn())
            line("throw new IllegalStateException(\"C++ noreturn function returned\");");
    }
    bool escapesLoop(const Stmt* s) {
        if (!s || isa<LambdaExpr>(s))
            return false;
        if (isa<BreakStmt>(s))
            return true;
        if (isa<ForStmt>(s) || isa<WhileStmt>(s) || isa<DoStmt>(s) || isa<CXXForRangeStmt>(s) ||
            isa<SwitchStmt>(s))
            return false;
        if (auto branch = dyn_cast<IfStmt>(s)) {
            if (branch->isConstexpr())
                return escapesLoop(branch->getCond()->EvaluateKnownConstInt(ctx).getBoolValue()
                                       ? branch->getThen()
                                       : branch->getElse());
            if (branch->isConsteval())
                return escapesLoop(branch->isNegatedConsteval() ? branch->getThen()
                                                                : branch->getElse());
        }
        for (auto child : s->children())
            if (escapesLoop(child))
                return true;
        return false;
    }
    bool terminates(const Stmt* s) {
        if (!s)
            return false;
        if (isa<ReturnStmt>(s) || isa<CXXThrowExpr>(s) || isa<BreakStmt>(s) ||
            isa<ContinueStmt>(s) || isa<GotoStmt>(s))
            return true;
        if (auto label = dyn_cast<LabelStmt>(s))
            return terminates(label->getSubStmt());
        if (auto label = dyn_cast<CaseStmt>(s))
            return terminates(label->getSubStmt());
        if (auto label = dyn_cast<DefaultStmt>(s))
            return terminates(label->getSubStmt());
        if (auto call = dyn_cast<CallExpr>(s))
            return call->getDirectCallee() && call->getDirectCallee()->isNoReturn();
        if (auto b = dyn_cast<CompoundStmt>(s))
            return !b->body_empty() && terminates(b->body_back());
        if (auto i = dyn_cast<IfStmt>(s))
            return i->getElse() && terminates(i->getThen()) && terminates(i->getElse());
        if (auto loop = dyn_cast<ForStmt>(s)) {
            bool condition = true;
            return (!loop->getCond() ||
                    (foldedCondition(loop->getCond(), condition) && condition)) &&
                   !escapesLoop(loop->getBody());
        }
        if (auto loop = dyn_cast<WhileStmt>(s)) {
            bool condition;
            return foldedCondition(loop->getCond(), condition) && condition &&
                   !escapesLoop(loop->getBody());
        }
        if (auto loop = dyn_cast<DoStmt>(s)) {
            bool condition;
            return foldedCondition(loop->getCond(), condition) && condition &&
                   !escapesLoop(loop->getBody());
        }
        return false;
    }
    void requireCheckpointFlow(const Stmt* statement, unsigned loops = 0, unsigned breaks = 0) {
        if (!statement || isa<LambdaExpr>(statement))
            return;
        if ((isa<BreakStmt>(statement) && !breaks) || (isa<ContinueStmt>(statement) && !loops) ||
            isa<GotoStmt>(statement) || (isa<SwitchCase>(statement) && !breaks))
            fail(statement, "this control-flow exit around setjmp needs additional lowering");
        if (auto branch = dyn_cast<IfStmt>(statement)) {
            if (branch->isConstexpr()) {
                requireCheckpointFlow(branch->getCond()->EvaluateKnownConstInt(ctx).getBoolValue()
                                          ? branch->getThen()
                                          : branch->getElse(),
                                      loops, breaks);
                return;
            }
            if (branch->isConsteval()) {
                requireCheckpointFlow(branch->isNegatedConsteval() ? branch->getThen()
                                                                   : branch->getElse(),
                                      loops, breaks);
                return;
            }
        }
        if (isa<ForStmt>(statement) || isa<WhileStmt>(statement) || isa<DoStmt>(statement) ||
            isa<CXXForRangeStmt>(statement)) {
            ++loops;
            ++breaks;
        } else if (isa<SwitchStmt>(statement))
            ++breaks;
        for (auto child : statement->children())
            requireCheckpointFlow(child, loops, breaks);
    }
    bool bodyStatements(const CompoundStmt* body, bool returnValue = false, size_t first = 0) {
        struct Exit {
            const LabelDecl* label;
            size_t begin, end;
        };
        std::vector<const Stmt*> statements(body->body_begin() + first, body->body_end());
        std::vector<Exit> exits;
        for (size_t target = 0; target < statements.size(); ++target) {
            auto label = dyn_cast<LabelStmt>(statements[target]);
            if (!label)
                continue;
            size_t begin = target;
            std::function<void(const Stmt*, size_t)> find = [&](const Stmt* s, size_t index) {
                if (!s || isa<LambdaExpr>(s))
                    return;
                if (auto jump = dyn_cast<GotoStmt>(s);
                    jump && jump->getLabel() == label->getDecl()) {
                    if (index >= target)
                        fail(jump, "backward goto requires additional control flow lowering");
                    begin = std::min(begin, index);
                }
                for (auto child : s->children())
                    find(child, index);
            };
            for (size_t i = 0; i < statements.size(); ++i)
                find(statements[i], i);
            if (begin < target)
                exits.push_back({label->getDecl(), begin, target});
        }
        // Crossing forward exits become nested labeled blocks, ordered by destination.
        for (size_t i = 0; i < exits.size(); ++i)
            for (size_t j = i + 1; j < exits.size(); ++j)
                if (exits[j].begin > exits[i].begin && exits[j].begin < exits[i].end)
                    exits[j].begin = exits[i].begin;
        for (size_t i = 0; i < statements.size(); ++i)
            if (std::any_of(exits.begin(), exits.end(),
                            [&](const Exit& e) { return i >= e.begin && i < e.end; }))
                if (auto declarations = dyn_cast<DeclStmt>(statements[i]))
                    for (auto decl : declarations->decls())
                        if (auto v = dyn_cast<VarDecl>(decl); v && v->hasLocalStorage() &&
                                                              !v->hasInit() &&
                                                              hoistedVariables.insert(v).second)
                            line(variable(v) + ";");
        auto previousLabels = jumpLabels;
        unsigned resources = 0;
        bool dead = false;
        for (size_t i = 0; i < statements.size(); ++i) {
            auto child = statements[i];
            for (const auto& exit : exits)
                if (exit.end == i && jumpLabels.count(exit.label)) {
                    --indent;
                    line("}");
                    jumpLabels.erase(exit.label);
                    dead = false;
                }
            for (auto exit = exits.rbegin(); exit != exits.rend(); ++exit)
                if (exit->begin == i && !dead) {
                    auto label = name(exit->label);
                    line(label + ": {");
                    ++indent;
                    jumpLabels[exit->label] = label;
                }
            if (isa<SwitchCase>(child))
                dead = false;
            if (dead)
                continue;
            if (auto saved = checkpoint(child)) {
                if (returnValue || !exits.empty())
                    fail(child, "this setjmp context needs additional continuation lowering");
                bool hasValue = !isa<Expr>(child);
                for (size_t suffix = i + (hasValue ? 0 : 1); suffix < statements.size(); ++suffix)
                    requireCheckpointFlow(statements[suffix]);
                auto target = temporary("jppCheckpoint"), jump = temporary("jppJump");
                std::string resumed;
                line("final JppJumpTarget " + target + " = Jpp.checkpoint(" +
                     expr(saved->getArg(0)) + ");");
                if (hasValue) {
                    resumed = temporary("jppResumeValue");
                    line("int " + resumed + " = 0;");
                    checkpointValues[saved] = resumed;
                }
                line("try {");
                ++indent;
                line("while (true) {");
                ++indent;
                line("try {");
                ++indent;
                dead = bodyStatements(body, false, first + i + (hasValue ? 0 : 1));
                if (hasValue)
                    checkpointValues.erase(saved);
                if (!dead)
                    line("break;");
                --indent;
                line("} catch (JppLongJump " + jump + ") {");
                ++indent;
                line("if (" + jump + ".target != " + target + ") throw " + jump + ";");
                if (hasValue)
                    line(resumed + " = " + jump + ".value;");
                --indent;
                line("}");
                --indent;
                line("}");
                --indent;
                line("} finally {");
                ++indent;
                line(target + ".active = false;");
                --indent;
                line("}");
                break;
            }
            if (returnValue && i + 1 == statements.size())
                if (auto e = dyn_cast<Expr>(child); e && !e->getType()->isVoidType()) {
                    line("return " + value(e, e->getType(), true) + ";");
                    dead = true;
                    continue;
                }
            const Stmt* declarationStatement = child;
            while (auto label = dyn_cast<LabelStmt>(declarationStatement))
                declarationStatement = label->getSubStmt();
            auto declarations = dyn_cast<DeclStmt>(declarationStatement);
            bool ownsStream = false;
            if (declarations)
                for (auto decl : declarations->decls())
                    if (auto v = dyn_cast<VarDecl>(decl); v && scopedResource(v->getType()) &&
                                                          !v->getType()->isReferenceType() &&
                                                          !v->isStaticLocal())
                        ownsStream = true;
            if (!ownsStream) {
                stmt(child);
                dead = terminates(child);
                continue;
            }
            for (auto decl : declarations->decls()) {
                auto v = dyn_cast<VarDecl>(decl);
                if (!v)
                    fail(child, "unsupported declaration alongside a file stream");
                if (scopedResource(v->getType()) && !v->getType()->isReferenceType() &&
                    !v->isStaticLocal()) {
                    if (cell(v)) {
                        line(variable(v) + ";");
                        line("try (" + type(v->getType()) + " " + temporary("jppResource") + " = " +
                             name(v) + ".get()) {");
                    } else
                        line("try (" + variable(v) + ") {");
                    ++indent;
                    ++resources;
                } else
                    line(variable(v) + ";");
            }
        }
        if (returnValue && !dead)
            line("return null;");
        while (resources--) {
            --indent;
            line("}");
        }
        jumpLabels = std::move(previousLabels);
        return dead;
    }
    void block(const Stmt* s) {
        line("{");
        ++indent;
        if (auto b = dyn_cast_or_null<CompoundStmt>(s))
            bodyStatements(b);
        else if (s)
            stmt(s);
        --indent;
        line("}");
    }
    void stmt(const Stmt* s) {
        if (!s || isa<NullStmt>(s)) {
            line(";");
            return;
        }
        if (auto b = dyn_cast<CompoundStmt>(s)) {
            block(b);
            return;
        }
        if (auto d = dyn_cast<DeclStmt>(s)) {
            for (auto decl : d->decls()) {
                if (auto v = dyn_cast<VarDecl>(decl)) {
                    if (v->hasExternalStorage())
                        continue;
                    if (!hoistedVariables.count(v))
                        line(variable(v) + ";");
                } else if (auto alias = dyn_cast<TypedefNameDecl>(decl))
                    captureArrayBounds(alias->getUnderlyingType());
                else if (isa<UsingDecl>(decl) || isa<UsingDirectiveDecl>(decl) ||
                         isa<StaticAssertDecl>(decl) || isa<CXXRecordDecl>(decl) ||
                         isa<EnumDecl>(decl))
                    continue;
                else
                    fail(s, "unsupported local declaration");
            }
            return;
        }
        if (auto e = dyn_cast<Expr>(s)) {
            expressionStatement(e);
            return;
        }
        if (auto jump = dyn_cast<GotoStmt>(s)) {
            if (auto label = jumpLabels.find(jump->getLabel()); label != jumpLabels.end())
                line("break " + label->second + ";");
            else
                fail(jump, "goto into another scope requires additional control flow lowering");
            return;
        }
        if (auto label = dyn_cast<LabelStmt>(s)) {
            stmt(label->getSubStmt());
            return;
        }
        if (auto r = dyn_cast<ReturnStmt>(s)) {
            if (!r->getRetValue())
                line("return;");
            else if (r->getRetValue()->getType()->isVoidType()) {
                expressionStatement(r->getRetValue());
                if (!terminates(bare(r->getRetValue())))
                    line("return;");
            } else if (current && referenceABI(current->getReturnType()))
                line("return " + reference(r->getRetValue()) + ";");
            else
                line("return " +
                     value(r->getRetValue(),
                           current ? current->getReturnType() : r->getRetValue()->getType(), true) +
                     ";");
            return;
        }
        if (auto i = dyn_cast<IfStmt>(s)) {
            bool scope = i->getInit() || i->getConditionVariable();
            if (scope) {
                line("{");
                ++indent;
            }
            if (i->getInit())
                stmt(i->getInit());
            if (i->getConditionVariable())
                line(variable(i->getConditionVariable()) + ";");
            if (i->isConsteval()) {
                if (i->isNegatedConsteval())
                    stmt(i->getThen());
                else if (i->getElse())
                    stmt(i->getElse());
            } else if (i->isConstexpr()) {
                if (i->getCond()->EvaluateKnownConstInt(ctx).getBoolValue())
                    stmt(i->getThen());
                else if (i->getElse())
                    stmt(i->getElse());
            } else {
                line("if (" + truth(i->getCond()) + ")");
                block(i->getThen());
                if (i->getElse()) {
                    line("else");
                    block(i->getElse());
                }
            }
            if (scope) {
                --indent;
                line("}");
            }
            return;
        }
        if (auto w = dyn_cast<WhileStmt>(s)) {
            if (w->getConditionVariable())
                fail(s, "while condition declarations are not supported");
            line("while (" + loopCondition(w->getCond()) + ")");
            block(w->getBody());
            return;
        }
        if (auto d = dyn_cast<DoStmt>(s)) {
            line("do");
            block(d->getBody());
            line("while (" + truth(d->getCond()) + ");");
            return;
        }
        if (auto f = dyn_cast<ForStmt>(s)) {
            if (auto d = dyn_cast_or_null<DeclStmt>(f->getInit()); d && d->isSingleDecl()) {
                if (auto v = dyn_cast<VarDecl>(d->getSingleDecl());
                    v && !isa<DecompositionDecl>(v)) {
                    auto declaration = variable(v);
                    auto inc = f->getInc() ? statementValue(f->getInc()) : "";
                    line("for (" + declaration + "; " +
                         (f->getCond() ? loopCondition(f->getCond()) : "") + "; " + inc + ")");
                    block(f->getBody());
                    return;
                }
            }
            line("{");
            ++indent;
            if (f->getInit())
                stmt(f->getInit());
            std::vector<std::string> increments;
            std::function<void(const Expr*)> inc = [&](const Expr* e) {
                if (!e)
                    return;
                auto b = bare(e);
                if (auto x = dyn_cast<BinaryOperator>(b); x && x->getOpcode() == BO_Comma) {
                    inc(x->getLHS());
                    inc(x->getRHS());
                } else
                    increments.push_back(statementValue(e));
            };
            inc(f->getInc());
            line("for (; " + (f->getCond() ? loopCondition(f->getCond()) : "") + "; " +
                 join(increments) + ")");
            block(f->getBody());
            --indent;
            line("}");
            return;
        }
        if (auto f = dyn_cast<CXXForRangeStmt>(s)) {
            auto v = f->getLoopVariable();
            auto range = f->getRangeInit();
            if (primitiveArray(range->getType()) &&
                (v->getType()->isReferenceType() || !javaArray(range) ||
                 ctx.getAsArrayType(range->getType())->getElementType()->isArrayType())) {
                auto array = ctx.getAsArrayType(range->getType());
                auto item = array->getElementType();
                auto location = temporary("jppRange"), first = temporary("jppFirst"),
                     index = temporary("jppIndex");
                line("{");
                ++indent;
                if (f->getInit())
                    stmt(f->getInit());
                line("final JppPtr<" + boxed(range->getType()) + "> " + location + " = " +
                     arrayLocation(range) + ";");
                line("final JppPtr<" + boxed(item) + "> " + first + " = Jpp.arrayDecay(" +
                     location + ");");
                line("for (int " + index + " = 0; " + index + " < Jpp.arrayLength(" + location +
                     "); ++" + index + ") {");
                ++indent;
                auto at = first + ".plus(" + index + ")";
                if (auto bindings = dyn_cast<DecompositionDecl>(v)) {
                    auto selected = temporary("jppBinding");
                    if (v->getType()->isReferenceType())
                        line("final JppPtr<" + boxed(v->getType()) + "> " + selected + " = " + at +
                             ";");
                    else {
                        auto copy = temporary("jppCopy");
                        line("final " + type(v->getType()) + " " + copy + " = Jpp.arrayCopy(" +
                             newArray(clean(v->getType()), nullptr) + ", " + at + ", " +
                             memoryType(v->getType()) + ", " + targetLittleEndian() + ");");
                        line("final JppPtr<" + boxed(v->getType()) + "> " + selected +
                             " = Jpp.arrayAddress(" + copy + ", " + memoryType(v->getType()) +
                             ", " + targetLittleEndian() + ");");
                    }
                    auto element = ctx.getAsArrayType(clean(v->getType()))->getElementType();
                    unsigned part = 0;
                    for (auto binding : bindings->bindings()) {
                        line("final JppRef<" + boxed(binding->getType()) + "> " + name(binding) +
                             " = JppRef.at(Jpp.<" + boxed(element) + ">arrayDecay(" + selected +
                             ").plus(" + std::to_string(part++) + "));");
                        aliases[binding] = name(binding) + ".get()";
                    }
                } else if (v->getType()->isReferenceType()) {
                    references.insert(v);
                    line("final JppRef<" + boxed(v->getType()) + "> " + name(v) + " = JppRef.at(" +
                         at + ");");
                } else {
                    auto initial = item->isArrayType() && dataPointer(v->getType())
                                       ? "Jpp.<" + boxed(clean(v->getType())->getPointeeType()) +
                                             ">arrayDecay(" + at + ")"
                                       : converted(at + ".get()", item, v->getType());
                    line((cell(v) ? "final JppRef<" + boxed(v->getType()) + "> " + name(v) +
                                        " = JppRef.cell(" + initial + ")"
                                  : type(v->getType()) + " " + name(v) + " = " + initial) +
                         ";");
                }
                if (auto body = dyn_cast<CompoundStmt>(f->getBody()))
                    bodyStatements(body);
                else
                    stmt(f->getBody());
                --indent;
                line("}");
                --indent;
                line("}");
                return;
            }
            if (v->getType()->isReferenceType() && !isa<DecompositionDecl>(v) &&
                (isVec(range->getType()) || ctx.getAsArrayType(range->getType()) ||
                 isString(range->getType()) || family(range->getType()) == "deque")) {
                references.insert(v);
                auto seq = "jppRange" + std::to_string(++serial),
                     index = "jppIndex" + std::to_string(serial);
                line("{");
                ++indent;
                line(type(range->getType()) + " " + seq + " = " + expr(range) + ";");
                auto size =
                    ctx.getAsArrayType(range->getType()) ? seq + ".length" : seq + ".size()";
                line("for (int " + index + " = 0; " + index + " < " + size + "; ++" + index + ")");
                line("{");
                ++indent;
                line("final JppRef<" + boxed(v->getType()) + "> " + name(v) + " = JppRef.at(" +
                     seq + ", " + index + ");");
                if (auto body = dyn_cast<CompoundStmt>(f->getBody()))
                    bodyStatements(body);
                else
                    stmt(f->getBody());
                --indent;
                line("}");
                --indent;
                line("}");
                return;
            }
            if (auto d = dyn_cast<DecompositionDecl>(v)) {
                auto temp = "jppItem" + std::to_string(++serial);
                line("for (" + type(d->getType()) + " " + temp + " : " + expr(range) + ")");
                line("{");
                ++indent;
                if (!v->getType()->isReferenceType())
                    line(temp + " = jppCopy(" + temp + ");");
                unsigned k = 0;
                for (auto b : d->bindings()) {
                    line("final JppRef<" + boxed(b->getType()) + "> " + name(b) + " = " +
                         bindingReference(temp, d->getType(), k++) + ";");
                    aliases[b] = name(b) + ".get()";
                }
                if (auto body = dyn_cast<CompoundStmt>(f->getBody()))
                    bodyStatements(body);
                else
                    stmt(f->getBody());
                --indent;
                line("}");
            } else {
                auto initializer = bare(v->getInit());
                auto sourceType = initializer->getType();
                if (auto ctor = dyn_cast<CXXConstructExpr>(initializer);
                    ctor && ctor->getNumArgs() == 1)
                    sourceType = ctor->getArg(0)->getType();
                bool adapt = !ctx.hasSameType(clean(sourceType), clean(v->getType())) || cell(v);
                auto item = adapt ? temporary("jppItem") : name(v);
                line("for (" + type(sourceType) + " " + item + " : " + expr(range) + ")");
                if (adapt || copyType(v->getType())) {
                    line("{");
                    ++indent;
                    auto init = converted(item, sourceType, v->getType());
                    if (copyType(v->getType()))
                        init = "jppCopy(" + init + ")";
                    if (cell(v))
                        line("final JppRef<" + boxed(v->getType()) + "> " + name(v) +
                             " = JppRef.cell(" + init + ");");
                    else
                        line((adapt ? type(v->getType()) + " " : "") + name(v) + " = " + init +
                             ";");
                    if (auto body = dyn_cast<CompoundStmt>(f->getBody()))
                        bodyStatements(body);
                    else
                        stmt(f->getBody());
                    --indent;
                    line("}");
                } else
                    block(f->getBody());
            }
            return;
        }
        if (isa<BreakStmt>(s)) {
            line("break;");
            return;
        }
        if (isa<ContinueStmt>(s)) {
            line("continue;");
            return;
        }
        if (auto sw = dyn_cast<SwitchStmt>(s)) {
            auto previousType = switchType;
            auto previousLabels = std::move(switchLabels);
            switchLabels.clear();
            switchType = clean(sw->getCond()->getType());
            if (ctx.getTypeSize(switchType) < 32)
                switchType = ctx.IntTy;
            std::vector<const CaseStmt*> cases;
            bool ranges = false;
            for (auto label = sw->getSwitchCaseList(); label; label = label->getNextSwitchCase())
                if (auto c = dyn_cast<CaseStmt>(label)) {
                    cases.push_back(c);
                    ranges |= c->getRHS() != nullptr;
                }
            bool scope = sw->getInit() || sw->getConditionVariable() || ranges;
            if (scope) {
                line("{");
                ++indent;
            }
            if (sw->getInit())
                stmt(sw->getInit());
            if (sw->getConditionVariable())
                line(variable(sw->getConditionVariable()) + ";");
            auto selector = value(sw->getCond(), switchType);
            if (ranges) {
                auto saved = temporary("jppSwitch"), selected = temporary("jppCase");
                line("final " + type(switchType) + " " + saved + " = " + selector + ";");
                std::string dispatch = "-1";
                unsigned index = 0;
                for (auto c : cases) {
                    auto label = std::to_string(index++);
                    switchLabels[c] = label;
                    auto constant = [&](const Expr* e) {
                        auto v =
                            e->EvaluateKnownConstInt(ctx).extOrTrunc(ctx.getTypeSize(switchType));
                        v.setIsUnsigned(switchType->isUnsignedIntegerType());
                        return integerLiteral(v, switchType);
                    };
                    auto first = constant(c->getLHS());
                    std::string matches;
                    if (c->getRHS())
                        matches =
                            "(" + arithmetic(saved, switchType, first, switchType, ">=") + " && " +
                            arithmetic(saved, switchType, constant(c->getRHS()), switchType, "<=") +
                            ")";
                    else
                        matches = arithmetic(saved, switchType, first, switchType, "==");
                    dispatch = "(" + matches + " ? " + label + " : " + dispatch + ")";
                }
                line("final int " + selected + " = " + dispatch + ";");
                selector = selected;
            } else if (big(switchType))
                selector = "(" + selector + ").toString()";
            else if (ctx.getTypeSize(switchType) == 64)
                selector = "Long.toString(" + selector + ")";
            line("switch (" + selector + ")");
            block(sw->getBody());
            if (scope) {
                --indent;
                line("}");
            }
            switchType = previousType;
            switchLabels = std::move(previousLabels);
            return;
        }
        if (auto c = dyn_cast<CaseStmt>(s)) {
            std::string label;
            if (auto found = switchLabels.find(c); found != switchLabels.end())
                label = found->second;
            else {
                if (switchType.isNull())
                    fail(c, "a case label has no enclosing switch");
                auto v =
                    c->getLHS()->EvaluateKnownConstInt(ctx).extOrTrunc(ctx.getTypeSize(switchType));
                v.setIsUnsigned(switchType->isUnsignedIntegerType());
                label = big(switchType) ? quote(number(v))
                        : ctx.getTypeSize(switchType) == 64
                            ? quote(std::to_string(v.getSExtValue()))
                            : integerLiteral(v, switchType);
            }
            line("case " + label + ":");
            stmt(c->getSubStmt());
            return;
        }
        if (auto c = dyn_cast<DefaultStmt>(s)) {
            line("default:");
            stmt(c->getSubStmt());
            return;
        }
        if (auto a = dyn_cast<AttributedStmt>(s)) {
            stmt(a->getSubStmt());
            return;
        }
        fail(s, std::string("unsupported statement ") + s->getStmtClassName());
    }
    void collect(const DeclContext* dc) {
        for (auto d : dc->decls()) {
            if (!user(d))
                continue;
            if (auto ns = dyn_cast<NamespaceDecl>(d)) {
                collect(ns);
                continue;
            }
            if (auto td = dyn_cast<FunctionTemplateDecl>(d)) {
                for (auto f : td->specializations())
                    if (f->hasBody() && collected.insert(f->getCanonicalDecl()).second) {
                        functions.push_back(f);
                        collect(f);
                    }
                continue;
            }
            if (auto td = dyn_cast<ClassTemplateDecl>(d)) {
                for (auto r : td->specializations())
                    if (r->isCompleteDefinition() &&
                        collected.insert(r->getCanonicalDecl()).second) {
                        records.push_back(r);
                        collect(r);
                    }
                continue;
            }
            if (auto r = dyn_cast<CXXRecordDecl>(d)) {
                if (!r->isImplicit() && r->isCompleteDefinition() &&
                    collected.insert(r->getCanonicalDecl()).second) {
                    records.push_back(r);
                    collect(r);
                }
                continue;
            }
            if (auto f = dyn_cast<FunctionDecl>(d)) {
                if (f->doesThisDeclarationHaveABody() && !f->isImplicit() &&
                    !f->getDescribedFunctionTemplate() &&
                    collected.insert(f->getCanonicalDecl()).second) {
                    functions.push_back(f);
                    collect(f);
                }
                continue;
            }
            if (auto v = dyn_cast<VarDecl>(d)) {
                if (v->isFileVarDecl() && v->isThisDeclarationADefinition() &&
                    collected.insert(v->getCanonicalDecl()).second)
                    globals.push_back(v);
                continue;
            }
            if (auto en = dyn_cast<EnumDecl>(d))
                if (en->isCompleteDefinition())
                    enums.push_back(en);
        }
    }
    void function(const FunctionDecl* f) {
        if (!emittedFunctions.insert(f->getCanonicalDecl()).second)
            return;
        if (f->isConsteval()) {
            if (options.sourceComments)
                line("// consteval " + f->getNameAsString() + " was evaluated during translation.");
            return;
        }
        if (auto m = dyn_cast<CXXMethodDecl>(f);
            m && (m->isCopyAssignmentOperator() || m->isMoveAssignmentOperator()) &&
            (m->isImplicit() || m->isDefaulted()))
            return;
        if (auto destructor = dyn_cast<CXXDestructorDecl>(f);
            destructor && !scopedDestructor(ctx.getRecordType(destructor->getParent())))
            fail(f->getLocation(), "this destructor needs additional lifetime lowering");
        if (auto c = dyn_cast<CXXConstructorDecl>(f);
            c && c->isCopyOrMoveConstructor() && (c->isImplicit() || c->isDefaulted()))
            return;
        current = f;
        auto previousBounds = arrayBounds;
        source(f);
        std::vector<std::string> params;
        for (auto p : f->parameters()) {
            auto t = p->getType();
            params.push_back(parameterType(t) + " " +
                             (cells.count(p) && !references.count(p) ? "jppParam_" : "") + name(p));
        }
        if (auto ctor = dyn_cast<CXXConstructorDecl>(f)) {
            if (ctor->isMoveConstructor())
                params.push_back("JppMove " + temporary("jppMove"));
            line(name(ctor->getParent()) + "(" + join(params) + ")");
            line("{");
            ++indent;
            for (auto p : f->parameters())
                if (cells.count(p) && !references.count(p))
                    line("final JppRef<" + boxed(p->getType()) + "> " + name(p) +
                         " = JppRef.cell(jppParam_" + name(p) + ");");
            for (auto p : f->parameters())
                captureArrayBounds(p->getOriginalType());
            for (auto init : ctor->inits())
                if (!init->isMemberInitializer())
                    fail(f->getLocation(),
                         "base and delegating constructors are not supported yet");
            for (auto field : ctor->getParent()->fields()) {
                const Expr* initializer = nullptr;
                for (auto init : ctor->inits())
                    if (init->getMember() == field)
                        initializer = init->getInit();
                line("this." + name(field) + " = " +
                     (initializer ? value(initializer, field->getType(), true)
                                  : defaultValue(field->getType())) +
                     ";");
            }
            if (auto b = dyn_cast<CompoundStmt>(f->getBody()))
                bodyStatements(b);
            --indent;
            line("}");
        } else {
            bool stat = !isa<CXXMethodDecl>(f) || cast<CXXMethodDecl>(f)->isStatic();
            auto destructor = isa<CXXDestructorDecl>(f);
            line(std::string(destructor ? "public "
                             : stat     ? "static "
                                        : "") +
                 parameterType(f->getReturnType()) + " " + (destructor ? "close" : name(f)) + "(" +
                 join(params) + ")");
            line("{");
            ++indent;
            for (auto p : f->parameters())
                if (cells.count(p) && !references.count(p))
                    line("final JppRef<" + boxed(p->getType()) + "> " + name(p) +
                         " = JppRef.cell(jppParam_" + name(p) + ");");
            for (auto p : f->parameters())
                captureArrayBounds(p->getOriginalType());
            if (auto b = dyn_cast<CompoundStmt>(f->getBody()))
                bodyStatements(b);
            else
                stmt(f->getBody());
            if (f->isMain() && !terminates(f->getBody()))
                line("return 0;");
            --indent;
            line("}");
        }
        line();
        current = nullptr;
        arrayBounds = std::move(previousBounds);
    }
    Expr* recordField(Expr* object, const FieldDecl* field, SourceLocation loc) {
        Sema::ContextRAII context(sema, semanticContext());
        auto result = sema.BuildFieldReferenceExpr(
            object, false, loc, CXXScopeSpec(), const_cast<FieldDecl*>(field),
            DeclAccessPair::make(const_cast<FieldDecl*>(field), field->getAccess()),
            DeclarationNameInfo(field->getDeclName(), loc));
        if (result.isInvalid())
            fail(loc, "cannot resolve this defaulted member operation");
        return result.get();
    }
    std::map<const CXXRecordDecl*, std::string> defaultedMoveDefinitions() {
        std::map<const CXXRecordDecl*, std::string> definitions;
        auto previous = out.str();
        auto previousIndent = indent;
        out.str("");
        out.clear();
        indent = 2;
        // Resolving one member can select another record's implicit move. Keep
        // processing those selections before inserting methods in their classes.
        for (size_t i = 0; i < defaultedMoves.size(); ++i) {
            auto method = defaultedMoves[i];
            auto r = method->getParent();
            current = method;
            currentRecord = r;
            auto start = static_cast<size_t>(out.tellp());
            auto parameter = method->getParamDecl(0);
            auto other = parameter->getIdentifier() ? name(parameter) : "other";
            auto loc = method->getLocation();
            auto from = mappedValue(parameter->getType().getNonReferenceType(), VK_XValue, other,
                                    "JppRef.object(" + other + ")", loc);
            auto to =
                mappedValue(ctx.getRecordType(r), VK_LValue, "this", "JppRef.object(this)", loc);
            auto constructor = dyn_cast<CXXConstructorDecl>(method);
            auto n = name(r);
            line(constructor
                     ? n + "(" + n + " " + other + ", JppMove " + temporary("jppMove") + ") {"
                     : n + " " + name(method) + "(" + n + " " + other + ") {");
            ++indent;
            for (auto field : r->fields()) {
                auto source = recordField(from, field, loc);
                if (constructor)
                    line("this." + name(field) + " = " +
                         constructElement(field->getType(), source) + ";");
                else
                    line(assignElement(recordField(to, field, loc), source) + ";");
            }
            if (!constructor)
                line("return this;");
            --indent;
            line("}");
            line();
            definitions[r] += out.str().substr(start);
        }
        current = nullptr;
        currentRecord = nullptr;
        indent = previousIndent;
        out.str(previous);
        out.clear();
        out.seekp(0, std::ios_base::end);
        return definitions;
    }
    void emitRecord(const CXXRecordDecl* r) {
        if (r->isUnion())
            fail(r->getLocation(), "unions require a storage layout backend");
        if (r->getNumBases())
            fail(r->getLocation(), "class inheritance is not supported yet");
        source(r);
        currentRecord = r;
        auto defaultConstructor = sema.LookupDefaultConstructor(const_cast<CXXRecordDecl*>(r));
        bool defaultConstructible = defaultConstructor && !defaultConstructor->isDeleted();
        bool less = false;
        const CXXMethodDecl* lt = nullptr;
        for (auto m : r->methods())
            if (m->isOverloadedOperator() && m->getOverloadedOperator() == OO_Less) {
                less = true;
                lt = m;
            }
        auto n = name(r);
        auto destructor = scopedDestructor(ctx.getRecordType(r));
        if (destructor && !destructor->hasBody())
            fail(destructor->getLocation(), "the destructor definition is needed for translation");
        line("static class " + n + (byteRecords.count(r) ? " extends JppRecord" : "") +
             " implements JppAssignable<" + n + ">" +
             (r->isTriviallyCopyable() && !r->isUnion() ? ", JppTrivialValue" : "") +
             (destructor ? ", AutoCloseable" : "") + (less ? ", Comparable<" + n + ">" : ""));
        line("{");
        ++indent;
        if (byteRecords.count(r)) {
            line("JppMemoryType jppLayout() { return " + memoryType(ctx.getRecordType(r)) + "; }");
            if (!r->isAggregate())
                line(n + "(JppUninitialized " + temporary("jppUninitialized") + ") {}");
        }
        std::vector<std::string> params, assignments, defaults;
        for (auto decl : r->decls())
            if (auto field = dyn_cast<VarDecl>(decl); field && field->isStaticDataMember()) {
                auto definition = field->getDefinition();
                if (definition && definition->hasInit())
                    field = definition;
                if (!clean(field->getType())->isArithmeticType() &&
                    !clean(field->getType())->isEnumeralType())
                    fail(field->getLocation(),
                         "this static data-member type needs additional lowering");
                if (auto initializer = field->getInit();
                    initializer && (clean(field->getType())->isArithmeticType() ||
                                    clean(field->getType())->isEnumeralType())) {
                    if (constantValue(initializer).empty())
                        fail(field->getLocation(),
                             "dynamic static data-member initialization needs additional lowering");
                }
                line("static " + std::string(field->getType().isConstQualified() ? "final " : "") +
                     variable(field, true) + ";");
            }
        for (auto field : r->fields()) {
            if (field->isBitField())
                fail(field->getLocation(), "bitfields require a storage layout backend");
            auto init = field->hasInClassInitializer()
                            ? value(field->getInClassInitializer(), field->getType(), true)
                            : defaultValue(field->getType());
            line(type(field->getType()) + " " + name(field) + ";");
            defaults.push_back("this." + name(field) + " = " + init + ";");
            params.push_back(type(field->getType()) + " " + name(field));
            assignments.push_back("this." + name(field) + " = " + name(field) + ";");
        }
        if (r->isAggregate()) {
            line(n + "(JppUninitialized " + temporary("jppUninitialized") + ") {}");
            if (defaultConstructible) {
                line(n + "() {");
                ++indent;
                for (auto d : defaults)
                    line(d);
                --indent;
                line("}");
            }
            if (!params.empty()) {
                line(n + "(" + join(params) + ") {");
                ++indent;
                for (auto a : assignments)
                    line(a);
                --indent;
                line("}");
            }
        }
        const CXXConstructorDecl* customCopy = nullptr;
        const CXXMethodDecl* customAssignment = nullptr;
        bool emittedDefault = r->isAggregate() && defaultConstructible;
        for (auto method : r->methods()) {
            if (method->isCopyAssignmentOperator() && !method->isImplicit() &&
                !method->isDefaulted() && method->hasBody())
                customAssignment = method;
            if (auto constructor = dyn_cast<CXXConstructorDecl>(method)) {
                if (constructor->isCopyConstructor() && !constructor->isImplicit() &&
                    !constructor->isDefaulted() && constructor->hasBody()) {
                    if (customCopy)
                        fail(r->getLocation(),
                             "multiple copy constructors need distinct Java signatures");
                    customCopy = constructor;
                }
                if (!constructor->isImplicit() && constructor->getNumParams() == 0 &&
                    constructor->hasBody())
                    emittedDefault = true;
            }
        }
        if (!emittedDefault && !r->hasUserDeclaredConstructor() && defaultConstructible) {
            line(n + "() {");
            ++indent;
            for (auto d : defaults)
                line(d);
            --indent;
            line("}");
        }
        if (!customCopy) {
            line(n + "(" + n + " other) {");
            ++indent;
            for (auto field : r->fields())
                line("this." + name(field) + " = jppCopy(other." + name(field) + ");");
            --indent;
            line("}");
        }
        std::vector<std::string> copyArguments = {"this"};
        if (customCopy)
            for (unsigned i = 1; i < customCopy->getNumParams(); ++i) {
                auto parameter = customCopy->getParamDecl(i);
                copyArguments.push_back(
                    referenceABI(parameter->getType())
                        ? reference(parameter->getDefaultArg())
                        : value(parameter->getDefaultArg(), parameter->getType(), true));
            }
        line("public " + n + " copy() { return new " + n + "(" + join(copyArguments) + "); }");
        line("public " + n + " jppAssignFrom(" + n + " other) {");
        ++indent;
        if (customAssignment)
            line(name(customAssignment) + "(other);");
        else
            for (auto field : r->fields())
                line("this." + name(field) + " = " +
                     (copyType(field->getType())
                          ? "Jpp.assignValue(this." + name(field) + ", other." + name(field) + ")"
                          : "jppCopy(other." + name(field) + ")") +
                     ";");
        line("return this;");
        --indent;
        line("}");
        if (less)
            line("public int compareTo(" + n + " other) { return " + name(lt) +
                 "(other) ? -1 : other." + name(lt) + "(this) ? 1 : 0; }");
        for (auto f : functions)
            if (f->getDeclContext() == r)
                function(f);
        recordOperationsAt.emplace_back(r, static_cast<size_t>(out.tellp()));
        --indent;
        line("}");
        line();
        currentRecord = nullptr;
    }

  public:
    Emitter(ASTContext& c, Sema& s, const Options& o)
        : ctx(c), sema(s), options(o), sm(c.getSourceManager()) {}
    std::string generate() {
        collect(ctx.getTranslationUnitDecl());
        for (auto r : records)
            for (auto field : r->fields())
                if (ownsDestructor(field->getType()) ||
                    (scopedDestructor(ctx.getRecordType(r)) &&
                     !field->getType()->isReferenceType() &&
                     field->getType().isDestructedType() != QualType::DK_none))
                    fail(field->getLocation(),
                         "member destruction needs additional lifetime lowering");
        for (auto variable : globals)
            if (ownsDestructor(variable->getType()))
                fail(variable->getLocation(),
                     "static object destruction needs additional lifetime lowering");
        for (auto f : functions) {
            if (ownsDestructor(f->getReturnType()))
                fail(f->getLocation(),
                     "returned object destruction needs additional lifetime lowering");
            for (auto p : f->parameters()) {
                if (ownsDestructor(p->getType()))
                    fail(p->getLocation(),
                         "value-parameter destruction needs additional lifetime lowering");
                scanArrayBounds(p->getOriginalType());
                reservedNames.insert(identifier(p->getNameAsString()));
                if (referenceABI(p->getType()))
                    references.insert(p);
            }
            if (referenceABI(f->getReturnType()))
                markReturnedReferences(f->getBody());
            scan(f->getBody());
        }
        for (auto v : globals)
            scan(v->getInit());
        for (auto r : records)
            for (auto field : r->fields())
                if (field->hasInClassInitializer())
                    scan(field->getInClassInitializer());
        for (auto f : functions)
            if (auto constructor = dyn_cast<CXXConstructorDecl>(f))
                for (auto initializer : constructor->inits())
                    scan(initializer->getInit());
        for (auto r : records)
            name(r);
        for (auto f : functions)
            name(f);
        for (auto v : globals)
            name(v);
        const FunctionDecl* main = nullptr;
        for (auto f : functions)
            if (f->isMain()) {
                main = f;
                if (f->getNumParams() > 2)
                    fail(f->getLocation(),
                         "main's environment parameter needs additional process lowering");
            }
        out << "// Generated by J++ 0.1.0. Target: Java " << options.javaVersion << ".\n"
            << "// Input dialect: GNU++23. Support: "
               "https://github.com/Irvinwop/JPlusPlus/blob/main/docs/compatibility.md\n"
            << "import java.io.*;\nimport java.util.*;\nimport java.math.*;\n\n"
            << "@SuppressWarnings({\"unchecked\", \"rawtypes\"})\npublic class "
            << options.className << " {\n";
        auto startupAt = static_cast<size_t>(out.tellp());
        for (auto en : enums)
            for (auto v : en->enumerators())
                line("static final " + type(ctx.getTypeDeclType(en)) + " " + name(v) + " = " +
                     integerLiteral(v->getInitVal(), ctx.getTypeDeclType(en)) + ";");
        for (auto v : globals) {
            source(v);
            line("static " + variable(v, true) + ";");
        }
        if (!globals.empty())
            line();
        for (auto r : records)
            emitRecord(r);
        for (auto f : functions)
            if (!isa<CXXMethodDecl>(f))
                function(f);
        auto moveDefinitions = defaultedMoveDefinitions();
        for (size_t i = 0; i < interfaces.size(); ++i) {
            auto [f, n] = interfaces[i];
            std::vector<std::string> p;
            unsigned j = 0;
            for (auto t : f->param_types())
                p.push_back(parameterType(t) + " arg" + std::to_string(j++));
            bool pred = f->getNumParams() == 2 && (clean(f->getReturnType())->isArithmeticType() ||
                                                   clean(f->getReturnType())->isEnumeralType());
            auto extends = pred ? " extends java.util.function.BiPredicate<" +
                                      boxed(f->getParamType(0)) + ", " + boxed(f->getParamType(1)) +
                                      ">"
                                : "";
            std::vector<std::string> bridge;
            if (pred)
                for (unsigned i = 0; i < 2; ++i) {
                    auto t = f->getParamType(i);
                    auto value = std::string(i ? "b" : "a");
                    if (referenceABI(t))
                        value = "JppRef.cell(" + value + ")";
                    else if (copyType(t))
                        value = "jppCopy(" + value + ")";
                    bridge.push_back(value);
                }
            auto result = "call(" + join(bridge) + ")";
            if (referenceABI(f->getReturnType()))
                result += ".get()";
            line("interface " + n + extends + " { " + parameterType(f->getReturnType()) + " call(" +
                 join(p) + ");" +
                 (pred ? " default boolean test(" + boxed(f->getParamType(0)) + " a, " +
                             boxed(f->getParamType(1)) + " b) { return " +
                             booleanValue(result, f->getReturnType()) + "; }"
                       : "") +
                 " }");
            if (closureInterfaces.count(n))
                line("static abstract class " + n + "Closure extends JppClosure<" + n +
                     "> implements " + n + " {}");
        }
        out << tupleDefinitions();
        if (main) {
            line("public static void main(String[] args) {");
            ++indent;
            line("int status;");
            std::vector<std::string> arguments;
            if (main->getNumParams())
                arguments.push_back("args.length + 1");
            if (main->getNumParams() > 1)
                arguments.push_back("Jpp.arguments(" + quote(options.className) + ", args)");
            line("try { status = " + name(main) + "(" + join(arguments) + "); }");
            line("finally {");
            ++indent;
            line("jppOut.flush(); jppErr.flush(); jppLog.flush();");
            if (!staticLocals.empty() || std::any_of(globals.begin(), globals.end(), [&](auto v) {
                    return fileStream(v->getType()) && !v->getType()->isReferenceType();
                }))
                line("JppStaticResources.closeAll();");
            --indent;
            line("}");
            line("if (status != 0) System.exit(status);");
            --indent;
            line("}");
        }
        line();
        auto program = out.str();
        for (auto entry = recordOperationsAt.rbegin(); entry != recordOperationsAt.rend(); ++entry)
            program.insert(entry->second, moveDefinitions[entry->first]);
        std::string pointerFields;
        for (const auto& entry : functionPointers) {
            auto owner = isa<CXXMethodDecl>(entry.declaration)
                             ? name(cast<CXXMethodDecl>(entry.declaration)->getParent())
                             : options.className;
            pointerFields += "static final " + entry.javaType + " " + entry.field + " = " + owner +
                             "::" + name(entry.declaration) + ";\n";
        }
        for (auto v : staticLocals) {
            auto t = v->getType();
            std::string zero = "null";
            if (!t->isReferenceType()) {
                if (clean(t)->isArithmeticType() || clean(t)->isEnumeralType())
                    zero = defaultValue(t);
                else if (ctx.getAsArrayType(t))
                    zero = newArray(t, nullptr, false);
            }
            auto storedType = t->isReferenceType() ? "JppRef<" + boxed(t) + ">" : boxed(t);
            pointerFields += "static final JppStatic<" + storedType + "> " +
                             staticStorage.at(v->getCanonicalDecl()) + " = new JppStatic<" +
                             storedType + ">(" + zero + ", " +
                             (t->isReferenceType() ? "false" : "true") + ");\n";
        }
        std::string layouts;
        for (const auto& [references, name] : productReferenceKinds) {
            std::vector<std::string> values;
            for (bool reference : references)
                values.push_back(reference ? "true" : "false");
            layouts += "static final boolean[] " + name + " = {" + join(values) + "};\n";
        }
        for (const auto& [name, initializer] : arrayLayoutFields)
            layouts += "static final JppMemoryType " + name + " = " + initializer + ";\n";
        for (const auto& [bytes, name] : literalArrays)
            layouts += "static final char[] " + name + " = Jpp.charArray(" + quote(bytes, true) +
                       ", " + std::to_string(bytes.size() + 1) + ");\n";
        for (const auto& [value, name] : extendedConstants)
            layouts += "static final JppLongDouble " + name + " = JppLongDouble.parse(" +
                       quote(value) + ");\n";
        program.insert(startupAt, layouts + pointerFields);
        std::string startup;
        auto support = supportFor(program, startup);
        program.insert(startupAt, startup);
        return formatJava(program + support + "}\n");
    }
};

class Consumer final : public ASTConsumer {
    CompilerInstance& compiler;
    const Options& opt;
    std::string& output;
    bool& success;

  public:
    Consumer(CompilerInstance& c, const Options& o, std::string& s, bool& b)
        : compiler(c), opt(o), output(s), success(b) {}
    void HandleTranslationUnit(ASTContext& ctx) override {
        if (ctx.getDiagnostics().hasErrorOccurred())
            return;
        try {
            output = Emitter(ctx, compiler.getSema(), opt).generate();
            success = true;
        } catch (const Unsupported&) {
            success = false;
            output.clear();
        }
    }
};
class Action final : public ASTFrontendAction {
    const Options& opt;
    std::string& output;
    bool& success;

  public:
    Action(const Options& o, std::string& s, bool& b) : opt(o), output(s), success(b) {}
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& compiler,
                                                   llvm::StringRef) override {
        return std::make_unique<Consumer>(compiler, opt, output, success);
    }
};
class Factory final : public tooling::FrontendActionFactory {
    const Options& opt;
    std::string& output;
    bool& success;

  public:
    Factory(const Options& o, std::string& s, bool& b) : opt(o), output(s), success(b) {}
    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<Action>(opt, output, success);
    }
};
} // namespace

bool javaIdentifier(const std::string& s) {
    if (s.empty() || javaWords.count(s))
        return false;
    if (!(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_' || s[0] == '$'))
        return false;
    return std::all_of(s.begin() + 1, s.end(),
                       [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '$'; });
}
std::string identifier(const std::string& s) {
    std::string r;
    for (unsigned char c : s)
        r += (std::isalnum(c) || c == '_' || c == '$') ? static_cast<char>(c) : '_';
    if (r.empty())
        r = "anonymous";
    if (std::isdigit(static_cast<unsigned char>(r[0])))
        r = "_" + r;
    if (javaWords.count(r) || javaRuntimeNames.count(r) || r.starts_with("jpp") ||
        r.starts_with("Jpp"))
        r = "cpp_" + r;
    return r;
}
bool transpile(const Options& opt, std::string& java) {
    std::vector<std::string> args = {"-std=gnu++23",   "-fsyntax-only", "-Wno-unused-value",
                                     "-DONLINE_JUDGE", "-resource-dir", JPP_CLANG_RESOURCE_DIR};
    args.insert(args.end(), opt.compilerArgs.begin(), opt.compilerArgs.end());
    tooling::FixedCompilationDatabase db(std::filesystem::current_path().string(), args);
    tooling::ClangTool tool(db, {opt.input});
    bool success = false;
    Factory factory(opt, java, success);
    return tool.run(&factory) == 0 && success;
}
} // namespace jpp
