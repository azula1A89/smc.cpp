# SoulsNpc: SMC C++ Interactive Example

This example uses `smc_cpp` to generate a C++ state machine from
[SoulsNpc.sm](SoulsNpc.sm). The `SoulsNpc` owner provides actions, guard
conditions, and interactive events. It is a deterministic command-line
simulation; it does not implement real-time combat, timers, or graphics.
Actions only print their effects and do not send events or change guard
conditions automatically.

## Build, generate, and test

A CMake 3.20 installation and a C++20 compiler are required. Run these
commands from this directory:

```sh
# Build the generator without enabling its own tests.
cmake -S ../.. -B build/smc -DBUILD_TESTING=OFF
cmake --build build/smc --target smc_cpp

# Validate the state machine without writing generated files.
./build/smc/smc_cpp -c++ --check SoulsNpc.sm

# Build the example, passing the generator's absolute path at configure time.
cmake -S . -B build/demo -DSMC_EXECUTABLE="$(pwd)/build/smc/smc_cpp"
cmake --build build/demo
ctest --test-dir build/demo --output-on-failure
./build/demo/souls_npc
```

The example build invokes `smc_cpp -c++ -d build/demo SoulsNpc.sm` to generate
`build/demo/SoulsNpc.hpp` and `build/demo/SoulsNpc.cpp`. It also requires the
runtime in `../../include/headers/c++`. The files are regenerated when the
`.sm` file or generator executable changes. Generated files are not hand-written
sources and should not be committed.

`SoulsNpc.h` includes the generated header and owns a `SoulsNpcContext`;
`npc.cpp` implements the owner actions and guard queries used by the `.sm`
file. `main.cpp` calls `enterStartState()` to run the initial entry action,
then sends events to the generated context. State changes and actions are
printed to the terminal.

## Usage

- `help` lists commands and events. `status` shows the current state, state
  stack depth, and guards.
- `event <name>` sends an event, for example `event ThinkTimer`. `Sensed`
  requires an argument, such as `event Sensed true`. The `.sm` file keeps this
  parameter as a signature example; its value is not currently used.
- `set <boss|token|lowhp|far> <true|false>` sets a guard. All guards default to
  `false`. `lowhp` and `far` are switches, not simulations of health or
  distance. Actions that print “release token” or “heal” do not update these
  switches automatically.
- `quit` or end-of-file (EOF) exits. Commands and events are case-sensitive.
  Invalid input or an event undefined in the current state reports an error
  and the process continues. `nil` transitions and the `Dead` state's default
  transition intentionally ignore events and are not errors.

For example, enter these commands in order:

```text
event Sensed true
event TargetSpotted
event InAttackRange
set token true
event ThinkTimer
event MovePicked
event WindupDone
event StrikeDone
event RecoveryDone
event BeyondLeash
event AtSpawn
```

This path moves from `Idle` to `Chase`, pushes `CombatMap::Strafe` (stack
depth 1), proceeds through `Windup`, `Strike`, and `Recovery`, returns to
`Strafe`, then pops to `Returning` (stack depth 0) and returns to `Idle`. To
exercise `Retreat`, run `set lowhp true` before `ThinkTimer`. To enter
`GapCloser`, leave `token` false and set `far` to true. In `Strafe`, set
`boss` to true and send `HpBelowPhaseThreshold` to demonstrate a pop followed
by a phase transition. `HpZero` and `Stagger` also demonstrate popping from
the combat sub-machine to the main state machine.
