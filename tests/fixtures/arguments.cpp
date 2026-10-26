#include <cstring>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    char text[6] = {'a', 'b', 'c', 0, 'z', 0};
    char* selected = text;
    std::cout << std::strlen("ab\0cd") << ' ' << __builtin_strlen("ab\0cd") << ' '
              << std::strlen(selected++) << ' ' << std::strlen(selected) << ' ' << (selected - text)
              << '\n';
    text[2] = 0;
    std::cout << std::strlen(text) << ' ' << __builtin_strlen(selected) << '\n';
    std::cout << argc << ' ' << (argv != nullptr) << ' ' << (argv[argc] == nullptr) << ' '
              << (argv[0] != nullptr && argv[0][0] != '\0') << '\n';
    for (int i = 1; i < argc; ++i) {
        std::cout << i << ':' << std::strlen(argv[i]) << ':' << argv[i] << '\n';
        for (const unsigned char* p = reinterpret_cast<unsigned char*>(argv[i]); *p; ++p)
            std::cout << static_cast<unsigned>(*p) << ' ';
        std::cout << '\n';
    }
    if (argc > 1) {
        char* saved = argv[1];
        if (*saved)
            *saved ^= 0x20;
        std::cout << saved << ' ' << argv[1] << '\n';
        argv[1] = argv[argc - 1];
        std::cout << (saved == argv[1]) << ' ' << (argv[1] == argv[argc - 1]) << ' '
                  << std::string(saved).size() << '\n';
    }
    char** start = argv;
    char** past = argv + argc;
    ++argv;
    --argc;
    std::cout << (past - start) << ' ' << (argv - start) << ' ' << argc << ' '
              << (argv[argc] == nullptr) << '\n';
}
