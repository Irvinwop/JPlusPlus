#pragma once
#include <string>
#include <vector>

namespace jpp {
struct Options {
    std::string input;
    std::string className = "Main";
    std::vector<std::string> compilerArgs;
    int javaVersion = 21;
    bool sourceComments = true;
};
// Returns false for invalid C++ or an unsupported construct. No partial Java is returned.
bool transpile(const Options&, std::string& java);
bool javaIdentifier(const std::string&);
std::string identifier(const std::string&);
} // namespace jpp
