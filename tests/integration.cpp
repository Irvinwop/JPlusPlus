#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <regex>
#include <set>
#include <signal.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
struct Temp {
    fs::path path;
    Temp() {
        char p[] = "/tmp/jpp-tests-XXXXXX";
        auto s = mkdtemp(p);
        if (!s)
            throw std::runtime_error("mkdtemp");
        path = s;
    }
    ~Temp() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};
std::string read(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}
void write(const fs::path& p, const std::string& s) {
    std::ofstream f(p);
    f << s;
    if (!f)
        throw std::runtime_error("write failed");
}

int process(const std::vector<std::string>& args, const fs::path& input, const fs::path& output,
            const fs::path& errors, const fs::path& directory = {}) {
    pid_t pid = fork();
    if (pid < 0)
        throw std::runtime_error("fork");
    if (!pid) {
        setpgid(0, 0);
        int in = open(input.c_str(), O_RDONLY),
            out = open(output.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600),
            err = open(errors.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (in < 0 || out < 0 || err < 0)
            _exit(125);
        if (!directory.empty() && chdir(directory.c_str()))
            _exit(125);
        dup2(in, 0);
        dup2(out, 1);
        dup2(err, 2);
        close(in);
        close(out);
        close(err);
        std::vector<char*> raw;
        for (auto& s : args)
            raw.push_back(const_cast<char*>(s.c_str()));
        raw.push_back(nullptr);
        execvp(raw[0], raw.data());
        _exit(127);
    }
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(45);
    int status;
    while (waitpid(pid, &status, WNOHANG) == 0) {
        if (std::chrono::steady_clock::now() > until) {
            kill(-pid, SIGKILL);
            waitpid(pid, &status, 0);
            return 124;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

std::vector<std::string> configured(const char* variable, const std::string& fallback) {
    const char* env = std::getenv(variable);
    if (!env || !*env)
        return {fallback};
    std::vector<std::string> r;
    std::istringstream in(env);
    std::string s;
    while (std::getline(in, s, ':'))
        if (!s.empty())
            r.push_back(s);
    return r;
}

int main() {
    try {
        Temp temp;
        auto p = temp.path;
        write(p / "empty", "");
        int checks = 0;
        auto must = [&](const std::vector<std::string>& args, const fs::path& input) {
            int code = process(args, input, p / "stdout", p / "stderr", p);
            if (code) {
                std::ostringstream s;
                s << "command failed (" << code << "): ";
                for (auto& a : args)
                    s << a << ' ';
                s << '\n' << read(p / "stderr");
                throw std::runtime_error(s.str());
            }
        };
        std::mt19937_64 random(0x4a7070);
        auto r = [&](int n) { return static_cast<int>(random() % n); };
        std::vector<std::pair<std::string, std::string>> cases;
        std::ostringstream sequence;
        sequence << 35 << '\n';
        for (int i = 0; i < 35; ++i) {
            int n = 1 + r(55);
            sequence << n << '\n';
            for (int j = 0; j < n; ++j)
                sequence << (static_cast<long long>(r(100)) - 50) * (i % 2 ? 1LL : 1000000007LL)
                         << ' ';
            sequence << '\n';
        }
        cases.emplace_back("arrays", sequence.str());
        cases.emplace_back("vectors", sequence.str());
        cases.emplace_back("iterators", sequence.str());
        std::ostringstream graph;
        graph << 55 << ' ' << 400 << '\n';
        for (int i = 0; i < 400; ++i)
            graph << r(55) << ' ' << r(55) << ' ' << (1LL + r(1000000)) * 1000003 << '\n';
        cases.emplace_back("dijkstra", graph.str());
        std::ostringstream words;
        words << 32 << '\n';
        for (int i = 0; i < 32; ++i) {
            for (int j = 0, n = 1 + r(40); j < n; ++j)
                words << char('a' + r(26));
            words << '\n';
        }
        cases.emplace_back("values", words.str());
        std::ostringstream keys;
        keys << 300 << '\n';
        for (int i = 0; i < 300; ++i)
            keys << r(70) - 35 << ' ';
        keys << '\n';
        cases.emplace_back("maps", keys.str());
        cases.emplace_back("jumps", keys.str());
        cases.emplace_back("lambdas", "17\n");
        cases.emplace_back("cpp23", "-9\n");
        std::ostringstream bits;
        bits << 70 << '\n';
        for (int i = 0; i < 70; ++i)
            bits << (i < 3 ? std::vector<unsigned long long>{0, 1, ~0ULL}[i] : random()) << ' '
                 << random() % 9000000000000000000ULL << ' ' << random() % 9000000000000000000ULL
                 << ' ' << 1000000007 + r(1000) << '\n';
        cases.emplace_back("intrinsics", bits.str());
        std::ostringstream expressions;
        expressions << 48 << '\n';
        for (int i = 0; i < 48; ++i)
            expressions << (i % 8 == 0 ? 0 : r(101) - 50) << ' ' << r(17) - 8 << '\n';
        cases.emplace_back("expressions", expressions.str());
        cases.emplace_back("discarded", expressions.str());
        cases.emplace_back("heaps", expressions.str());
        cases.emplace_back("reference_returns", expressions.str());
        cases.emplace_back("gnu", expressions.str());
        cases.emplace_back("pointers", expressions.str());
        cases.emplace_back("objects", expressions.str());
        cases.emplace_back("statics", expressions.str());
        cases.emplace_back("calls", expressions.str());
        cases.emplace_back("standard_values", expressions.str());
        cases.emplace_back("ordering", expressions.str());
        cases.emplace_back("stream_state", expressions.str());
        cases.emplace_back("stream_exit", "");
        cases.emplace_back("map_iterators", expressions.str());
        cases.emplace_back("bindings", expressions.str());
        cases.emplace_back("halt", "");
        cases.emplace_back("coverage", "-19 37 5\n");
        cases.emplace_back("constants", "-17\n");
        cases.emplace_back("names", "-19\n");
        std::ostringstream library;
        library << 70 << '\n';
        for (int i = 0; i < 70; ++i)
            library << r(200001) - 100000 << ' ';
        library << '\n';
        cases.emplace_back("library", library.str());
        cases.emplace_back("files", library.str());
        cases.emplace_back("redirect", library.str());
        cases.emplace_back("algorithms", sequence.str());
        cases.emplace_back("conversions", "-9 4 1\n");
        cases.emplace_back("sequencing", "37\n");
        std::ostringstream formatted;
        formatted << 25 << '\n';
        for (int i = 0; i < 25; ++i)
            formatted << r(20001) - 10000 << ',' << static_cast<unsigned>(random()) << ' '
                      << static_cast<long long>(r(200001) - 100000) * 1000000007LL << ' '
                      << (r(20001) - 10000) * 0.25 << " cat @ abc\n";
        formatted << "-017 0x2a 37 999Q";
        cases.emplace_back("formatted", formatted.str());
        std::vector<std::string> tokens = {"0 -2147483648",
                                           "0 2147483648",
                                           "0 -2147483649",
                                           "0 +",
                                           "0 e",
                                           "1 -9223372036854775808",
                                           "1 9223372036854775808",
                                           "1 -9223372036854775809",
                                           "2 4294967295",
                                           "2 4294967296",
                                           "2 -1",
                                           "3 18446744073709551615",
                                           "3 18446744073709551616",
                                           "3 -1",
                                           "4 -32768",
                                           "4 32768",
                                           "5 65535",
                                           "5 65536",
                                           "5 -1",
                                           "6 -1.25",
                                           "6 1e+2",
                                           "6 1e+",
                                           "6 0x1.8p1",
                                           "6 inf",
                                           "6 .",
                                           "6 1e999",
                                           "6 1e-999",
                                           "7 0.100000001490116119384765625",
                                           "7 1e100",
                                           "8 0",
                                           "8 1",
                                           "8 2",
                                           "8 -1"};
        std::ostringstream inputCases;
        inputCases << "-9 17 42\n" << tokens.size() << '\n';
        for (const auto& token : tokens)
            inputCases << token << "Q\n";
        inputCases << "first\rline\nlast without newline";
        cases.emplace_back("input", inputCases.str());
        cases.emplace_back("maths", "25\n-7.75 -7.5 -7.25 -0.75 -0.5 -0.25 -0.0 0.0 0.25 0.5 "
                                    "0.75 7.25 7.5 7.75 2147483647.5 -2147483648.5 0x1p-1074 "
                                    "0x1p-1022 0x1p-32 0x1p32 1.0 inf -inf nan -nan\n");
        std::ostringstream unsignedCases;
        std::vector<unsigned long long> boundaries = {0,
                                                      1,
                                                      0xffffffffULL,
                                                      0x100000000ULL,
                                                      0x7fffffffffffffffULL,
                                                      0x8000000000000000ULL,
                                                      0xffffffffffffffffULL,
                                                      (1ULL << 63) + (1ULL << 39) + 1,
                                                      (1ULL << 63) + (3ULL << 39) - 1,
                                                      (1ULL << 63) + (1ULL << 10) + 1};
        unsignedCases << 90 << '\n';
        for (int i = 0; i < 90; ++i)
            unsignedCases << (i % 9 == 0 ? 0xffffffffU : static_cast<unsigned>(random())) << ' '
                          << (i < boundaries.size() ? boundaries[i] : random()) << '\n';
        cases.emplace_back("unsigned_conversions", unsignedCases.str());
        cases.emplace_back("bitsets", unsignedCases.str());
        std::string byteInput = "4\nélève 日本語\n";
        byteInput += "\xff\x80X\n";
        byteInput += std::string("prefix") + '\0' + "suffix\n";
        byteInput += "\r\n";
        byteInput += "élève\xff token\x80\n";
        cases.emplace_back("bytes", byteInput);
        auto switchBoundaries = boundaries;
        switchBoundaries.insert(
            switchBoundaries.end(),
            {0x100000001ULL, 7, 8, 0xfffffffffffffff0ULL, 0xffffffffffffffefULL});
        std::ostringstream switches;
        switches << switchBoundaries.size() * 4 + 60 << '\n';
        for (unsigned u : {0U, 1U, 0x80000000U, ~0U})
            for (auto bits : switchBoundaries)
                switches << u << ' ' << bits << '\n';
        for (int i = 0; i < 60; ++i)
            switches << static_cast<unsigned>(random()) << ' ' << random() << '\n';
        cases.emplace_back("switches", switches.str());
        write(p / "LibraryDriver.java", "import java.util.*; public class LibraryDriver {"
                                        "public static void main(String[] args) {"
                                        "Scanner in = new Scanner(System.in); int n = in.nextInt();"
                                        "ArrayList<Integer> values = new ArrayList<Integer>();"
                                        "while (n-- > 0) values.add(in.nextInt());"
                                        "System.out.println(Main.score(values)); }}\n");

        auto javacs = configured("JPP_TEST_JAVACS", "javac");
        auto javas = configured("JPP_TEST_JAVAS", "java");
        auto version = [&](const std::string& tool, bool compiler) {
            if (fs::path(tool).filename().string().find("ecj") != std::string::npos)
                return 17;
            must({tool, "-version"}, p / "empty");
            auto text = read(p / "stdout") + read(p / "stderr");
            std::smatch match;
            std::regex pattern(compiler ? R"(javac\s+(?:1\.)?(\d+))"
                                        : R"(version\s+"(?:1\.)?(\d+))");
            if (!std::regex_search(text, match, pattern))
                throw std::runtime_error("cannot identify Java version: " + tool + "\n" + text);
            return std::stoi(match[1]);
        };
        std::map<std::string, int> compilerVersions, runtimeVersions;
        for (auto& tool : javacs)
            compilerVersions[tool] = version(tool, true);
        for (auto& tool : javas)
            runtimeVersions[tool] = version(tool, false);
        std::set<std::string> selected;
        if (const char* selection = std::getenv("JPP_TEST_CASES"); selection && *selection) {
            for (const auto& name : configured("JPP_TEST_CASES", "")) {
                if (name != "cli" &&
                    std::none_of(cases.begin(), cases.end(),
                                 [&](const auto& test) { return test.first == name; }))
                    throw std::runtime_error("unknown test case: " + name);
                selected.insert(name);
            }
        }
        std::string cxx = std::getenv("CXX") ? std::getenv("CXX") : "g++";
        for (const auto& [name, input] : cases) {
            if (!selected.empty() && !selected.count(name))
                continue;
            auto source = (fs::path(JPP_SOURCE) / "tests/fixtures" / (name + ".cpp")).string();
            write(p / "input", input);
            write(p / "jpp-redirect.in", input);
            auto nativeSource =
                name == "library"
                    ? (fs::path(JPP_SOURCE) / "tests/fixtures/library_driver.cpp").string()
                    : source;
            must({cxx, "-std=gnu++23", "-O2", nativeSource, "-o", (p / "native").string()},
                 p / "empty");
            must({(p / "native").string()}, p / "input");
            auto expected = read(p / "stdout");
            auto expectedErrors = read(p / "stderr");
            std::map<std::string, std::string> expectedFiles;
            std::vector<std::string> outputNames;
            if (name == "files")
                outputNames = {"jpp-io-global.txt"};
            else if (name == "redirect")
                outputNames = {"jpp-redirect.out"};
            else if (name == "statics")
                outputNames = {"jpp-static.txt", "jpp-static-ref.txt"};
            else if (name == "halt")
                outputNames = {"jpp-halt.txt"};
            else if (name == "bytes")
                outputNames = {"jpp-byte-é.txt"};
            for (const auto& output : outputNames) {
                if (!fs::exists(p / output))
                    throw std::runtime_error("native file output is missing: " + name + ", " +
                                             output);
                expectedFiles.emplace(output, read(p / output));
            }
            // Generate once per requested Java release, then compile with each independent
            // compiler.
            for (int release : {8, 17, 21}) {
                must({JPP_BINARY, source, "-t", "-v", "j" + std::to_string(release), "-o",
                      (p / "Main.java").string()},
                     p / "empty");
                if (read(p / "Main.java").size() > 200000)
                    throw std::runtime_error("unexpectedly large output for " + name);
                for (const auto& javac : javacs) {
                    bool ecj = fs::path(javac).filename().string().find("ecj") != std::string::npos;
                    bool jdk8 = !ecj && compilerVersions[javac] == 8;
                    if (release > compilerVersions[javac])
                        continue;
                    auto classes = p / "classes";
                    fs::remove_all(classes);
                    fs::create_directory(classes);
                    std::vector<std::string> compile = {javac,   "-proc:none", "-encoding",
                                                        "UTF-8", "-d",         classes.string()};
                    if (jdk8) {
                        compile.insert(compile.end(), {"-source", "8", "-target", "8"});
                    } else
                        compile.insert(compile.end(), {"--release", std::to_string(release)});
                    compile.push_back((p / "Main.java").string());
                    if (name == "library")
                        compile.push_back((p / "LibraryDriver.java").string());
                    must(compile, p / "empty");
                    auto bytes = read(classes / "Main.class");
                    if (bytes.size() < 8 || static_cast<unsigned char>(bytes[7]) != release + 44)
                        throw std::runtime_error("incorrect class-file target");
                    for (const auto& java : javas) {
                        if (release > runtimeVersions[java])
                            continue;
                        for (const auto& [file, contents] : expectedFiles)
                            fs::remove(p / file);
                        must({java, "-cp", classes.string(),
                              name == "library" ? "LibraryDriver" : "Main"},
                             p / "input");
                        if (read(p / "stdout") != expected)
                            throw std::runtime_error("output differs from native C++: " + name +
                                                     ", release " + std::to_string(release) + ", " +
                                                     javac + ", " + java + "\nexpected:\n" +
                                                     expected.substr(0, 1000) + "\nactual:\n" +
                                                     read(p / "stdout").substr(0, 1000));
                        if ((name == "stream_state" || name == "stream_exit") &&
                            read(p / "stderr") != expectedErrors)
                            throw std::runtime_error("error output differs from native C++: " +
                                                     name);
                        for (const auto& [file, contents] : expectedFiles)
                            if (!fs::exists(p / file) || read(p / file) != contents)
                                throw std::runtime_error(
                                    "file output differs from native C++: " + name + ", " + file);
                        ++checks;
                    }
                }
            }
            std::cout << "PASS " << name << std::endl;
        }
        if (!selected.empty() && !selected.count("cli")) {
            std::cout << checks << " compiler/runtime differential checks passed\n";
            return 0;
        }
        // Rejection must preserve an existing submission, including across a failed --check.
        write(p / "bad.cpp", "int main(){ asm volatile(\"nop\"); }\n");
        write(p / "Main.java", "existing submission\n");
        int code = process({JPP_BINARY, (p / "bad.cpp").string(), "-o", (p / "Main.java").string()},
                           p / "empty", p / "stdout", p / "stderr");
        if (code == 0 || read(p / "Main.java") != "existing submission\n" ||
            read(p / "stderr").find("unsupported") == std::string::npos)
            throw std::runtime_error("unsupported construct was accepted or destroyed output");
        ++checks;
        write(p / "invalid.cpp", "int main( {\n");
        code = process({JPP_BINARY, (p / "invalid.cpp").string(), "-o", (p / "Main.java").string()},
                       p / "empty", p / "stdout", p / "stderr");
        if (code == 0 || read(p / "Main.java") != "existing submission\n")
            throw std::runtime_error("invalid C++ replaced output");
        ++checks;
        write(p / "exit.cpp",
              "#include <iostream>\nint main(){std::cout << 123 << '\\n'; return 7;}\n");
        code = process({JPP_BINARY, (p / "exit.cpp").string(), "-r", "-v", "j8", "-o",
                        (p / "Main.java").string()},
                       p / "empty", p / "stdout", p / "stderr");
        if (code != 7 || read(p / "stdout") != "123\n")
            throw std::runtime_error("-r did not forward output/exit status");
        ++checks;
        write(p / "exit.cpp", "#include <iostream>\n#include <cstdlib>\nint stop(){std::exit(7);}\n"
                              "int main(){std::cout << 321; stop();}\n");
        code = process({JPP_BINARY, (p / "exit.cpp").string(), "-r", "-v", "j8", "-o",
                        (p / "Main.java").string()},
                       p / "empty", p / "stdout", p / "stderr");
        if (code != 7 || read(p / "stdout") != "321")
            throw std::runtime_error("explicit C++ exit lost buffered output or its exit status");
        ++checks;
        // Exercise compiler selection through the real CLI. Ubuntu's ECJ fails at startup
        // without disabling annotation processing, even when the generated Java is valid.
        const char* compilerEnvironment = std::getenv("JPP_JAVAC");
        bool hadCompiler = compilerEnvironment != nullptr;
        std::string previousCompiler = hadCompiler ? compilerEnvironment : "";
        for (const auto& compiler : javacs) {
            if (fs::path(compiler).filename().string().find("ecj") == std::string::npos)
                continue;
            if (setenv("JPP_JAVAC", compiler.c_str(), 1))
                throw std::runtime_error("cannot select ECJ for CLI test");
            code = process({JPP_BINARY, (p / "exit.cpp").string(), "-r", "-v", "j8", "-o",
                            (p / "Main.java").string()},
                           p / "empty", p / "stdout", p / "stderr");
            if (code != 7 || read(p / "stdout") != "321")
                throw std::runtime_error("ECJ failed through -r: " + read(p / "stderr"));
            ++checks;
        }
        if (hadCompiler)
            setenv("JPP_JAVAC", previousCompiler.c_str(), 1);
        else
            unsetenv("JPP_JAVAC");
        std::cout << checks << " compiler/runtime differential and CLI checks passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
