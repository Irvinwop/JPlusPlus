#include "transpiler.hpp"
#include "runtime.hpp"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/StmtCXX.h>
#include <clang/Format/Format.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/Lexer.h>
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

std::string quote(std::string_view s) {
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
            if (c < 32 || c == 127) {
                char b[8];
                snprintf(b, sizeof b, "\\%03o", c);
                r += b;
            } else
                r += static_cast<char>(c);
        }
    }
    return r + '"';
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
    const Options& options;
    SourceManager& sm;
    std::ostringstream out;
    int indent = 1, serial = 0;
    std::unordered_map<const NamedDecl*, std::string> names;
    std::map<std::string, int> usedNames;
    std::unordered_map<const FunctionDecl*, std::map<std::string, int>> localNames;
    std::vector<const FunctionDecl*> functions;
    std::vector<const CXXRecordDecl*> records;
    std::vector<const VarDecl*> globals;
    std::vector<const EnumDecl*> enums;
    std::unordered_set<const Decl*> collected;
    std::unordered_set<const FunctionDecl*> emittedFunctions;
    std::unordered_map<const ValueDecl*, std::string> aliases;
    std::unordered_map<const OpaqueValueExpr*, std::string> opaqueValues;
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
    struct FunctionPointerField {
        const FunctionDecl* declaration;
        std::string field, javaType;
    };
    std::vector<FunctionPointerField> functionPointers;
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
    bool isString(QualType t) const {
        return family(t) == "basic_string" || family(t) == "basic_string_view";
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
    bool isIter(QualType t) const {
        auto n = family(t);
        return n.find("iterator") != std::string::npos || n == "__normal_iterator";
    }
    bool dataPointer(QualType t) const {
        t = clean(t);
        return t->isPointerType() && !t->getPointeeType()->isFunctionType();
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
    QualType element(QualType t, unsigned i = 0) {
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
            if (ctx.getTypeSize(t) != 32 && ctx.getTypeSize(t) != 64)
                fail(SourceLocation(),
                     "extended floating-point types need a software precision backend");
            return ctx.getTypeSize(t) <= 32 ? "float" : "double";
        }
        if (auto a = ctx.getAsArrayType(t))
            return type(a->getElementType()) + "[]";
        if (t->isPointerType()) {
            auto p = t->getPointeeType();
            if (p->isFunctionType())
                return functionType(p->getAs<FunctionProtoType>());
            return "JppPtr<" + (p->isVoidType() ? std::string("?") : boxed(p)) + ">";
        }
        auto n = family(t);
        if (n == "basic_ifstream" || n == "basic_istream")
            return "JppInput";
        if (n == "basic_ofstream" || n == "basic_ostream")
            return "JppOutput";
        if (auto r = record(t); r && r->isLambda())
            return functionType(lambdaOperator(r)->getType()->getAs<FunctionProtoType>());
        if (isString(t))
            return "JppString";
        if (isVec(t))
            return "ArrayList<" + boxed(element(t)) + ">";
        if (n == "pair")
            return "JppPair<" + boxed(element(t)) + ", " + boxed(element(t, 1)) + ">";
        if (isMap(t))
            return "JppMap<" + boxed(element(t)) + ", " + boxed(element(t, 1)) + ">";
        if (isSet(t))
            return "JppSet<" + boxed(element(t)) + ">";
        if (n == "queue" || n == "deque" || n == "stack")
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
    std::string defaultValue(QualType t) {
        if (t->isReferenceType())
            t = t.getNonReferenceType();
        if (t->isNullPtrType())
            return "null";
        if (ctx.getAsArrayType(t))
            return newArray(t, nullptr);
        auto jt = type(t);
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
            return "Jpp.vectorDefault(" + number(a[1].getAsIntegral()) + ", () -> " +
                   defaultValue(element(t)) + ")";
        }
        if (family(t) == "pair")
            return "new " + jt + "(" + defaultValue(element(t)) + ", " +
                   defaultValue(element(t, 1)) + ")";
        if (auto r = record(t); r && user(r))
            for (auto constructor : r->ctors())
                if (constructor->isDefaultConstructor() && constructor->getNumParams()) {
                    std::vector<std::string> arguments;
                    for (auto parameter : constructor->parameters()) {
                        auto argument = parameter->getDefaultArg();
                        auto target = parameter->getType();
                        arguments.push_back(scalarReference(target)
                                                ? reference(argument)
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
        if (family(target) == "function" && !from->isNullPtrType() && type(from) != type(target)) {
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
                    auto argument = scalarReference(accepted) ? parameter + ".get()" : parameter;
                    if (scalarReference(needed) && scalarReference(accepted) &&
                        ctx.hasSameType(clean(accepted), clean(needed)))
                        argument = parameter;
                    else {
                        argument = converted(argument, accepted, needed);
                        if (scalarReference(needed))
                            argument = "JppRef.cell(" + argument + ")";
                        else if (copyType(needed))
                            argument = "jppCopy(" + argument + ")";
                    }
                    arguments.push_back(argument);
                }
                auto invocation =
                    function + "." + callableMethod(from) + "(" + join(arguments) + ")";
                auto body = destination->getReturnType()->isVoidType()
                                ? "{ " + invocation + "; }"
                                : converted(invocation, source->getReturnType(),
                                            destination->getReturnType());
                line("return (" + type(target) + ") ((" + join(parameters) + ") -> " + body + ");");
            });
        }
        if (family(from) == "pair" && family(target) == "pair" && !ctx.hasSameType(from, target)) {
            auto first = temporary("jppFirst"), second = temporary("jppSecond");
            return "Jpp.convertPair(" + s + ", " + first + " -> " +
                   converted(first, element(from), element(target)) + ", " + second + " -> " +
                   converted(second, element(from, 1), element(target, 1)) + ")";
        }
        if (target->isBooleanType())
            return booleanValue(s, from);
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
        return s;
    }
    std::string value(const Expr* e, QualType target, bool copy = false) {
        if (dataPointer(target))
            return "Jpp.pointer(" + expr(e) + ")";
        auto s = expr(e);
        if (isString(target) && !isString(e->getType()))
            s = "new JppString(" + s + ")";
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
    bool scalarReference(QualType t) const {
        return t->isReferenceType() &&
               (clean(t)->isArithmeticType() || clean(t)->isEnumeralType() || dataPointer(t));
    }
    std::string parameterType(QualType t) {
        return scalarReference(t) ? "JppRef<" + boxed(t) + ">" : type(t);
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
            if (binary->getOpcode() == BO_Assign)
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
    void scan(const Stmt* s) {
        if (!s)
            return;
        if (auto argument = dyn_cast<CXXDefaultArgExpr>(s))
            scan(argument->getExpr());
        if (auto r = dyn_cast<DeclRefExpr>(s))
            reservedNames.insert(identifier(r->getDecl()->getNameAsString()));
        if (auto l = dyn_cast<LambdaExpr>(s)) {
            auto callable = lambdaOperator(l->getLambdaClass());
            for (auto p : callable->parameters()) {
                reservedNames.insert(identifier(p->getNameAsString()));
                if (scalarReference(p->getType()))
                    references.insert(p);
            }
            for (auto capture : l->captures())
                if (capture.capturesVariable())
                    if (auto v = dyn_cast<VarDecl>(capture.getCapturedVar()))
                        cells.insert(v);
            if (callable->getBody() != l->getBody())
                scan(callable->getBody());
        }
        if (auto d = dyn_cast<DeclStmt>(s))
            for (auto decl : d->decls())
                if (auto v = dyn_cast<VarDecl>(decl)) {
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
                    if (scalarReference(v->getType())) {
                        references.insert(v);
                        markReference(v->getInit());
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
                if (scalarReference(f->getParamDecl(i)->getType()))
                    markReference(c->getArg(i + offset));
        }
        if (auto c = dyn_cast<CXXOperatorCallExpr>(s); c && c->getOperator() == OO_Call)
            if (auto function = c->getDirectCallee())
                for (unsigned i = 0; i < function->getNumParams() && i + 1 < c->getNumArgs(); ++i)
                    if (scalarReference(function->getParamDecl(i)->getType()))
                        markReference(c->getArg(i + 1));
        if (auto c = dyn_cast<CXXConstructExpr>(s))
            for (unsigned i = 0; i < c->getConstructor()->getNumParams() && i < c->getNumArgs();
                 ++i)
                if (scalarReference(c->getConstructor()->getParamDecl(i)->getType()))
                    markReference(c->getArg(i));
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
                    if (scalarReference(prototype->getParamType(i)))
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
    std::string reference(const Expr* e) {
        auto wrapped = referenceSource(e);
        if (isa<MaterializeTemporaryExpr>(wrapped))
            return "JppRef.cell(" + expr(e) + ")";
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
            if (function && function->getReturnType()->isReferenceType() &&
                (!user(function) || function->isImplicit() || function->isDefaulted()))
                return "Jpp.assignReference(" +
                       value(assignment->getArg(1), assignment->getArg(0)->getType()) + ", " +
                       reference(assignment->getArg(0)) + ")";
        }
        if (auto c = dyn_cast<CallExpr>(bare(e));
            c && c->getDirectCallee() && !user(c->getDirectCallee())) {
            auto n = c->getDirectCallee()->getNameAsString();
            if ((n == "move" || n == "forward" || n == "as_const") && c->getNumArgs())
                return reference(c->getArg(0));
            if (auto call = dyn_cast<CXXMemberCallExpr>(c)) {
                auto member = dyn_cast<MemberExpr>(bare(call->getCallee()));
                if (member) {
                    auto objectType = member->getBase()->getType();
                    if (objectType->isPointerType())
                        objectType = objectType->getPointeeType();
                    if (isVec(objectType) || isString(objectType)) {
                        auto object = expr(member->getBase()) + (member->isArrow() ? ".get()" : "");
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
        if (auto v = variableRef(e)) {
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
        if (base && dataPointer(base->getType()) && !ctx.getAsArrayType(bare(base)->getType()))
            return "JppRef.at(" + expr(base) + ".plus(" + intArgument(index) + "))";
        if (base)
            return "JppRef.at(" + expr(bare(base)) + ", " +
                   (isMap(base->getType()) ? expr(index) : intArgument(index)) + ")";
        if (auto dereference = dyn_cast<UnaryOperator>(bare(e));
            dereference && dereference->getOpcode() == UO_Deref)
            return "JppRef.at(" + expr(dereference->getSubExpr()) + ")";
        if (auto m = dyn_cast<MemberExpr>(bare(e))) {
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
        if (auto dereference = dyn_cast<UnaryOperator>(bare(e));
            dereference && dereference->getOpcode() == UO_Deref)
            return "Jpp.pointer(" + expr(dereference->getSubExpr()) + ")";
        return "(" + reference(e) + ").pointer()";
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
        if (t->isArithmeticType() || t->isEnumeralType())
            return "((" + s + ") != 0)";
        if (t->isPointerType())
            return "(" + s + " != null)";
        if (stream(t))
            return "(" + s + ".good)";
        fail(SourceLocation(), "condition cannot be represented as a Java boolean");
    }
    std::string truth(const Expr* e) {
        e = strip(e);
        if (auto cast = dyn_cast<ImplicitCastExpr>(e)) {
            if (cast->getCastKind() == CK_IntegralToBoolean ||
                cast->getCastKind() == CK_FloatingToBoolean ||
                cast->getCastKind() == CK_PointerToBoolean)
                return truth(cast->getSubExpr());
        }
        return booleanValue(expr(e), e->getType());
    }
    std::string newArray(QualType t, const Expr* init, bool initializeObjects = true) {
        auto a = ctx.getAsArrayType(t);
        if (!a)
            return defaultValue(t);
        auto elem = a->getElementType();
        if (init) {
            if (auto literal = dyn_cast<StringLiteral>(bare(init)); literal && elem->isCharType()) {
                auto size = dyn_cast<ConstantArrayType>(a);
                if (!size)
                    fail(init, "a string-initialized character array needs a fixed bound");
                return "Jpp.charArray(" + quote(literal->getBytes().str()) + ", " +
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
                auto literal = "new " + type(t) + " {" + join(values) + "}";
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
            std::string size;
            if (auto c = dyn_cast<ConstantArrayType>(ar))
                size = std::to_string(c->getSize().getZExtValue());
            else if (auto v = dyn_cast<VariableArrayType>(ar))
                size = intArgument(v->getSizeExpr());
            else
                fail(SourceLocation(), "array bound is not known");
            dims += "[" + size + "]";
            part = ar->getElementType();
        }
        std::string allocation = "new " + base + dims;
        if (base.find('<') != std::string::npos) {
            std::string raw;
            int nesting = 0;
            for (char c : base) {
                if (c == '<')
                    ++nesting;
                else if (c == '>')
                    --nesting;
                else if (!nesting)
                    raw += c;
            }
            allocation = "(" + type(t) + ") new " + raw + dims;
        }
        if ((initializeObjects && part->isRecordType()) || big(part))
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
        if (isa<AbstractConditionalOperator>(bare(lhs)))
            return reference(lhs) + ".set(" + rhs + ")";
        if (auto d = dyn_cast<DeclRefExpr>(bare(lhs));
            d && aliases.count(d->getDecl()) && aliases[d->getDecl()].ends_with(".get()"))
            return reference(lhs) + ".set(" + rhs + ")";
        if (auto v = variableRef(lhs); cell(v))
            return reference(lhs) + ".set(" + rhs + ")";
        auto [base, index] = subscript(lhs);
        if (base && dataPointer(base->getType()) && !ctx.getAsArrayType(bare(base)->getType()))
            return expr(base) + ".plus(" + intArgument(index) + ").put(" + rhs + ")";
        if (base && ctx.getAsArrayType(bare(base)->getType()))
            return expr(bare(base)) + "[" + intArgument(index) + "] = " + rhs;
        if (base && isVec(base->getType()))
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
        if (auto common = commonArithmeticType(leftType, rightType); !common.isNull())
            return arithmetic(converted(left, leftType, common), common,
                              converted(right, rightType, common), common, "<");
        if (family(leftType) == "pair" && family(rightType) == "pair") {
            auto first = lessExpression(left + ".first", element(leftType), right + ".first",
                                        element(rightType));
            auto reverse = lessExpression(right + ".first", element(rightType), left + ".first",
                                          element(leftType));
            auto second = lessExpression(left + ".second", element(leftType, 1), right + ".second",
                                         element(rightType, 1));
            return "(" + first + " || (!" + reverse + " && " + second + "))";
        }
        if (isVec(leftType) && isVec(rightType))
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
            return comparisonPredicate(leftType, rightType, kind == "greater",
                                       parameter->isVoidType() ? QualType{} : parameter);
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
                if (scalarReference(target))
                    argument = "JppRef.cell(" + argument + ")";
                else if (copyType(target))
                    argument = "jppCopy(" + argument + ")";
                arguments.push_back(argument);
            }
            auto invocation =
                function + "." + callableMethod(comparator) + "(" + join(arguments) + ")";
            line("return (java.util.function.BiPredicate<" + boxed(leftType) + ", " +
                 boxed(rightType) + ">) ((" + join(parameters) + ") -> " +
                 booleanValue(invocation, prototype->getReturnType()) + ");");
        });
    }
    bool addressHasSideEffects(const Expr* e) const {
        if (auto [base, index] = subscript(e); base)
            return isMap(base->getType()) || base->HasSideEffects(ctx) ||
                   index->HasSideEffects(ctx);
        if (auto m = dyn_cast<MemberExpr>(bare(e)))
            return m->getBase()->HasSideEffects(ctx);
        return e->HasSideEffects(ctx);
    }
    std::string compoundAssignment(const CompoundAssignOperator* e) {
        if (dataPointer(e->getLHS()->getType())) {
            if (clean(e->getLHS()->getType())->getPointeeType()->isVoidType())
                fail(e, "GNU void-pointer arithmetic needs byte-addressed storage");
            auto left = temporary("jppPointer"), right = temporary("jppOffset");
            return "Jpp.compound(" + intArgument(e->getRHS()) + ", " + reference(e->getLHS()) +
                   ", (" + left + ", " + right + ") -> " + left + ".plus(" +
                   (e->getOpcode() == BO_SubAssign ? "-(" + right + ")" : right) + "))";
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
        if (!addressHasSideEffects(lhs) && !cell(variableRef(lhs)) && !referenceAlias &&
            !rhs->HasSideEffects(ctx)) {
            auto indexed = dyn_cast<ArraySubscriptExpr>(bare(lhs));
            bool javaLvalue = dr || isa<MemberExpr>(bare(lhs)) ||
                              (indexed && ctx.getAsArrayType(bare(indexed->getBase())->getType()));
            if (javaLvalue && !t->isBooleanType() && !big(t) &&
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
        return "Jpp.compound(" + expr(rhs) + ", " + reference(lhs) + ", (" + left + ", " + right +
               ") -> " + calculate(left, right) + ")";
    }
    std::string binary(const Expr* lhs, const Expr* rhs, std::string op, QualType result) {
        if ((op == "+" || op == "-") && ((dataPointer(lhs->getType()) &&
                                          clean(lhs->getType())->getPointeeType()->isVoidType()) ||
                                         (dataPointer(rhs->getType()) &&
                                          clean(rhs->getType())->getPointeeType()->isVoidType())))
            fail(lhs, "GNU void-pointer arithmetic needs byte-addressed storage");
        auto position = [&](QualType t) { return isIter(t) || dataPointer(t); };
        if ((op == "+=" || op == "-=") && isIter(lhs->getType()))
            return "Jpp.advance(" + std::string(op == "-=" ? "-(" : "(") + intArgument(rhs) +
                   "), " + expr(lhs) + ")";
        if (op == "+" && position(rhs->getType()) && lhs->getType()->isIntegerType())
            return asIterator(rhs) + ".plus(" + intArgument(lhs) + ")";
        if (op == "-" && position(lhs->getType()) && position(rhs->getType())) {
            auto difference = asIterator(lhs) + ".minus(" + asIterator(rhs) + ")";
            return ctx.getTypeSize(clean(result)) > 32 ? "(long)(" + difference + ")" : difference;
        }
        if (op == "=") {
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
        if (comparison && (lhs->getType()->isRecordType() || rhs->getType()->isRecordType()))
            return "(Jpp.compare(" + expr(lhs) + ", " + expr(rhs) + ") " + op + " 0)";
        return arithmetic(expr(lhs), lhs->getType(), expr(rhs), rhs->getType(), op);
    }
    std::string construct(const CXXConstructExpr* c) {
        if (c->getConstructor()->isConsteval())
            fail(c, "consteval object construction needs additional constant-value lowering");
        auto t = c->getType();
        std::vector<const Expr*> args;
        for (auto a : c->arguments())
            if (user(c->getConstructor()) || !isa<CXXDefaultArgExpr>(a))
                args.push_back(a);
        if (c->getConstructor()->isCopyOrMoveConstructor() && !args.empty()) {
            auto constructor = c->getConstructor();
            if (user(constructor) && !constructor->isImplicit() && !constructor->isDefaulted()) {
                std::vector<std::string> bound;
                for (unsigned i = 0; i < args.size(); ++i) {
                    auto target = constructor->getParamDecl(i)->getType();
                    bound.push_back(scalarReference(target) ? reference(args[i])
                                                            : value(args[i], target, true));
                }
                if (constructor->isMoveConstructor())
                    bound.push_back("true");
                return "new " + type(t) + "(" + join(bound) + ")";
            }
            return "jppCopy(" + expr(args[0]) + ")";
        }
        if (args.empty())
            return defaultValue(t);
        auto n = family(t);
        if (fileStream(t)) {
            std::vector<std::string> a = {"Jpp.cString(" + expr(args[0]) + ")"};
            if (args.size() > 1)
                a.push_back(expr(args[1]));
            return "new " + type(t) + "(" + join(a) + ")";
        }
        if (isString(t)) {
            std::vector<std::string> a;
            for (auto e : args)
                a.push_back(expr(e));
            if (args.size() == 2)
                a[0] = intArgument(args[0]);
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
        if (n == "pair") {
            if (args.size() == 1 && family(args[0]->getType()) == "pair")
                return converted(expr(args[0]), args[0]->getType(), t);
            std::vector<std::string> a;
            for (unsigned i = 0; i < args.size(); ++i)
                a.push_back(value(args[i], element(t, i), true));
            return "new " + type(t) + "(" + join(a) + ")";
        }
        if (n == "function")
            return converted(expr(args[0]), args[0]->getType(), t);
        if (isIter(t))
            return expr(args[0]);
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
        if (n == "greater" || n == "less")
            return "(a, b) -> Jpp.compare(a, b) " + std::string(n == "less" ? "<" : ">") + " 0";
        std::vector<std::string> a;
        for (unsigned i = 0; i < args.size(); ++i) {
            QualType target = i < c->getConstructor()->getNumParams()
                                  ? c->getConstructor()->getParamDecl(i)->getType()
                                  : args[i]->getType();
            a.push_back(user(c->getConstructor()) && scalarReference(target)
                            ? reference(args[i])
                            : value(args[i], target, true));
        }
        return "new " + type(t) + "(" + join(a) + ")";
    }
    std::string storedElement(const Expr* e, QualType target, bool move) {
        auto result = value(e, target);
        if (!copyType(target))
            return result;
        if (auto r = record(target); move && r && user(r))
            for (auto constructor : r->ctors())
                if (constructor->isMoveConstructor() && !constructor->isImplicit() &&
                    !constructor->isDefaulted() && !constructor->isDeleted() &&
                    constructor->hasBody())
                    return "new " + type(target) + "(" + result + ", true)";
        return "jppCopy(" + result + ")";
    }
    std::string asIterator(const Expr* e) {
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
        if (family(target) == "pair") {
            if (args.size() == 1)
                return value(args[0], target, true);
            for (unsigned i = 0; i < args.size(); ++i)
                values.push_back(value(args[i], element(target, i), true));
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
        if (auto m = dyn_cast<CXXMemberCallExpr>(c)) {
            auto me = dyn_cast<MemberExpr>(bare(m->getCallee()));
            if (!me)
                fail(c, "unsupported member call");
            auto obj = me->getBase();
            auto ot = obj->getType();
            if (ot->isPointerType())
                ot = ot->getPointeeType();
            if (f && user(f)) {
                auto method = cast<CXXMethodDecl>(f);
                auto allocatedName = method->getParent()->isLambda() ? "call" : name(f);
                auto object = me->isArrow() ? expr(obj) + ".get()" : expr(obj);
                return object + "." + allocatedName + "(" + arguments(c, f) + ")";
            }
            auto n = me->getMemberDecl()->getNameAsString();
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
                args.push_back(f && user(f) && scalarReference(target) ? reference(a)
                                                                       : value(a, target, true));
            }
            auto o = me->isArrow() ? expr(obj) + ".get()" : expr(obj);
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
            if (isVec(ot) && (n == "begin" || n == "end" || n == "cbegin" || n == "cend" ||
                              n == "rbegin" || n == "rend"))
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
            if (isVec(ot) && (n == "erase" || n == "insert" || n == "assign")) {
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
            if (n == "resize" && isVec(ot)) {
                if (c->getNumArgs() > 1 && !isa<CXXDefaultArgExpr>(c->getArg(1)))
                    return "Jpp.resize(" + o + ", " + intArgument(c->getArg(0)) + ", " +
                           value(c->getArg(1), element(ot), false) + ")";
                return "Jpp.resizeDefault(" + o + ", " + intArgument(c->getArg(0)) + ", () -> " +
                       defaultValue(element(ot)) + ")";
            }
            if (n == "reserve" && isVec(ot))
                return o + ".ensureCapacity(" + intArgument(c->getArg(0)) + ")";
            if (n == "reserve" && family(ot).starts_with("unordered_"))
                return "Jpp.ignore(" + o + ", " + expr(c->getArg(0)) + ")";
            if (n == "fill" && family(ot) == "array")
                return "Jpp.fill(" + o + ", " + args[0] + ")";
            if (n == "at" && (isVec(ot) || isString(ot)))
                return o + ".get(" + intArgument(c->getArg(0)) + ")";
            if (n == "substr" || n == "find") {
                if (isString(ot)) {
                    for (size_t i = (n == "substr" ? 0 : 1); i < args.size(); ++i)
                        args[i] = intArgument(c->getArg(i));
                }
            }
            if (n == "c_str" && isString(ot))
                return o + ".c_str()";
            if (n == "data" && isVec(ot))
                return "Jpp.pointer(Jpp.begin(" + o + "))";
            if (n == "pop" && family(ot) == "stack")
                return o + ".pop_back()";
            static const std::set<std::string> methods = {
                "size",        "length", "empty",  "clear",     "front",      "back",
                "top",         "push",   "pop",    "push_back", "push_front", "pop_back",
                "pop_front",   "begin",  "end",    "cbegin",    "cend",       "rbegin",
                "rend",        "erase",  "insert", "count",     "contains",   "lower_bound",
                "upper_bound", "assign", "substr", "find",      "append",     "at",
                "swap",        "merge"};
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
                args.push_back(scalarReference(target) ? reference(arg) : value(arg, target, true));
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
            }
            return prefix + name(f) + "(" + arguments(c, f) + ")";
        }
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
        if (n == "__assert_fail")
            return "Jpp.assertFail()";
        if (n == "make_pair")
            return "new " + type(c->getType()) + "(" + arguments(c) + ")";
        if (n == "swap")
            return "Jpp.swap(" + reference(c->getArg(0)) + ", " + reference(c->getArg(1)) + ")";
        if (n == "flush")
            return expr(c->getArg(0)) + ".flush()";
        if (n == "isdigit")
            return "Jpp.isdigit(" + expr(c->getArg(0)) + ")";
        if (n == "move" || n == "forward" || n == "as_const")
            return expr(c->getArg(0));
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
            return ctx.getAsArrayType(bare(a)->getType()) || isVec(a->getType())
                       ? "Jpp." + std::string(method) + "(" + expr(a) + ")"
                       : expr(a) + "." + method + "()";
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
            auto argument = scalarReference(target)
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
                        return "Collections.reverse(" + v + ")";
                    auto item = iteratorElement(c->getArg(0)->getType());
                    auto predicate = c->getNumArgs() > 2
                                         ? comparatorPredicate(c->getArg(2)->getType(), item, item,
                                                               expr(c->getArg(2)))
                                         : comparisonPredicate(item, item);
                    return v + ".sort(Jpp.comparator(" + predicate + "))";
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
            a.push_back(f && scalarReference(target) ? reference(e)
                                                     : value(e, target, f != nullptr));
        }
        return join(a);
    }
    std::string expr(const Expr* input) {
        if (!input)
            return "";
        auto e = strip(input);
        if (auto x = dyn_cast<IntegerLiteral>(e)) {
            return integerLiteral(x->getValue(), x->getType());
        }
        if (auto x = dyn_cast<FloatingLiteral>(e)) {
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
            return quote(x->getBytes().str());
        if (auto x = dyn_cast<CXXBoolLiteralExpr>(e))
            return x->getValue() ? "true" : "false";
        if (isa<CXXNullPtrLiteralExpr>(e) || isa<GNUNullExpr>(e))
            return "null";
        if (auto x = dyn_cast<DeclRefExpr>(e)) {
            if (aliases.count(x->getDecl()))
                return aliases[x->getDecl()];
            auto d = x->getDecl();
            auto n = d->getNameAsString();
            if (!user(d)) {
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
            if (x->getCastKind() == CK_IntegralToPointer)
                fail(e, "integer-derived pointer addresses need additional storage lowering");
            if (x->getCastKind() == CK_PointerToIntegral)
                fail(e, "converting pointer addresses to integers needs byte-addressed storage");
            if (x->getCastKind() == CK_BitCast && dataPointer(x->getType()) &&
                dataPointer(x->getSubExpr()->getType())) {
                auto from = clean(x->getSubExpr()->getType()->getPointeeType());
                auto to = clean(x->getType()->getPointeeType());
                if (!from->isVoidType() && !to->isVoidType() && !ctx.hasSameType(from, to))
                    fail(e, "pointer reinterpretation between element types needs byte storage");
            }
            if (x->getType()->isVoidType() && x->getSubExpr()->getType()->isVoidType())
                return expr(x->getSubExpr());
            if (x->getType()->isVoidType())
                return "Jpp.ignore(" + expr(x->getSubExpr()) + ")";
            if (x->getType()->isBooleanType())
                return truth(x->getSubExpr());
            if (x->getType()->isArithmeticType() || x->getType()->isEnumeralType())
                return value(x->getSubExpr(), x->getType());
            return "(" + type(x->getType()) + ")(" + expr(x->getSubExpr()) + ")";
        }
        if (auto x = dyn_cast<CompoundAssignOperator>(e))
            return compoundAssignment(x);
        if (auto x = dyn_cast<OpaqueValueExpr>(e)) {
            if (auto it = opaqueValues.find(x); it != opaqueValues.end())
                return it->second;
            fail(e, "an opaque GNU expression value has no binding");
        }
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
                if (dataPointer(a->getType())) {
                    if (clean(a->getType())->getPointeeType()->isVoidType())
                        fail(e, "GNU void-pointer arithmetic needs byte-addressed storage");
                    return "Jpp.updatePointer(" + reference(a) + ", " + std::to_string(delta) +
                           ", " + (x->isPostfix() ? "true" : "false") + ")";
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
                    (isVec(base->getType()) || isMap(base->getType()) ||
                     isString(base->getType()) ||
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
            if (!user(x->getMemberDecl())) {
                auto n = x->getMemberDecl()->getNameAsString();
                if (n == "first" || n == "second")
                    return base + "." + n;
                if (n == "npos")
                    return "-1L";
                fail(e, "unsupported library field " + n);
            }
            return base + "." + name(x->getMemberDecl());
        }
        if (auto x = dyn_cast<CXXOperatorCallExpr>(e)) {
            auto op = x->getOperator();
            if (op == OO_LessLess && x->getNumArgs() == 2 && stream(x->getArg(0)->getType()))
                return outputChain(x);
            if (op == OO_GreaterGreater && x->getNumArgs() == 2 && stream(x->getArg(0)->getType()))
                return inputExpression(x);
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
                    a.push_back(scalarReference(target) ? reference(e) : value(e, target, true));
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
            if (ctx.getAsArrayType(t))
                return newArray(t, x);
            if (family(t) == "array") {
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
                return "Jpp.vectorInitialized(" + number(templateArgs(t)[1].getAsIntegral()) +
                       ", () -> " + defaultValue(element(t)) + ", Arrays.<" + boxed(element(t)) +
                       ">asList(" + join(a) + "))";
            }
            if (isVec(t)) {
                for (auto v : x->inits())
                    a.push_back(value(v, element(t), true));
                return "new " + type(t) + "(Arrays.asList(" + join(a) + "))";
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
            if (x->getKind() != UETT_SizeOf && x->getKind() != UETT_AlignOf)
                fail(e, "unsupported type trait");
            auto t = x->getTypeOfArgument();
            if (t->isVariableArrayType()) {
                auto a = ctx.getAsVariableArrayType(t);
                return "((long)(" + expr(a->getSizeExpr()) + ") * " +
                       std::to_string(ctx.getTypeSizeInChars(a->getElementType()).getQuantity()) +
                       "L)";
            }
            return std::to_string(x->getKind() == UETT_SizeOf
                                      ? ctx.getTypeSizeInChars(t).getQuantity()
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
    std::string lambda(const LambdaExpr* e) {
        auto f = lambdaOperator(e->getLambdaClass());
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
        std::vector<std::string> captures;
        for (auto capture : e->captures())
            if (capture.capturesVariable() && capture.getCaptureKind() == LCK_ByCopy) {
                auto v = capture.getCapturedVar();
                auto snapshot = "jppCapture" + std::to_string(++serial);
                auto outer = aliases.count(v)
                                 ? aliases[v]
                                 : (cell(dyn_cast<VarDecl>(v)) ? name(v) + ".get()" : name(v));
                captures.push_back("final JppRef<" + boxed(v->getType()) + "> " + snapshot +
                                   " = JppRef.cell(jppCopy(" + outer + "));");
                aliases[v] = snapshot + ".get()";
            }
        // Render into the same stream, then extract the body for the surrounding expression.
        auto old = out.str();
        auto oldIndent = indent;
        auto oldCurrent = current;
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
        enclosingFunctions.resize(oldEnclosing);
        aliases = std::move(oldAliases);
        if (!captures.empty())
            return "new " + functionType(f->getType()->getAs<FunctionProtoType>()) + "() { " +
                   join(captures, " ") + " public " + type(f->getReturnType()) + " call(" +
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
        if (scalarReference(t))
            return "final JppRef<" + boxed(t) + "> " + name(v) + " = " + reference(v->getInit());
        if (auto d = dyn_cast<DecompositionDecl>(v))
            return decomposition(d, member);
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
    std::string decomposition(const DecompositionDecl* v, bool member) {
        if (member)
            fail(v->getLocation(), "global structured bindings are not supported");
        auto temp = "jppBinding" + std::to_string(++serial);
        auto t = v->getType();
        std::string s = type(t) + " " + temp + " = " + value(v->getInit(), t, true);
        unsigned i = 0;
        for (auto b : v->bindings()) {
            std::string get;
            if (family(t) == "pair")
                get = temp + (i == 0 ? ".first" : ".second");
            else if (ctx.getAsArrayType(t))
                get = temp + "[" + std::to_string(i) + "]";
            else if (auto r = record(t); r && user(r)) {
                auto fields = r->fields();
                auto it = fields.begin();
                std::advance(it, i);
                get = temp + "." + name(*it);
            } else
                fail(v->getLocation(),
                     "structured binding supports pairs, arrays, and user structs");
            s += "; " + type(b->getType()) + " " + name(b) + " = " + get;
            ++i;
        }
        return s;
    }
    std::string statementValue(const Expr* e) {
        auto s = expr(e);
        auto b = bare(e);
        bool assignment = false;
        if (auto bin = dyn_cast<BinaryOperator>(b))
            assignment = bin->isAssignmentOp();
        if (auto op = dyn_cast<CXXOperatorCallExpr>(b))
            assignment =
                op->getOperator() == OO_Equal || isCompoundAssignmentOperator(op->getOperator());
        if (assignment && s.starts_with('('))
            s = s.substr(1, s.size() - 2);
        return s;
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
        // Java only accepts calls, assignments, creation, and ++/-- as statements.
        if (isa<ConditionalOperator>(b) ||
            (isa<BinaryOperator>(b) && !cast<BinaryOperator>(b)->isAssignmentOp()))
            s = "Jpp.ignore(" + s + ")";
        line(s + ";");
        if (auto call = dyn_cast<CallExpr>(b);
            call && call->getDirectCallee() && call->getDirectCallee()->isNoReturn())
            line("throw new IllegalStateException(\"C++ noreturn function returned\");");
    }
    bool terminates(const Stmt* s) const {
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
        return false;
    }
    void bodyStatements(const CompoundStmt* body, bool returnValue = false) {
        struct Exit {
            const LabelDecl* label;
            size_t begin, end;
        };
        std::vector<const Stmt*> statements(body->body_begin(), body->body_end());
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
                    if (auto v = dyn_cast<VarDecl>(decl); v && fileStream(v->getType()) &&
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
                if (fileStream(v->getType()) && !v->getType()->isReferenceType() &&
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
                } else if (isa<TypedefNameDecl>(decl) || isa<UsingDecl>(decl) ||
                           isa<UsingDirectiveDecl>(decl) || isa<StaticAssertDecl>(decl) ||
                           isa<CXXRecordDecl>(decl) || isa<EnumDecl>(decl))
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
            } else
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
            line("while (" + truth(w->getCond()) + ")");
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
                    auto inc = f->getInc() ? statementValue(f->getInc()) : "";
                    line("for (" + variable(v) + "; " + (f->getCond() ? truth(f->getCond()) : "") +
                         "; " + inc + ")");
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
            line("for (; " + (f->getCond() ? truth(f->getCond()) : "") + "; " + join(increments) +
                 ")");
            block(f->getBody());
            --indent;
            line("}");
            return;
        }
        if (auto f = dyn_cast<CXXForRangeStmt>(s)) {
            auto v = f->getLoopVariable();
            auto range = f->getRangeInit();
            if (v->getType()->isReferenceType() && !isa<DecompositionDecl>(v) &&
                (isVec(range->getType()) || ctx.getAsArrayType(range->getType()) ||
                 isString(range->getType()))) {
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
                    for (auto c : body->body())
                        stmt(c);
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
                bool mapRef = isMap(range->getType()) && v->getType()->isReferenceType();
                auto mapName = "jppMap" + std::to_string(serial);
                if (mapRef) {
                    line("{");
                    ++indent;
                    line(type(range->getType()) + " " + mapName + " = " + expr(range) + ";");
                }
                line("for (" + type(d->getType()) + " " + temp + " : " +
                     (mapRef ? mapName : expr(range)) + ")");
                line("{");
                ++indent;
                if (!v->getType()->isReferenceType())
                    line(temp + " = jppCopy(" + temp + ");");
                unsigned k = 0;
                for (auto b : d->bindings()) {
                    if (mapRef && k == 1) {
                        line("final JppRef<" + boxed(b->getType()) + "> " + name(b) +
                             " = JppRef.at(" + mapName + ", " + temp + ".first);");
                        aliases[b] = name(b) + ".get()";
                    } else if (v->getType()->isReferenceType())
                        aliases[b] = temp + (k == 0 ? ".first" : ".second");
                    else
                        line(type(b->getType()) + " " + name(b) + " = " + temp +
                             (k == 0 ? ".first" : ".second") + ";");
                    ++k;
                }
                if (auto body = dyn_cast<CompoundStmt>(f->getBody()))
                    for (auto c : body->body())
                        stmt(c);
                else
                    stmt(f->getBody());
                --indent;
                line("}");
                if (mapRef) {
                    --indent;
                    line("}");
                }
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
                        for (auto c : body->body())
                            stmt(c);
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
            m && m->isCopyAssignmentOperator() && (m->isImplicit() || m->isDefaulted()))
            return;
        if (isa<CXXDestructorDecl>(f))
            fail(f->getLocation(),
                 "user-defined destructors require deterministic lifetime support");
        if (auto c = dyn_cast<CXXConstructorDecl>(f);
            c && c->isCopyOrMoveConstructor() && (c->isImplicit() || c->isDefaulted()))
            return;
        current = f;
        source(f);
        std::vector<std::string> params;
        for (auto p : f->parameters()) {
            auto t = p->getType();
            params.push_back(parameterType(t) + " " +
                             (cells.count(p) && !references.count(p) ? "jppParam_" : "") + name(p));
        }
        if (auto ctor = dyn_cast<CXXConstructorDecl>(f)) {
            if (ctor->isMoveConstructor())
                params.push_back("boolean " + temporary("jppMove"));
            line(name(ctor->getParent()) + "(" + join(params) + ")");
            line("{");
            ++indent;
            for (auto p : f->parameters())
                if (cells.count(p) && !references.count(p))
                    line("final JppRef<" + boxed(p->getType()) + "> " + name(p) +
                         " = JppRef.cell(jppParam_" + name(p) + ");");
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
            line(std::string(stat ? "static " : "") + type(f->getReturnType()) + " " + name(f) +
                 "(" + join(params) + ")");
            line("{");
            ++indent;
            for (auto p : f->parameters())
                if (cells.count(p) && !references.count(p))
                    line("final JppRef<" + boxed(p->getType()) + "> " + name(p) +
                         " = JppRef.cell(jppParam_" + name(p) + ");");
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
    }
    void emitRecord(const CXXRecordDecl* r) {
        if (r->isUnion())
            fail(r->getLocation(), "unions require a storage layout backend");
        if (r->getNumBases())
            fail(r->getLocation(), "class inheritance is not supported yet");
        source(r);
        currentRecord = r;
        bool less = false;
        const CXXMethodDecl* lt = nullptr;
        for (auto m : r->methods())
            if (m->isOverloadedOperator() && m->getOverloadedOperator() == OO_Less) {
                less = true;
                lt = m;
            }
        auto n = name(r);
        line("static class " + n + " implements JppAssignable<" + n + ">" +
             (less ? ", Comparable<" + n + ">" : ""));
        line("{");
        ++indent;
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
            line(n + "() {");
            ++indent;
            for (auto d : defaults)
                line(d);
            --indent;
            line("}");
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
        bool emittedDefault = r->isAggregate();
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
                if (constructor->getNumParams() == 0 && constructor->hasBody())
                    emittedDefault = true;
            }
        }
        if (!emittedDefault && !r->hasUserDeclaredConstructor()) {
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
                    scalarReference(parameter->getType())
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
        --indent;
        line("}");
        line();
        currentRecord = nullptr;
    }

  public:
    Emitter(ASTContext& c, const Options& o) : ctx(c), options(o), sm(c.getSourceManager()) {}
    std::string generate() {
        collect(ctx.getTranslationUnitDecl());
        for (auto f : functions) {
            for (auto p : f->parameters()) {
                reservedNames.insert(identifier(p->getNameAsString()));
                if (scalarReference(p->getType()))
                    references.insert(p);
            }
            scan(f->getBody());
        }
        for (auto v : globals)
            scan(v->getInit());
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
                if (f->getNumParams())
                    fail(f->getLocation(), "main(argc, argv) is not supported yet; use main()");
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
                    if (scalarReference(t))
                        value = "JppRef.cell(" + value + ")";
                    else if (copyType(t))
                        value = "jppCopy(" + value + ")";
                    bridge.push_back(value);
                }
            line("interface " + n + extends + " { " + type(f->getReturnType()) + " call(" +
                 join(p) + ");" +
                 (pred ? " default boolean test(" + boxed(f->getParamType(0)) + " a, " +
                             boxed(f->getParamType(1)) + " b) { return " +
                             booleanValue("call(" + join(bridge) + ")", f->getReturnType()) + "; }"
                       : "") +
                 " }");
        }
        if (main) {
            line("public static void main(String[] args) {");
            ++indent;
            line("int status;");
            line("try { status = " + name(main) + "(); }");
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
        program.insert(startupAt, pointerFields);
        std::string startup;
        auto support = supportFor(program, startup);
        program.insert(startupAt, startup);
        return formatJava(program + support + "}\n");
    }
};

class Consumer final : public ASTConsumer {
    const Options& opt;
    std::string& output;
    bool& success;

  public:
    Consumer(const Options& o, std::string& s, bool& b) : opt(o), output(s), success(b) {}
    void HandleTranslationUnit(ASTContext& ctx) override {
        if (ctx.getDiagnostics().hasErrorOccurred())
            return;
        try {
            output = Emitter(ctx, opt).generate();
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
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance&, llvm::StringRef) override {
        return std::make_unique<Consumer>(opt, output, success);
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
    std::vector<std::string> args = {"-std=gnu++23", "-fsyntax-only", "-Wno-unused-value",
                                     "-DONLINE_JUDGE"};
    args.insert(args.end(), opt.compilerArgs.begin(), opt.compilerArgs.end());
    tooling::FixedCompilationDatabase db(std::filesystem::current_path().string(), args);
    tooling::ClangTool tool(db, {opt.input});
    bool success = false;
    Factory factory(opt, java, success);
    return tool.run(&factory) == 0 && success;
}
} // namespace jpp
