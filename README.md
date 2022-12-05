# lc3sim-c

A C implementation of the LC-3 virtual machine, an educational computer architecture.

[Read tutorial here](https://www.jmeiners.com/lc3-vm/)

## How to build

This project uses CMake to support a variety of build systems, platforms, and IDEs. Many IDEs
support opening CMake projects natively, and others support opening CMake projects through the use
of IDE plugins or CMake generators. You can also use CMake's command line generators to build using
`make` or `ninja`.

For details on building and running CMake projects, I'd recommend reading [An Introduction to Modern
CMake: Running CMake][running-cmake], checking your IDE's documentation for details about its
compatibility with CMake, or reading the CMake manual for details about supported [IDEs][cmake-ides]
and [generators][cmake-generators].

[running-cmake]: https://cliutils.gitlab.io/modern-cmake/chapters/intro/running.html
[cmake-ides]: https://cmake.org/cmake/help/latest/guide/ide-integration/index.html#ides-with-cmake-integration
[cmake-generators]: https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html#cmake-generators

### Build using command line

If your IDE isn't supported or you'd prefer to build using a command line generator, it's easy to
get started. For example, to build in debug mode:

```sh
# initialize the build scripts
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# build or rebuild as often as needed
cmake --build build

# run the vm
./build/lc3sim /path/to/lc3-program.obj
```

If you find yourself in need of trace-level logs for the VM's state, you can enable them by turning
on the `ENABLE_TRACING` flag and then rebuilding:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_TRACING=ON
cmake --build build
```

## Running tests

At the time of writing these instructions, this project only has a single "hello world!" unit test.
Even so, this simple test can still detect some more obvious failures if something goes really
wrong.

The unit tests for this project are defined using standard CMake/CTest conventions, so to run them,
you can either use your IDE's integrated unit test runner (if it has one), or by running the
following on command line:

```sh
# initialize the build scripts if you haven't already
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# build or rebuild if you haven't already
cmake --build build

# run the tests
ctest --test-dir build
```
