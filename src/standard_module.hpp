#pragma once

#include "transpiler.hpp"
#include <functional>

namespace clang {
class DiagnosticConsumer;
}

namespace jpp {
using TranslationAttempt =
    std::function<bool(const std::vector<std::string>&, clang::DiagnosticConsumer*)>;

bool withStandardModule(const Options&, const std::vector<std::string>&, const TranslationAttempt&);
} // namespace jpp
