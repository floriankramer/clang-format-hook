# clang-format-hook

This repository contains a cpp wrapper around clang format designed to be used in a git
pre-commit hook. There are probably better implementations out there, but I wanted
one written in cpp and was fealing like doing it myself.

This hook only works on linux.

## Using the Hook
First clone the repo and build the wrapper using the provided CMakeLists.txt. E.g.
```
git clone https://github.com/floriankramer/clang-format-hook.git
cd clang-format-hook
mkdir build
cd build
cmake ..
cmake --build
```
Then simply copy the `clang-format-hook` executabe from the build folder and the
`pre-commit` script file from the `hooks` folder into your git repositories
`.git/hooks` folder. If you need to use a non system clang-formt modify the `pre-commit`script
and add `-c <path-to-your-clang-format>` to the line invoking `clang-format-hook`.
