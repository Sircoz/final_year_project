<img src="static/logo.svg" alt="Velox logo" width="50%" align="center" />

Velox is a C++ database acceleration library which provides reusable,
extensible, and high-performance data processing components. These components
can be reused to build compute engines focused on different analytical
workloads, including batch, interactive, stream processing, and AI/ML.
Velox was created by Facebook and it is currently developed in partnership with
Intel, ByteDance, and Ahana.

In common usage scenarios, Velox takes a fully optimized query plan as input
and performs the described computation. Considering Velox does not provide a
SQL parser, a dataframe layer, or a query optimizer, it is usually not meant
to be used directly by end-users; rather, it is mostly used by developers
integrating and optimizing their compute engines.

Velox provides the following high-level components:

* **Type**: a generic typing system that supports scalar, complex, and nested
  types, such as structs, maps, arrays, tensors, etc.
* **Vector**: an [Arrow-compatible columnar memory layout
  module](https://facebookincubator.github.io/velox/develop/vectors.html),
  which provides multiple encodings, such as Flat, Dictionary, Constant,
  Sequence/RLE, and Bias, in addition to a lazy materialization pattern and
  support for out-of-order writes.
* **Expression Eval**: a [fully vectorized expression evaluation
  engine](https://facebookincubator.github.io/velox/develop/expression-evaluation.html)
  that allows expressions to be efficiently executed on top of Vector/Arrow
  encoded data.
* **Function Packages**: sets of vectorized function implementations following
  the Presto and Spark semantic.
* **Operators**: implementation of common data processing operators such as
  scans, projection, filtering, groupBy, orderBy, shuffle, [hash
  join](https://facebookincubator.github.io/velox/develop/joins.html), unnest,
  and more.
* **I/O**: a generic connector interface that allows different file formats
  (ORC/DWRF and Parquet) and storage adapters (S3, HDFS, local files) to be
  used.
* **Network Serializers**: an interface where different wire protocols can be
  implemented, used for network communication, supporting
  [PrestoPage](https://prestodb.io/docs/current/develop/serialized-page.html)
  and Spark's UnsafeRow.
* **Resource Management**: a collection of primitives for handling
  computational resources, such as [memory
  arenas](https://facebookincubator.github.io/velox/develop/arena.html) and
  buffer management, tasks, drivers, and thread pools for CPU and thread
  execution, spilling, and caching.

Velox is extensible and allows developers to define their own engine-specific
specializations, including:

1. Custom types
2. [Simple and vectorized functions](https://facebookincubator.github.io/velox/develop/scalar-functions.html)
3. [Aggregate functions](https://facebookincubator.github.io/velox/develop/aggregate-functions.html)
4. Operators
5. File formats
6. Storage adapters
7. Network serializers

## Examples

Examples of extensibility and integration with different component APIs [can be
found here](velox/examples)

## Documentation

Developer guides detailing many aspects of the library, in addition to the list
of available functions [can be found here.](https://facebookincubator.github.io/velox)

## Getting Started

We provide scripts to help developers setup and install Velox dependencies.

### Get the Velox Source
```
git clone --recursive https://github.com/facebookincubator/velox.git
cd velox
# if you are updating an existing checkout
git submodule sync --recursive
git submodule update --init --recursive
```

### Setting up on macOS

Once you have checked out Velox, on an Intel MacOS machine you can setup and then build like so:

```shell
$ ./scripts/setup-macos.sh 
$ make
```

On an M1 MacOS machine you can build like so:

```shell
$ CPU_TARGET="arm64" ./scripts/setup-macos.sh
$ CPU_TARGET="arm64" make
```

You can also produce intel binaries on an M1, use `CPU_TARGET="sse"` for the above.

### Setting up on aarch64 Linux (Ubuntu 20.04 or later)

On an aarch64 based machine, you can build like so:

```shell
$ CPU_TARGET="aarch64" ./scripts/setup-ubuntu.sh
$ CPU_TARGET="aarch64" make
```

### Setting up on x86_64 Linux (Ubuntu 20.04 or later)

Once you have checked out Velox, you can setup and build like so:

```shell
$ ./scripts/setup-ubuntu.sh 
$ make
```

### Building Velox

Run `make` in the root directory to compile the sources. For development, use
`make debug` to build a non-optimized debug version, or `make release` to build
an optimized version.  Use `make unittest` to build and run tests.

Note that,
* Velox requires C++17 , thus minimum supported compiler is GCC 5.0 and Clang 5.0.
* Velox requires the CPU to support instruction sets:
  * bmi
  * bmi2
  * f16c
* Velox tries to use the following (or equivalent) instruction sets where available:
  * On Intel CPUs
    * avx  
    * avx2
    * sse
  * On ARM
    * Neon
    * Neon64

### Building Velox with docker-compose

If you don't want to install the system dependencies required to build Velox,
you can also build and run tests for Velox on a docker container
using [docker-compose](https://docs.docker.com/compose/).
Use the following commands:

```shell
$ docker-compose build ubuntu-cpp
$ docker-compose run --rm ubuntu-cpp
```
If you want to increase or decrease the number of threads used when building Velox
you can override the `NUM_THREADS` environment variable by doing:
```shell
$ docker-compose run -e NUM_THREADS=<NUM_THREADS_TO_USE> --rm ubuntu-cpp
```

## Contributing

Check our [contributing guide](CONTRIBUTING.md) to learn about how to
contribute to the project.

## Community

The main communication channel with the Velox OSS community is through the
[the Velox-OSS Slack workspace](http://velox-oss.slack.com). 
Please reach out to **velox@meta.com** to get access to Velox Slack Channel.


## License

Velox is licensed under the Apache 2.0 License. A copy of the license
[can be found here.](LICENSE)


# MEng Project

## Building

  Building the project requires the same steps as building the main Velox project.
  For the Python interface read the documentation under ```pyvelox/```.

  To avoid the requirement of sudo accesss when running the setup script make sure
  you have installed the dependencies between line 34 and 58 in ```scripts/setup-ubuntu.sh```
  and run:

  ```shell
  $ export INSTALL_PREFIX = path_that_doesnt_require_sudo
  $ export CMAKE_PREFIX_PATH = same_path
  ```

## Project code

The project code can be found in:
 * ```velox/exec/tests/project/projectExperiments/```: contains all the project experiments presented in the report
 * ```velox/exec/tests/utils/ProjectModifyExpressionUtils```: the library for modifying an expression
 * ```velox/exec/tests/utils/PlanBuilder```: added method filterCustom that takes an already compiled expression
 * ```velox/exec/tests/utils/AssertQueryBuilder```: addeed methods returnResults and assertAndReturnResults
 * ```velox/core/ITypedExpr```: added methods toSQLString and copyWInputs
 * ```velox/core/Expression```: added methods toSQLString and copyWInputs
 * ```pyvelox/pyvelox```: created python interface for the project
 * ```DuckDB/```: created various experiment to obtain the performance of DuckDB
 
 ## Running the experiments

 After running ```make``` all project experiments are created under ```_build/release/velox/exec/tests/project/projectExperiments/```.

 ### ProjectCompilatioTimesExperiment

 This experiment tests the differences between the compilation times from SQL to ITypedExpr and form ITypedExpr to ExprSet.

 To run it execute:

  ```shell
  $ ./_build/release/velox/exec/tests/project/projectExperiments/project_compilation_times_experiment 
  ```

  ## HardcodedQueiesMutationExperiment

  This experiment tests the mutation the times required to compile SQL to ITypedExpr versus the time of mutating
  an ITypedExpr to another ITypedExpr. This is done for two hardcoded queries.

  To run it execute:

  ```shell
  $ ./_build/release/velox/exec/tests/project/projectExperiments/project_hardcoded_queries_experiment
  ```

  ## QueryModificationExperiment

  This experiment tests the mutation the times required to compile SQL to ITypedExpr versus the time of mutating
  an ITypedExpr to another ITypedExpr. This is done on random queries using a mutation strategy.

  To run it execute:

  ```shell
  $ ./_build/release/velox/exec/tests/project/projectExperiments/project_query_modif_experiment
  ```

  ## PlanArchitectureExperiment

  This experiment tests the performances of different Query Plan architectures.

  To run it execute:

  ```shell
  $ ./_build/release/velox/exec/tests/project/projectExperiments/project_plan_architecture_experiment
  ```

  ## Running the VTuneExperiments

  All VTune experiments are under ```velox/exec/tests/projectExperiments/vtuneArchitectures```.
  To build them uncomment line 55 in ```velox/exec/tests/projectExperiments/CMakeLists.txt```.

  In addition make sure you follow the VTune guide for adding the ittnotify library.
  https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2023-0/configuring-your-build-system.html

  ## Upgrading to new versions of Velox

  For upgrading to the latest version of Velox merge the project with their latest GitHub branch. 
  Make sure you save from the project: 
 * ```velox/exec/tests/utils/ProjectModifyExpressionUtils```: the library for modifying an expression
 * ```velox/exec/tests/utils/PlanBuilder```: added method filterCustom that takes an already compiled expression
 * ```velox/exec/tests/utils/AssertQueryBuilder```: addeed methods returnResults and assertAndReturnResults
 * ```velox/core/ITypedExpr```: added methods toSQLString and copyWInputs
 * ```velox/core/Expression```: added methods toSQLString and copyWInputs
 * ```pyvelox/pyvelox```: created python interface for the project