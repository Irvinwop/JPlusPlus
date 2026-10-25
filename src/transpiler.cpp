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

std::string supportFor(const std::string& program) {
    struct Part {
        std::string code;
        std::set<std::string> defines;
        bool method = false, selected = false;
        bool input = false;
    };
    std::vector<Part> parts;
    for (auto code : declarations(javaRuntime)) {
        if (code.find("static class JppInput {") != std::string::npos) {
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
                    parts.push_back(
                        {decl, {"call:" + decl.substr(begin, end - begin)}, false, false, true});
                } else
                    fields += decl;
            }
            parts.push_back({fields, {"JppInput"}, false, false, true});
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
                changed = true;
            }
    } while (changed);
    std::string result =
        "\n    // Helpers used by this program. Everything stays in this source file.\n";
    for (auto& p : parts)
        if (p.selected && !p.method && !p.input)
            result += p.code + '\n';
    if (needed.count("JppInput")) {
        result += "\n    static class JppInput {\n";
        for (auto& p : parts)
            if (p.selected && p.input)
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
    std::unordered_set<const VarDecl*> cells;
    std::unordered_set<const VarDecl*> references;
    std::unordered_map<const FunctionProtoType*, std::string> functionTypes;
    std::vector<std::pair<const FunctionProtoType*, std::string>> interfaces;
    const FunctionDecl* current = nullptr;
    const CXXRecordDecl* currentRecord = nullptr;

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
        if (auto f = dyn_cast<FunctionDecl>(d)) {
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
        if (auto v = dyn_cast<VarDecl>(d); v && v->isLocalVarDeclOrParm() && current) {
            int count = ++localNames[current][base];
            if (count > 1)
                base += "_" + std::to_string(count);
        }
        // Member/local scopes retain natural names. Java overload erasure is avoided for functions.
        if (isa<FunctionDecl>(d) || isa<CXXRecordDecl>(d) ||
            (isa<VarDecl>(d) && cast<VarDecl>(d)->isFileVarDecl())) {
            int n = ++usedNames[base];
            if (n > 1)
                base += "_" + std::to_string(n);
        }
        return names[d] = base;
    }
    static QualType clean(QualType t) {
        return t.getNonReferenceType().getUnqualifiedType().getCanonicalType();
    }
    const CXXRecordDecl* record(QualType t) const {
        return clean(t)->getAsCXXRecordDecl();
    }
    std::string family(QualType t) const {
        auto r = record(t);
        return r && !user(r) ? r->getNameAsString() : "";
    }
    bool isString(QualType t) const {
        return family(t) == "basic_string" || family(t) == "basic_string_view";
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
        if (t->isBooleanType())
            return "boolean";
        if (t->isCharType() || t->isWideCharType() || t->isChar16Type())
            return "char";
        if (t->isEnumeralType())
            return "int";
        if (big(t))
            return "BigInteger";
        if (t->isIntegerType())
            return ctx.getTypeSize(t) > 32 ? "long" : "int";
        if (t->isRealFloatingType()) {
            if (t->isSpecificBuiltinType(BuiltinType::LongDouble))
                fail(SourceLocation(),
                     "long double has no exact Java equivalent; use double explicitly");
            return ctx.getTypeSize(t) <= 32 ? "float" : "double";
        }
        if (auto a = ctx.getAsArrayType(t))
            return type(a->getElementType()) + "[]";
        if (t->isPointerType()) {
            auto p = t->getPointeeType();
            if (p->isFunctionType())
                return functionType(p->getAs<FunctionProtoType>());
            return type(p) + "[]";
        }
        auto n = family(t);
        if (auto r = record(t); r && r->isLambda())
            return functionType(r->getLambdaCallOperator()->getType()->getAs<FunctionProtoType>());
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
        if (isIter(t)) {
            QualType v = element(t);
            if (v->isPointerType())
                v = v->getPointeeType();
            if (family(t) == "reverse_iterator") {
                std::string inner = type(v);
                return inner;
            }
            return "JppIter<" + boxed(v) + ">";
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
    std::string defaultValue(QualType t) {
        if (t->isReferenceType())
            t = t.getNonReferenceType();
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
        if (t->isPointerType() || family(t) == "function")
            return "null";
        if (isMap(t))
            return "new " + jt + "(" + (family(t) == "map" ? "true" : "false") + ", () -> " +
                   defaultValue(element(t, 1)) + ")";
        if (isSet(t))
            return "new " + jt + "(" +
                   ((family(t) == "multiset" || family(t) == "unordered_multiset") ? "true"
                                                                                   : "false") +
                   ")";
        if (family(t) == "priority_queue") {
            auto args = templateArgs(t);
            std::string comparator = "(a, b) -> Jpp.compare(b, a)";
            if (args.size() > 2) {
                auto c = args[2].getAsType();
                auto n = family(c);
                if (n == "greater")
                    comparator = "Jpp::compare";
                else if (n != "less")
                    comparator = "Jpp.heapComparator((a, b) -> new " + type(c) + "().call(a, b))";
            }
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
    std::string value(const Expr* e, QualType target, bool copy = false) {
        std::string s = expr(e);
        if (big(target) && !big(e->getType()))
            s = "BigInteger.valueOf(" + s + ")";
        else if (!big(target) && big(e->getType()))
            s = "(" + s + ")." + (type(target) == "long" ? "longValue" : "intValue") + "()";
        else if (isString(target) && !isString(e->getType()))
            s = "new JppString(" + s + ")";
        else if (target->isBooleanType() && !e->getType()->isBooleanType())
            s = truth(e);
        else if (e->getType()->isBooleanType() && !target->isBooleanType())
            s = "(" + s + " ? 1 : 0)";
        else if (type(target) == "char" && type(e->getType()) != "char")
            s = "(char)(" + s + ")";
        else if (clean(target)->isArithmeticType() && clean(e->getType())->isArithmeticType() &&
                 type(target) != type(e->getType()))
            s = "(" + type(target) + ")(" + s + ")";
        if (copy && copyType(target) && e->isLValue())
            s = "jppCopy(" + s + ")";
        return s;
    }
    bool scalarReference(QualType t) const {
        return t->isReferenceType() && (clean(t)->isArithmeticType() || clean(t)->isEnumeralType());
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
    void markReference(const Expr* e) {
        if (auto v = variableRef(e); v && !v->hasGlobalStorage())
            cells.insert(v);
    }
    void scan(const Stmt* s) {
        if (!s)
            return;
        if (auto l = dyn_cast<LambdaExpr>(s))
            for (auto capture : l->captures())
                if (capture.capturesVariable())
                    if (auto v = dyn_cast<VarDecl>(capture.getCapturedVar()))
                        cells.insert(v);
        if (auto d = dyn_cast<DeclStmt>(s))
            for (auto decl : d->decls())
                if (auto v = dyn_cast<VarDecl>(decl); v && scalarReference(v->getType())) {
                    references.insert(v);
                    markReference(v->getInit());
                }
        if (auto c = dyn_cast<CallExpr>(s);
            c && c->getDirectCallee() &&
            (user(c->getDirectCallee()) || c->getDirectCallee()->getNameAsString() == "swap")) {
            auto f = c->getDirectCallee();
            unsigned offset = isa<CXXOperatorCallExpr>(c) && isa<CXXMethodDecl>(f) ? 1 : 0;
            for (unsigned i = 0; i < f->getNumParams() && i + offset < c->getNumArgs(); ++i)
                if (scalarReference(f->getParamDecl(i)->getType()))
                    markReference(c->getArg(i + offset));
        }
        for (auto child : s->children())
            scan(child);
    }
    std::string reference(const Expr* e) {
        if (auto d = dyn_cast<DeclRefExpr>(bare(e));
            d && aliases.count(d->getDecl()) && aliases[d->getDecl()].ends_with(".get()")) {
            auto s = aliases[d->getDecl()];
            return s.substr(0, s.size() - 6);
        }
        if (auto v = variableRef(e)) {
            if (cell(v)) {
                if (auto a = aliases.find(v); a != aliases.end() && a->second.ends_with(".get()"))
                    return a->second.substr(0, a->second.size() - 6);
                return name(v);
            }
            if (v->hasGlobalStorage())
                return "JppRef.of(() -> " + expr(e) + ", jppValue -> " + expr(e) + " = jppValue)";
        }
        auto [base, index] = subscript(e);
        if (base)
            return "JppRef.at(" + expr(base) + ", " +
                   (isMap(base->getType()) ? expr(index) : "(int)(" + expr(index) + ")") + ")";
        if (auto m = dyn_cast<MemberExpr>(bare(e)); m && !m->isArrow())
            return "JppRef.member(" + expr(m->getBase()) + ", o -> o." + name(m->getMemberDecl()) +
                   ", (o, v) -> o." + name(m->getMemberDecl()) + " = v)";
        if (e->isLValue())
            fail(e, "this reference binding is not supported");
        return "JppRef.cell(" + expr(e) + ")";
    }
    std::string truth(const Expr* e) {
        e = strip(e);
        if (auto cast = dyn_cast<ImplicitCastExpr>(e)) {
            if (cast->getCastKind() == CK_IntegralToBoolean ||
                cast->getCastKind() == CK_FloatingToBoolean ||
                cast->getCastKind() == CK_PointerToBoolean)
                return truth(cast->getSubExpr());
        }
        auto t = clean(e->getType());
        auto s = expr(e);
        if (t->isBooleanType())
            return s;
        if (big(t))
            return "(" + s + ".signum() != 0)";
        if (t->isArithmeticType() || t->isEnumeralType())
            return "((" + s + ") != 0)";
        if (s.starts_with("Jpp.input("))
            return s;
        fail(e, "condition cannot be represented as a Java boolean");
    }
    std::string newArray(QualType t, const Expr* init) {
        auto a = ctx.getAsArrayType(t);
        if (!a)
            return defaultValue(t);
        auto elem = a->getElementType();
        if (init) {
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
                size = expr(v->getSizeExpr());
            else
                fail(SourceLocation(), "array bound is not known");
            dims += "[(int)(" + size + ")]";
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
        if (part->isRecordType())
            return "Jpp.objects(" + allocation + ", () -> " + defaultValue(part) + ")";
        return allocation;
    }
    bool stream(QualType t) const {
        auto n = family(t);
        return n.find("istream") != std::string::npos || n.find("ostream") != std::string::npos ||
               n.find("basic_ios") != std::string::npos;
    }
    std::string inputValue(QualType t) {
        auto jt = type(t);
        if (jt == "int")
            return "jppIn.nextInt()";
        if (jt == "long")
            return "jppIn.nextLong()";
        if (jt == "char")
            return "jppIn.nextChar()";
        if (jt == "double")
            return "jppIn.nextDouble()";
        if (jt == "float")
            return "(float)jppIn.nextDouble()";
        if (jt == "boolean")
            return "(jppIn.nextInt() != 0)";
        if (isString(t))
            return "jppIn.nextString()";
        if (big(t))
            return "new BigInteger(jppIn.nextString().toString())";
        fail(SourceLocation(), "unsupported cin extraction type " + t.getAsString());
    }
    void inputChain(const Expr* e, std::vector<std::string>& parts) {
        e = bare(e);
        if (auto op = dyn_cast<CXXOperatorCallExpr>(e);
            op && op->getOperator() == OO_GreaterGreater && op->getNumArgs() == 2 &&
            stream(op->getArg(0)->getType())) {
            inputChain(op->getArg(0), parts);
            parts.push_back(assign(op->getArg(1), inputValue(op->getArg(1)->getType())));
        }
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
                return n == "fixed"
                           ? left + ".fixed(true)"
                           : (fail(right, "scientific stream formatting is not supported"), "");
            if (n == "defaultfloat")
                return left + ".fixed(false)";
            if (n == "boolalpha" || n == "noboolalpha")
                return left + ".boolalpha(" + (n == "boolalpha" ? "true" : "false") + ")";
        }
        if (auto call = dyn_cast<CallExpr>(right);
            call && call->getDirectCallee() &&
            call->getDirectCallee()->getNameAsString() == "setprecision")
            return left + ".precision((int)(" + expr(call->getArg(0)) + "))";
        auto s = expr(op->getArg(1));
        auto t = op->getArg(1)->getType();
        if (unsignedLong(t))
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
        if (auto d = dyn_cast<DeclRefExpr>(bare(lhs));
            d && aliases.count(d->getDecl()) && aliases[d->getDecl()].ends_with(".get()"))
            return reference(lhs) + ".set(" + rhs + ")";
        if (auto v = variableRef(lhs); cell(v))
            return reference(lhs) + ".set(" + rhs + ")";
        auto [base, index] = subscript(lhs);
        if (base && isVec(base->getType()))
            return "Jpp.set(" + expr(base) + ", (int)(" + expr(index) + "), " + rhs + ")";
        if (base && (isMap(base->getType()) || isString(base->getType())))
            return expr(base) + ".put(" +
                   (isMap(base->getType()) ? expr(index) : "(int)(" + expr(index) + ")") + ", " +
                   rhs + ")";
        if (auto u = dyn_cast<UnaryOperator>(bare(lhs));
            u && u->getOpcode() == UO_Deref && isIter(u->getSubExpr()->getType()))
            return expr(u->getSubExpr()) + ".put(" + rhs + ")";
        return expr(lhs) + " = " + rhs;
    }
    std::string binary(const Expr* lhs, const Expr* rhs, std::string op, QualType result) {
        if (op == "=")
            return assign(lhs, value(rhs, lhs->getType(), true));
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
                return assign(lhs, v);
            }
            return expr(lhs) + " " + op + " " + expr(rhs);
        }
        if (op == ",")
            return "Jpp.last(" + expr(lhs) + ", " + expr(rhs) + ")";
        if (op == "&&" || op == "||")
            return "(" + truth(lhs) + " " + op + " " + truth(rhs) + ")";
        bool comparison =
            op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=";
        if (isIter(lhs->getType()) || isIter(rhs->getType())) {
            if (op == "==" || op == "!=")
                return (op == "!=" ? "!" : "") + expr(lhs) + ".same(" + expr(rhs) + ")";
            if (op == "-" && isIter(rhs->getType()))
                return type(result) == "long" ? "(long)(" + expr(lhs) + ".minus(" + expr(rhs) + "))"
                                              : expr(lhs) + ".minus(" + expr(rhs) + ")";
            if (op == "+" || op == "-")
                return expr(lhs) + ".plus((int)(" + (op == "-" ? "-" : "") + expr(rhs) + "))";
            if (comparison)
                return "(" + expr(lhs) + ".minus(" + expr(rhs) + ") " + op + " 0)";
        }
        if (lhs->getType()->isPointerType() && (op == "+" || op == "-") &&
            rhs->getType()->isIntegerType())
            return asIterator(lhs) + ".plus((int)(" + (op == "-" ? "-" : "") + expr(rhs) + "))";
        if (big(lhs->getType()) || big(rhs->getType())) {
            auto l = big(lhs->getType()) ? expr(lhs) : "BigInteger.valueOf(" + expr(lhs) + ")";
            auto r = big(rhs->getType()) ? expr(rhs) : "BigInteger.valueOf(" + expr(rhs) + ")";
            if (comparison)
                return "(" + l + ".compareTo(" + r + ") " + op + " 0)";
            static const std::map<std::string, std::string> ops = {
                {"+", "add"},        {"-", "subtract"},   {"*", "multiply"}, {"/", "divide"},
                {"%", "remainder"},  {"&", "and"},        {"|", "or"},       {"^", "xor"},
                {"<<", "shiftLeft"}, {">>", "shiftRight"}};
            if (auto it = ops.find(op); it != ops.end())
                return l + "." + it->second + "(" +
                       ((op == "<<" || op == ">>") ? "(int)(" + expr(rhs) + ")" : r) + ")";
        }
        if (isString(lhs->getType()) || isString(rhs->getType())) {
            if (op == "+")
                return "Jpp.concat(" + expr(lhs) + ", " + expr(rhs) + ")";
            if (comparison)
                return "(" + expr(lhs) + ".toString().compareTo(" + expr(rhs) + ".toString()) " +
                       op + " 0)";
        }
        if (comparison && (lhs->getType()->isRecordType() || rhs->getType()->isRecordType()))
            return "(Jpp.compare(" + expr(lhs) + ", " + expr(rhs) + ") " + op + " 0)";
        if (comparison && clean(lhs->getType())->isUnsignedIntegerType() &&
            (ctx.getTypeSize(lhs->getType()) == 32 || ctx.getTypeSize(lhs->getType()) == 64))
            return "(" + std::string(unsignedLong(lhs->getType()) ? "Long" : "Integer") +
                   ".compareUnsigned(" + expr(lhs) + ", " + expr(rhs) + ") " + op + " 0)";
        if ((op == "/" || op == "%") && clean(lhs->getType())->isUnsignedIntegerType())
            return std::string(unsignedLong(lhs->getType()) ? "Long" : "Integer") + "." +
                   (op == "/" ? "divideUnsigned" : "remainderUnsigned") + "(" + expr(lhs) + ", " +
                   expr(rhs) + ")";
        if (op == ">>" && clean(lhs->getType())->isUnsignedIntegerType())
            op = ">>>";
        return "(" + expr(lhs) + " " + op + " " + expr(rhs) + ")";
    }
    std::string construct(const CXXConstructExpr* c) {
        auto t = c->getType();
        std::vector<const Expr*> args;
        for (auto a : c->arguments())
            if (!isa<CXXDefaultArgExpr>(a))
                args.push_back(a);
        if (c->getConstructor()->isCopyOrMoveConstructor() && !args.empty())
            return "jppCopy(" + expr(args[0]) + ")";
        if (args.empty())
            return defaultValue(t);
        auto n = family(t);
        if (isString(t)) {
            std::vector<std::string> a;
            for (auto e : args)
                a.push_back(expr(e));
            if (args.size() == 2)
                a[0] = "(int)(" + a[0] + ")";
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
                return "Jpp.vector((int)(" + expr(args[0]) + "), " +
                       value(args[1], element(t), false) + ")";
            return "Jpp.vectorDefault((int)(" + expr(args[0]) + "), () -> " +
                   defaultValue(element(t)) + ")";
        }
        if (n == "pair") {
            if (args.size() == 1 && family(args[0]->getType()) == "pair")
                return "jppCopy(" + expr(args[0]) + ")";
            std::vector<std::string> a;
            for (unsigned i = 0; i < args.size(); ++i)
                a.push_back(value(args[i], element(t, i), true));
            return "new " + type(t) + "(" + join(a) + ")";
        }
        if (n == "function")
            return expr(args[0]);
        if (isIter(t))
            return expr(args[0]);
        if (isSet(t))
            fail(c, "non-default set constructors are not supported yet");
        if (n == "greater" || n == "less")
            return "(a, b) -> Jpp.compare(a, b) " + std::string(n == "less" ? "<" : ">") + " 0";
        std::vector<std::string> a;
        for (unsigned i = 0; i < args.size(); ++i) {
            QualType target = i < c->getConstructor()->getNumParams()
                                  ? c->getConstructor()->getParamDecl(i)->getType()
                                  : args[i]->getType();
            a.push_back(value(args[i], target, true));
        }
        return "new " + type(t) + "(" + join(a) + ")";
    }
    std::string asIterator(const Expr* e) {
        e = strip(e);
        auto b = bare(e);
        if (ctx.getAsArrayType(b->getType()))
            return "Jpp.begin(" + expr(b) + ")";
        if (e->getType()->isPointerType() && !isIter(b->getType()) && isa<DeclRefExpr>(b))
            return "Jpp.begin(" + expr(b) + ")";
        return expr(e);
    }
    std::string call(const CallExpr* c) {
        auto f = c->getDirectCallee();
        if (auto m = dyn_cast<CXXMemberCallExpr>(c)) {
            auto me = dyn_cast<MemberExpr>(bare(m->getCallee()));
            if (!me)
                fail(c, "unsupported member call");
            auto obj = me->getBase();
            auto ot = obj->getType();
            if (ot->isPointerType())
                ot = ot->getPointeeType();
            auto n = me->getMemberDecl()->getNameAsString();
            if (n == "sync_with_stdio" || (n == "tie" && stream(ot)))
                return "Jpp.ignore()";
            if (stream(ot)) {
                if (n == "operator bool")
                    return "jppIn.good";
                if (n == "good" || n == "eof")
                    return n == "good" ? "jppIn.good" : "!jppIn.good";
                if (n == "flush")
                    return expr(obj) + ".flush()";
                fail(c, "unsupported stream operation " + n);
            }
            std::vector<std::string> args;
            for (unsigned i = 0; i < c->getNumArgs(); ++i) {
                auto a = c->getArg(i);
                if (isa<CXXDefaultArgExpr>(a))
                    continue;
                QualType target =
                    f && i < f->getNumParams() ? f->getParamDecl(i)->getType() : a->getType();
                args.push_back(value(a, target, true));
            }
            auto o = expr(obj);
            if (n == "size" || n == "length" || n == "capacity")
                return integerResult(c, o + "." + n + "()");
            bool deque = family(ot) == "queue" || family(ot) == "deque" || family(ot) == "stack";
            bool heap = family(ot) == "priority_queue";
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
                    args[0] = "(int)(" + args[0] + ")";
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
                    return "Jpp.resize(" + o + ", (int)(" + expr(c->getArg(0)) + "), " +
                           value(c->getArg(1), element(ot), false) + ")";
                return "Jpp.resizeDefault(" + o + ", (int)(" + expr(c->getArg(0)) + "), () -> " +
                       defaultValue(element(ot)) + ")";
            }
            if (n == "reserve" && isVec(ot))
                return o + ".ensureCapacity((int)(" + args[0] + "))";
            if (n == "at" && (isVec(ot) || isString(ot)))
                return o + ".get((int)(" + args[0] + "))";
            if (n == "substr" || n == "find") {
                if (isString(ot)) {
                    for (size_t i = (n == "substr" ? 0 : 1); i < args.size(); ++i)
                        args[i] = "(int)(" + args[i] + ")";
                }
            }
            if (n == "c_str" && isString(ot))
                return o + ".toString()";
            if (n == "pop" && family(ot) == "stack")
                return o + ".pop_back()";
            if (user(me->getMemberDecl()))
                n = name(me->getMemberDecl());
            static const std::set<std::string> methods = {
                "size",        "length", "empty",  "clear",     "front",      "back",
                "top",         "push",   "pop",    "push_back", "push_front", "pop_back",
                "pop_front",   "begin",  "end",    "cbegin",    "cend",       "rbegin",
                "rend",        "erase",  "insert", "count",     "contains",   "lower_bound",
                "upper_bound", "assign", "substr", "find",      "append",     "at",
                "swap"};
            if (!user(me->getMemberDecl()) && !methods.count(n))
                fail(c, "unsupported library member '" + n + "'");
            if (n == "cbegin")
                n = "begin";
            if (n == "cend")
                n = "end";
            auto result = o + "." + n + "(" + join(args) + ")";
            return n == "count" ? integerResult(c, result) : result;
        }
        if (!f)
            return expr(c->getCallee()) + ".call(" + arguments(c) + ")";
        auto n = f->getNameAsString();
        if (user(f) && !f->getBuiltinID())
            return name(f) + "(" + arguments(c, f) + ")";
        if (n == "sync_with_stdio")
            return "Jpp.ignore()";
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
        if (n == "printf")
            return "jppOut.printf(" + arguments(c) + ")";
        if (n == "puts" || n == "putchar" || n == "getchar")
            return "Jpp." + n + "(" + arguments(c) + ")";
        if (n == "exit")
            return "Jpp.exit((int)(" + expr(c->getArg(0)) + "))";
        if (n == "_Exit")
            return "System.exit((int)(" + expr(c->getArg(0)) + "))";
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
        if (n == "stoi" || n == "stol" || n == "stoll" || n == "stod" || n == "stof")
            return std::string(n == "stoi"   ? "Integer.parseInt"
                               : n == "stod" ? "Double.parseDouble"
                               : n == "stof" ? "Float.parseFloat"
                                             : "Long.parseLong") +
                   "(" + expr(c->getArg(0)) + ".toString())";
        if (n == "getline")
            return assign(c->getArg(1), "jppIn.line()");
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
            return expr(c->getArg(0)) + ".plus((int)(" + std::string(n == "prev" ? "-" : "") +
                   (c->getNumArgs() > 1 && !isa<CXXDefaultArgExpr>(c->getArg(1))
                        ? expr(c->getArg(1))
                        : "1") +
                   "))";
        if (n == "advance")
            return expr(c->getArg(0)) + ".advance((int)(" + expr(c->getArg(1)) + "))";
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
            return "Math." + n + "(" + arguments(c) + ")";
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
                    return v + ".sort(" +
                           (c->getNumArgs() > 2 ? "Jpp.comparator(" + expr(c->getArg(2)) + ")"
                                                : "Jpp::compare") +
                           ")";
                }
            }
            std::vector<std::string> args;
            for (unsigned i = 0; i < c->getNumArgs(); ++i)
                if (!isa<CXXDefaultArgExpr>(c->getArg(i))) {
                    auto arg = c->getArg(i);
                    if (i < 2)
                        args.push_back(asIterator(arg));
                    else if (n == "fill") {
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
            auto result = "Jpp." + n + "(" + join(args) + ")";
            return n == "count" ? integerResult(c, result) : result;
        }
        if (n == "min" || n == "max" || n == "clamp" || n == "gcd" || n == "lcm")
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
            auto v = x->getValue();
            if (big(x->getType())) {
                llvm::SmallString<40> b;
                v.toString(b, 10, !x->getType()->isUnsignedIntegerType());
                return "new BigInteger(" + quote(b.str().str()) + ")";
            }
            if (ctx.getTypeSize(x->getType()) > 32)
                return std::to_string(v.getSExtValue()) + "L";
            return std::to_string(static_cast<int32_t>(v.getZExtValue()));
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
                if (n == "cerr" || n == "clog")
                    return "jppErr";
                if (n == "npos")
                    return "-1L";
                if (auto en = dyn_cast<EnumConstantDecl>(d))
                    return number(en->getInitVal());
                fail(e, "unsupported library value '" + d->getQualifiedNameAsString() + "'");
            }
            if (auto v = dyn_cast<VarDecl>(d); cell(v))
                return name(d) + ".get()";
            return name(d);
        }
        if (auto x = dyn_cast<ImplicitCastExpr>(e)) {
            auto sub = x->getSubExpr();
            switch (x->getCastKind()) {
            case CK_IntegralToBoolean:
            case CK_FloatingToBoolean:
            case CK_PointerToBoolean:
                return truth(sub);
            case CK_IntegralCast:
            case CK_IntegralToFloating:
            case CK_FloatingToIntegral:
            case CK_FloatingCast: {
                if (big(x->getType()) || big(sub->getType()))
                    return value(sub, x->getType());
                auto from = type(sub->getType()), to = type(x->getType());
                if (from == to)
                    return expr(sub);
                if (from == "boolean")
                    return "(" + expr(sub) + " ? 1 : 0)";
                return "(" + to + ")(" + expr(sub) + ")";
            }
            default:
                return expr(sub);
            }
        }
        if (auto x = dyn_cast<ExplicitCastExpr>(e)) {
            if (x->getType()->isVoidType())
                return "Jpp.ignore(" + expr(x->getSubExpr()) + ")";
            if (x->getType()->isBooleanType())
                return truth(x->getSubExpr());
            if (big(x->getType()) || big(x->getSubExpr()->getType()))
                return value(x->getSubExpr(), x->getType());
            return "(" + type(x->getType()) + ")(" + expr(x->getSubExpr()) + ")";
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
            if (x->getOpcode() == UO_Deref && isIter(a->getType()))
                return s + ".get()";
            if (x->getOpcode() == UO_AddrOf)
                fail(e, "address-taking requires a pointer backend; pass containers by reference "
                        "instead");
            if (x->isIncrementDecrementOp()) {
                int delta = x->isIncrementOp() ? 1 : -1;
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
                if (auto [base, index] = subscript(a);
                    base &&
                    (isVec(base->getType()) || isMap(base->getType()) || isString(base->getType())))
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
        if (auto x = dyn_cast<ConditionalOperator>(e))
            return "(" + truth(x->getCond()) + " ? " + expr(x->getTrueExpr()) + " : " +
                   expr(x->getFalseExpr()) + ")";
        if (auto x = dyn_cast<ArraySubscriptExpr>(e)) {
            auto index = x->getIdx()->getType()->isBooleanType()
                             ? "(" + expr(x->getIdx()) + " ? 1 : 0)"
                             : "(int)(" + expr(x->getIdx()) + ")";
            if (isa<StringLiteral>(bare(x->getBase())))
                return expr(x->getBase()) + ".charAt(" + index + ")";
            return expr(x->getBase()) + "[" + index + "]";
        }
        if (auto x = dyn_cast<MemberExpr>(e)) {
            if (isa<CXXThisExpr>(bare(x->getBase())))
                return "this." + name(x->getMemberDecl());
            auto base = expr(x->getBase());
            if (x->isArrow() && !isIter(x->getBase()->getType()))
                fail(e, "raw pointer member access is not supported");
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
            if (op == OO_GreaterGreater && x->getNumArgs() == 2 &&
                stream(x->getArg(0)->getType())) {
                std::vector<std::string> a;
                inputChain(x, a);
                return "Jpp.input(" + join(a) + ")";
            }
            if (op == OO_Subscript) {
                auto b = x->getArg(0);
                auto i = x->getArg(1);
                return expr(b) + ".get(" +
                       (isMap(b->getType()) ? expr(i) : "(int)(" + expr(i) + ")") + ")";
            }
            if (op == OO_Call) {
                auto object = x->getArg(0);
                std::vector<std::string> a;
                for (unsigned i = 1; i < x->getNumArgs(); ++i)
                    a.push_back(expr(x->getArg(i)));
                return expr(object) + ".call(" + join(a) + ")";
            }
            if (op == OO_PlusPlus || op == OO_MinusMinus) {
                auto a = x->getArg(0);
                auto delta = op == OO_PlusPlus ? "1" : "-1";
                if (isIter(a->getType()))
                    return expr(a) + (x->getNumArgs() > 1 ? ".post(" : ".advance(") + delta + ")";
            }
            if (x->getNumArgs() == 2) {
                if (auto f = x->getDirectCallee();
                    f && user(f) && isa<CXXMethodDecl>(f) && op != OO_Equal)
                    return expr(x->getArg(0)) + "." + name(f) + "(" + expr(x->getArg(1)) + ")";
                return binary(x->getArg(0), x->getArg(1), getOperatorSpelling(op), x->getType());
            }
            if (op == OO_Star && x->getNumArgs() == 1)
                return expr(x->getArg(0)) + ".get()";
            fail(e, "unsupported overloaded operator");
        }
        if (auto x = dyn_cast<CallExpr>(e))
            return call(x);
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
            if (isVec(t)) {
                for (auto v : x->inits())
                    a.push_back(value(v, element(t), true));
                return "new " + type(t) + "(Arrays.asList(" + join(a) + "))";
            }
            if (auto r = record(t)) {
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
            return "this";
        if (auto x = dyn_cast<LambdaExpr>(e))
            return lambda(x);
        if (auto x = dyn_cast<SubstNonTypeTemplateParmExpr>(e))
            return expr(x->getReplacement());
        fail(e, std::string("unsupported expression ") + e->getStmtClassName());
    }
    std::string lambda(const LambdaExpr* e) {
        auto f = e->getCallOperator();
        if (f->getDescribedFunctionTemplate())
            fail(e, "generic lambdas are not supported yet");
        std::vector<std::string> params;
        for (auto p : f->parameters()) {
            auto base = identifier(p->getNameAsString());
            if (std::any_of(names.begin(), names.end(),
                            [&](const auto& n) { return n.first != p && n.second == base; }))
                names[p] = base + "_lambda" + std::to_string(++serial);
            params.push_back(type(p->getType()) + " " + name(p));
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
        current = f;
        out.str("");
        out.clear();
        indent = 0;
        block(f->getBody());
        auto body = out.str();
        if (!body.empty() && body.back() == '\n')
            body.pop_back();
        out.str(old);
        out.clear();
        out.seekp(0, std::ios_base::end);
        indent = oldIndent;
        current = oldCurrent;
        aliases = std::move(oldAliases);
        if (!captures.empty())
            return "new " + functionType(f->getType()->getAs<FunctionProtoType>()) + "() { " +
                   join(captures, " ") + " public " + type(f->getReturnType()) + " call(" +
                   join(params) + ") " + body + " }";
        return "(" + functionType(f->getType()->getAs<FunctionProtoType>()) + ") ((" +
               join(params) + ") -> " + body + ")";
    }
    std::string variable(const VarDecl* v, bool member = false) {
        auto t = v->getType();
        if (scalarReference(t))
            return "final JppRef<" + boxed(t) + "> " + name(v) + " = " + reference(v->getInit());
        if (auto d = dyn_cast<DecompositionDecl>(v))
            return decomposition(d, member);
        std::string init;
        if (auto e = v->getInit()) {
            Expr::EvalResult evaluated;
            if (v->isConstexpr() && clean(t)->isIntegerType() && !big(t) &&
                e->EvaluateAsInt(evaluated, ctx))
                init = number(evaluated.Val.getInt()) + (type(t) == "long" ? "L" : "");
            else
                init = ctx.getAsArrayType(t) ? newArray(t, e) : value(e, t, true);
        } else
            init = defaultValue(t);
        if (cells.count(v)) {
            if (v->getInit() && (family(t) == "function" || (record(t) && record(t)->isLambda())))
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
    void expressionStatement(const Expr* e) {
        auto b = bare(e);
        if (auto op = dyn_cast<CXXOperatorCallExpr>(b);
            op && op->getOperator() == OO_GreaterGreater && op->getNumArgs() == 2 &&
            stream(op->getArg(0)->getType())) {
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
        auto s = expr(e);
        if (s == "Jpp.ignore()")
            return;
        if (auto bin = dyn_cast<BinaryOperator>(b); bin && bin->getOpcode() == BO_Assign) {
            auto [base, index] = subscript(bin->getLHS());
            if (base && isVec(base->getType()))
                s = expr(base) + ".set((int)(" + expr(index) + "), " +
                    value(bin->getRHS(), bin->getLHS()->getType(), true) + ")";
        }
        if (isa<BinaryOperator>(b) && cast<BinaryOperator>(b)->isAssignmentOp() &&
            s.starts_with('(')) {
            s = s.substr(1, s.size() - 2);
        }
        // Java only accepts calls, assignments, creation, and ++/-- as statements.
        if (isa<ConditionalOperator>(b) ||
            (isa<BinaryOperator>(b) && !cast<BinaryOperator>(b)->isAssignmentOp()))
            s = "Jpp.ignore(" + s + ")";
        line(s + ";");
    }
    bool terminates(const Stmt* s) const {
        if (!s)
            return false;
        if (isa<ReturnStmt>(s) || isa<CXXThrowExpr>(s) || isa<BreakStmt>(s) || isa<ContinueStmt>(s))
            return true;
        if (auto b = dyn_cast<CompoundStmt>(s))
            return !b->body_empty() && terminates(b->body_back());
        if (auto i = dyn_cast<IfStmt>(s))
            return i->getElse() && terminates(i->getThen()) && terminates(i->getElse());
        return false;
    }
    void block(const Stmt* s) {
        line("{");
        ++indent;
        if (auto b = dyn_cast_or_null<CompoundStmt>(s))
            for (auto child : b->body())
                stmt(child);
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
                if (auto v = dyn_cast<VarDecl>(decl))
                    line(variable(v) + ";");
                else if (isa<TypedefNameDecl>(decl) || isa<UsingDecl>(decl) ||
                         isa<UsingDirectiveDecl>(decl) || isa<StaticAssertDecl>(decl))
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
        if (auto r = dyn_cast<ReturnStmt>(s)) {
            if (!r->getRetValue())
                line("return;");
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
                    auto inc = f->getInc() ? expr(f->getInc()) : "";
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
                    increments.push_back(expr(e));
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
                line("for (" + type(v->getType()) + " " + name(v) + " : " + expr(range) + ")");
                if (copyType(v->getType())) {
                    line("{");
                    ++indent;
                    line(name(v) + " = jppCopy(" + name(v) + ");");
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
            if (sw->getInit())
                stmt(sw->getInit());
            line("switch ((int)(" + expr(sw->getCond()) + "))");
            block(sw->getBody());
            return;
        }
        if (auto c = dyn_cast<CaseStmt>(s)) {
            line("case " + expr(c->getLHS()) + ":");
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
                    if (f->hasBody() && collected.insert(f->getCanonicalDecl()).second)
                        functions.push_back(f);
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
                    collected.insert(f->getCanonicalDecl()).second)
                    functions.push_back(f);
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
        if (auto m = dyn_cast<CXXMethodDecl>(f); m && m->isCopyAssignmentOperator())
            return;
        if (isa<CXXDestructorDecl>(f))
            fail(f->getLocation(),
                 "user-defined destructors require deterministic lifetime support");
        if (auto c = dyn_cast<CXXConstructorDecl>(f); c && c->isCopyOrMoveConstructor())
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
            line(name(ctor->getParent()) + "(" + join(params) + ")");
            line("{");
            ++indent;
            for (auto init : ctor->inits()) {
                if (!init->isMemberInitializer())
                    fail(f->getLocation(),
                         "base and delegating constructors are not supported yet");
                line("this." + name(init->getMember()) + " = " +
                     value(init->getInit(), init->getMember()->getType(), true) + ";");
            }
            if (auto b = dyn_cast<CompoundStmt>(f->getBody()))
                for (auto s : b->body())
                    stmt(s);
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
                for (auto s : b->body())
                    stmt(s);
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
        line("static class " + n + " implements JppCopyable<" + n + ">" +
             (less ? ", Comparable<" + n + ">" : ""));
        line("{");
        ++indent;
        std::vector<std::string> params, assignments;
        for (auto field : r->fields()) {
            if (field->isBitField())
                fail(field->getLocation(), "bitfields require a storage layout backend");
            auto init = field->hasInClassInitializer()
                            ? value(field->getInClassInitializer(), field->getType(), true)
                            : defaultValue(field->getType());
            line(type(field->getType()) + " " + name(field) + " = " + init + ";");
            params.push_back(type(field->getType()) + " " + name(field));
            assignments.push_back("this." + name(field) + " = " + name(field) + ";");
        }
        if (r->isAggregate()) {
            line(n + "() {}");
            if (!params.empty()) {
                line(n + "(" + join(params) + ") {");
                ++indent;
                for (auto a : assignments)
                    line(a);
                --indent;
                line("}");
            }
        }
        line(n + "(" + n + " other) {");
        ++indent;
        for (auto field : r->fields())
            line("this." + name(field) + " = jppCopy(other." + name(field) + ");");
        --indent;
        line("}");
        line("public " + n + " copy() { return new " + n + "(this); }");
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
            for (auto p : f->parameters())
                if (scalarReference(p->getType()))
                    references.insert(p);
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
        bool main = false;
        for (auto f : functions)
            if (f->isMain()) {
                main = true;
                if (f->getNumParams())
                    fail(f->getLocation(), "main(argc, argv) is not supported yet; use main()");
            }
        if (!main)
            fail(SourceLocation(), "input needs a definition of main()");
        out << "// Generated by J++ 0.1.0. Target: Java " << options.javaVersion << ".\n"
            << "// Input dialect: GNU++23. Support: "
               "https://github.com/Irvinwop/JPlusPlus/blob/main/docs/compatibility.md\n"
            << "import java.io.*;\nimport java.util.*;\nimport java.math.BigInteger;\n\n"
            << "@SuppressWarnings({\"unchecked\", \"rawtypes\"})\npublic class "
            << options.className << " {\n";
        for (auto en : enums)
            for (auto v : en->enumerators())
                line("static final int " + name(v) + " = " + number(v->getInitVal()) + ";");
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
                p.push_back(type(t) + " arg" + std::to_string(j++));
            bool pred = f->getReturnType()->isBooleanType() && f->getNumParams() == 2;
            auto extends = pred ? " extends java.util.function.BiPredicate<" +
                                      boxed(f->getParamType(0)) + ", " + boxed(f->getParamType(1)) +
                                      ">"
                                : "";
            line("interface " + n + extends + " { " + type(f->getReturnType()) + " call(" +
                 join(p) + ");" +
                 (pred ? " default boolean test(" + boxed(f->getParamType(0)) + " a, " +
                             boxed(f->getParamType(1)) + " b) { return call(a, b); }"
                       : "") +
                 " }");
        }
        line("public static void main(String[] args) {");
        ++indent;
        line("int status;");
        line("try { status = cppMain(); }");
        line("finally { jppOut.flush(); jppErr.flush(); }");
        line("if (status != 0) System.exit(status);");
        --indent;
        line("}");
        line();
        auto program = out.str();
        return formatJava(program + supportFor(program) + "}\n");
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
    if (javaWords.count(r) || r.starts_with("jpp") || r.starts_with("Jpp"))
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
