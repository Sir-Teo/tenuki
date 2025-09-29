# Tenuki Engine

A lightweight Go (weiqi/baduk) engine core that follows the roadmap's M0 milestone: board rules, Zobrist hashing, GTP I/O, and SGF streaming utilities. This snapshot is intentionally lean so you can layer in search and neural inference changes later.

## Features

- Tromp–Taylor area rules with positional superko enforcement
- 19×19 (configurable) board representation with capture logic and simple ko detection
- Zobrist hashing for stones, ready for transposition tables
- PUCT search core (M1) with Dirichlet noise, temperature scheduling, and lightweight tree reuse
- Minimal GTP server powered by the search agent, ready for Sabaki-compatible analysis
- SGF loader/saver for a single game tree (FF[4])
- Smoke tests covering capture, ko, and Tromp–Taylor scoring

## Build

The project uses C++20. If CMake is available:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

On systems without CMake, you can compile directly with `clang++` or `g++`:

```bash
mkdir -p build
clang++ -std=c++20 -Iinclude src/main.cpp src/go/Board.cpp src/go/Zobrist.cpp \
    src/go/Rules.cpp src/gtp/GTP.cpp src/search/Search.cpp src/sgf/SGF.cpp -o build/tenuki_cli
clang++ -std=c++20 -Iinclude tests/BoardTests.cpp tests/SearchTests.cpp tests/TestMain.cpp \
    src/go/Board.cpp src/go/Zobrist.cpp src/go/Rules.cpp src/search/Search.cpp -o build/board_tests
```

## Usage

Run the GTP binary and issue commands (or connect from Sabaki/Lizzie):

```bash
printf "boardsize 9\ngenmove B\nshowboard\nquit\n" | ./build/tenuki_cli
```

## Tests

```
./build/board_tests
```

## Next Steps

- Extend GTP `genmove` with proper MCTS (Milestone M1)
- Promote `set_to_play`/hashing into a fuller move history manager
- Harden SGF parser (variations, comments) and add regression fixtures
