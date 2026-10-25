#include "transpiler.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
namespace {
struct TempDir {
    fs::path path;
    TempDir() {
        auto pattern = (fs::temp_directory_path() / "jpp-XXXXXX").string();
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back(0);
        if (!mkdtemp(buffer.data()))
            throw std::runtime_error("cannot create temporary directory");
        path = buffer.data();
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

int run(const std::vector<std::string>& args) {
    auto pid = fork();
    if (pid < 0)
        throw std::runtime_error("fork failed");
    if (!pid) {
        std::vector<char*> raw;
        for (auto& a : args)
            raw.push_back(const_cast<char*>(a.c_str()));
        raw.push_back(nullptr);
        execvp(raw[0], raw.data());
        std::cerr << "j++: cannot execute " << args[0] << ": " << std::strerror(errno) << '\n';
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0)
        if (errno != EINTR)
            throw std::runtime_error("waitpid failed");
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

std::string envTool(const char* variable, const char* fallback) {
    const char* value = std::getenv(variable);
    return value && *value ? value : fallback;
}

void write(const fs::path& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary);
    if (!out || !(out << data) || !out.flush())
        throw std::runtime_error("cannot write " + path.string());
}

void publish(const fs::path& target, const std::string& java) {
    // The temporary file lives on the destination filesystem, making rename atomic.
    auto pattern = target.string() + ".tmp.XXXXXX";
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back(0);
    int fd = mkstemp(name.data());
    if (fd < 0)
        throw std::runtime_error("cannot write output " + target.string() + ": " +
                                 std::strerror(errno));
    close(fd);
    try {
        write(name.data(), java);
        fs::rename(name.data(), target);
    } catch (...) {
        fs::remove(name.data());
        throw;
    }
}

void usage() {
    std::cout << R"(J++ 0.1.0 — C++ to readable, standalone Java

Usage: j++ <source.cpp> [options] [-- program-arguments...]

  -t, --transpile     Generate one Java source file (the default)
  -r, --run           Transpile, compile, and run; forward stdin/stdout/exit status
  -v j21|j17|j8       Target Java release (default: j21)
  -o <file.java>      Output path (default: Main.java beside the input)
  --class <name>     Java class name (default: output filename, or Main)
  --check            Compile generated Java to validate it without running
  -I <dir>, -D <def> Pass an include path or macro definition to Clang
  --cpp-arg <arg>    Pass another C++ frontend argument (repeatable)
  --no-source-comments  Omit C++ line references in generated Java
  --version          Print the version
  -h, --help         Show this help

GNU++23 is the input dialect. Competitive programming is the current focus;
unsupported constructs produce a source-located error, with no partial output.
JPP_JAVA and JPP_JAVAC can select Java executables. --check/--run require
a JDK supporting the selected --release. Transpiling alone needs no Java.
)";
}
} // namespace

int main(int argc, char** argv) {
    try {
        jpp::Options opt;
        fs::path output;
        bool execute = false, check = false, explicitClass = false;
        std::vector<std::string> programArgs;
        auto value = [&](int& i) -> std::string {
            if (++i >= argc)
                throw std::runtime_error(std::string("missing value for ") + argv[i - 1]);
            return argv[i];
        };
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "-h" || a == "--help") {
                usage();
                return 0;
            }
            if (a == "--version") {
                std::cout << "j++ 0.1.0\n";
                return 0;
            }
            if (a == "-t" || a == "--transpile")
                continue;
            if (a == "-r" || a == "--run") {
                execute = check = true;
                continue;
            }
            if (a == "--check") {
                check = true;
                continue;
            }
            if (a == "--no-source-comments") {
                opt.sourceComments = false;
                continue;
            }
            if (a == "-o") {
                output = value(i);
                continue;
            }
            if (a == "--class") {
                opt.className = value(i);
                explicitClass = true;
                continue;
            }
            if (a == "-v") {
                auto v = value(i);
                if (v == "j8")
                    opt.javaVersion = 8;
                else if (v == "j17")
                    opt.javaVersion = 17;
                else if (v == "j21")
                    opt.javaVersion = 21;
                else
                    throw std::runtime_error("invalid Java target '" + v +
                                             "'; use j8, j17, or j21");
                continue;
            }
            if (a == "--cpp-arg") {
                opt.compilerArgs.push_back(value(i));
                continue;
            }
            if (a == "-I" || a == "-D" || a == "-U") {
                opt.compilerArgs.push_back(a + value(i));
                continue;
            }
            if (a.starts_with("-I") || a.starts_with("-D") || a.starts_with("-U")) {
                opt.compilerArgs.push_back(a);
                continue;
            }
            if (a == "--") {
                while (++i < argc)
                    programArgs.emplace_back(argv[i]);
                break;
            }
            if (a.starts_with('-'))
                throw std::runtime_error("unknown option '" + a + "'");
            if (!opt.input.empty())
                throw std::runtime_error("expected one C++ input file");
            opt.input = a;
        }
        if (opt.input.empty()) {
            usage();
            return argc == 1 ? 0 : 2;
        }
        if (!fs::is_regular_file(opt.input))
            throw std::runtime_error("input is not a file: " + opt.input);
        opt.input = fs::absolute(opt.input).lexically_normal().string();
        if (output.empty())
            output = fs::path(opt.input).parent_path() / (opt.className + ".java");
        else if (!explicitClass)
            opt.className = output.stem().string();
        if (!jpp::javaIdentifier(opt.className))
            throw std::runtime_error("invalid Java class name: " + opt.className);
        if (output.extension() != ".java" || output.stem() != opt.className)
            throw std::runtime_error("output filename must be " + opt.className + ".java");
        output = fs::absolute(output).lexically_normal();
        if (output == fs::path(opt.input))
            throw std::runtime_error("output would overwrite the input");
        if (!programArgs.empty() && !execute)
            throw std::runtime_error("program arguments require -r");

        std::string java;
        if (!jpp::transpile(opt, java))
            return 1;
        if (check) {
            TempDir temp;
            auto source = temp.path / (opt.className + ".java");
            write(source, java);
            int result =
                run({envTool("JPP_JAVAC", "javac"), "-encoding", "UTF-8", "--release",
                     std::to_string(opt.javaVersion), "-d", temp.path.string(), source.string()});
            if (result) {
                std::cerr << "j++: Java validation failed; output was not replaced\n";
                return result;
            }
            publish(output, java);
            if (execute) {
                std::vector<std::string> args = {envTool("JPP_JAVA", "java"), "-cp",
                                                 temp.path.string(), opt.className};
                args.insert(args.end(), programArgs.begin(), programArgs.end());
                return run(args);
            }
        } else
            publish(output, java);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "j++: " << e.what() << '\n';
        return 2;
    }
}
