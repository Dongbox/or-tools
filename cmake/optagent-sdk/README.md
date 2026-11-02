# CP-SAT native SDK

This subproject builds only the audited CP-SAT closure with C++20. It consumes
one preinstalled protobuf/Abseil/support SDK and a matching host protoc. It does
not fetch or rebuild dependencies and does not build unrelated OR-Tools solvers.

The public session owns native Model registration and teardown inside the library.
Model compilation and application result verification remain with the consumer.
The C++ ABI requires the exact validated toolchain/runtime profile.

Use `update_closure.py <checkout>` to check upstream Bazel closure declarations.
Workflow execution is manual-only; repository Actions remains disabled during setup.
