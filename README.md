# SMC C++20 Clone

[English](README.md) | [简体中文](README_zh.md)

This project is an independent clone of [SMC (State Machine Compiler)](https://smc.sourceforge.net/).
The original SMC is implemented in Java; this project reimplements its main
functionality in C++20. It is not a complete port: currently, only C++ code
generation is supported.

The in-progress implementation includes an SMC lexer, parser, syntax checker,
and C++ generator. The lexer handles SMC directives, comments, source blocks,
and structural tokens; the parser builds an FSM model with maps, states,
transitions, guards, and opaque embedded C++ action/parameter text. The syntax
checker validates required C++ declarations, start-state references,
transition destinations, duplicate declarations/guards, and restrictions on
the default state.

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Validate a state-machine file without generating code:

```sh
./build/smc_cpp -c++ --check examples/SMC/SmcLexer.sm
```

Generate a header and source file next to the input (`SmcLexer.hpp` and
`SmcLexer.cpp`):

```sh
./build/smc_cpp -c++ examples/SMC/SmcLexer.sm
```

Use `-d` to select another output directory. Existing files are overwritten.
`--check` only validates the input and never writes generated files, including
when `-d` is specified:

```sh
./build/smc_cpp -c++ -d generated examples/SMC/SmcLexer.sm
./build/smc_cpp -c++ --check -d generated examples/SMC/SmcLexer.sm
```

## C++ generator API

After parsing an FSM, generate the two files through the visitor interface:

```cpp
std::ofstream header("ExampleFSM.hpp");
std::ofstream source("ExampleFSM.cpp");
smc::CppHeaderGenerator header_generator(header, "ExampleFSM.hpp");
smc::CppSourceGenerator source_generator(source, "ExampleFSM.hpp");
fsm.accept(header_generator);
fsm.accept(source_generator);
```

Include `smc/generator.hpp` and `<fstream>` in the caller. The generators
validate the complete model and throw `std::invalid_argument` for invalid or
unsupported input, and `std::ios_base::failure` on output failure. They do
not open files. The second constructor argument is the generated header's
include path, shared by both generators; `%fsmfile` does not choose output
paths. `%fsmclass` selects the FSM class; otherwise the input filename stem
followed by `Context` is used.
The same transition name must have consistent parameter types throughout the
FSM and consistent parameter names across its guards in one state. Generated
class, transition and parameter names must be valid C++ identifiers and must
not conflict with generated/runtime members.

Compile the generated source as C++20 with the directory
`include/headers/c++` on the include path. It includes the owner header named
by `%header` and the generated header. `%include` and `%import` apply to the
generated source; `%declare` provides declarations in the generated header.
Types used by transition signatures must be visible before the generated
header is included. The owner exposes the actions and holds the generated FSM;
call `enterStartState()` when its initial entry action should run.

Generated code uses exceptions, the dynamic state stack, and iostreams.
All translation units using `statemap.h` must use the same runtime macro
configuration; define `SMC_USES_IOSTREAMS` before any earlier inclusion of
`statemap.h`, and do not define `SMC_NO_EXCEPTIONS` or `SMC_FIXED_STACK`.
The generated header configures iostreams when it includes the runtime first.
Optional Java-generator flags such as CRTP and serialization are not
implemented.

## License

This project's original code is licensed under the [MIT License](LICENSE).
Some files copied from the original SMC project retain their own license and
copyright notices, including the MPL-1.1-licensed runtime headers and grammar
examples; those files are not relicensed under MIT.
