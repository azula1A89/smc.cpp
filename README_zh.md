# SMC C++20 克隆项目

[English](README.md) | 简体中文

本项目是 [SMC（State Machine Compiler）](https://smc.sourceforge.net/) 的独立克隆。
原版 SMC 使用 Java 实现；本项目使用 C++20 重新实现其主要功能。它并非完整移植：
目前仅支持生成 C++ 代码。

本项目仍在开发中，包含 SMC 词法分析器、解析器、语法检查器和 C++ 生成器。词法分析器
处理 SMC 指令、注释、源码块和结构标记；解析器将输入构建为 FSM 模型，其中包括状态图、
状态、转换、守卫条件以及不透明的嵌入式 C++ 动作和参数文本。语法检查器会校验必要的
C++ 声明、起始状态引用、转换目标、重复声明和守卫条件，以及默认状态的限制。

## 构建与测试

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

校验状态机文件，但不生成代码：

```sh
./build/smc_cpp -c++ --check examples/SMC/SmcLexer.sm
```

在输入文件旁生成头文件和源文件（`SmcLexer.hpp` 和 `SmcLexer.cpp`）：

```sh
./build/smc_cpp -c++ examples/SMC/SmcLexer.sm
```

使用 `-d` 指定其他输出目录。已有文件会被覆盖。即使指定了 `-d`，`--check` 也只校验
输入，不会写入生成文件：

```sh
./build/smc_cpp -c++ -d generated examples/SMC/SmcLexer.sm
./build/smc_cpp -c++ --check -d generated examples/SMC/SmcLexer.sm
```

## C++ 生成器 API

解析 FSM 后，可通过 visitor 接口生成两个文件：

```cpp
std::ofstream header("ExampleFSM.hpp");
std::ofstream source("ExampleFSM.cpp");
smc::CppHeaderGenerator header_generator(header, "ExampleFSM.hpp");
smc::CppSourceGenerator source_generator(source, "ExampleFSM.hpp");
fsm.accept(header_generator);
fsm.accept(source_generator);
```

调用方需包含 `smc/generator.hpp` 和 `<fstream>`。生成器会校验完整模型；输入无效或不受
支持时抛出 `std::invalid_argument`，输出失败时抛出 `std::ios_base::failure`。生成器不会
自行打开文件。两个构造函数的第二个参数都是生成头文件的 include 路径；`%fsmfile` 不会
决定输出路径。`%fsmclass` 用于指定 FSM 类名；未指定时，使用输入文件名（不含扩展名）
并在其后追加 `Context`。

同一转换名称在整个 FSM 中必须使用一致的参数类型；同一状态内，该转换各守卫条件中的参数
名称也必须一致。生成的类名、转换名和参数名必须是有效的 C++ 标识符，并且不得与生成代码
或运行时成员冲突。

请使用 C++20 编译生成的源文件，并将 `include/headers/c++` 加入 include 路径。生成的源文件
会包含 `%header` 指定的 owner 头文件和生成的头文件。`%include` 和 `%import` 适用于生成的
源文件；`%declare` 会在生成的头文件中提供声明。转换签名使用的类型必须在包含生成的头文件
之前可见。owner 提供动作实现并持有生成的 FSM；需要执行初始 entry 动作时，请调用
`enterStartState()`。

生成的代码使用异常、动态状态栈和 iostream。所有使用 `statemap.h` 的编译单元必须采用相同
的运行时宏配置；如果 `statemap.h` 之前已被其他头文件包含，请在更早的位置定义
`SMC_USES_IOSTREAMS`，并且不要定义 `SMC_NO_EXCEPTIONS` 或 `SMC_FIXED_STACK`。若生成的头文件
首先包含运行时，它会自行配置 iostream。Java 生成器的部分可选标志（例如 CRTP 和序列化）
尚未实现。

## 许可

本项目原创代码采用 [MIT 许可证](LICENSE)。从原版 SMC 项目复制的部分文件保留其自身的
许可证和版权声明，包括采用 MPL-1.1 许可证的运行时头文件和语法示例；这些文件不适用 MIT
许可证。
