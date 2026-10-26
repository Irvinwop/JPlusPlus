namespace own {
int isalpha(int value) {
    return value + 1000;
}
} // namespace own

int selections;
int selected(int value) {
    ++selections;
    return value;
}

int main() {
    for (int value = -1; value < 256; ++value) {
        selections = 0;
        int results[] = {std::isalnum(selected(value)), ::isalpha(selected(value)),
                         std::isblank(selected(value)), ::iscntrl(selected(value)),
                         std::isdigit(selected(value)), ::isgraph(selected(value)),
                         std::islower(selected(value)), ::isprint(selected(value)),
                         std::ispunct(selected(value)), ::isspace(selected(value)),
                         std::isupper(selected(value)), ::isxdigit(selected(value))};
        // The C API specifies zero or nonzero, rather than a particular true value.
        // Native compilers can normalize isdigit differently during optimization.
        for (int result : results)
            std::cout << (result != 0) << ',';
        std::cout << ':' << selections << ':' << own::isalpha(value) << '\n';
    }
}
