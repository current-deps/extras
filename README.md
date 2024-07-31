[![C++](https://img.shields.io/badge/c%2B%2B-17-informational.svg)](https://en.cppreference.com/w/cpp/17) [![tests](https://github.com/current-deps/extras/actions/workflows/test.yml/badge.svg)](https://github.com/current-deps/extras/actions/workflows/test.yml) [![./run.sh](https://github.com/current-deps/extras/actions/workflows/run.yml/badge.svg)](https://github.com/current-deps/extras/actions/workflows/run.yml)

# `extras`

A few non-header-only Current components.

TODO(dkorolev): Break them into separate repos once we find out how to have dependencies between them.

TODO(dkorolev): Test this properly.

* The logger.
* The `popen2()` engine via `fork` + `exec`.
* The `dlopen` wrapper to pass interfaces and to re-load the libs dynamically.
* The lifetime manager / graceful shutdown subsystem.
* The pubsub engine.
