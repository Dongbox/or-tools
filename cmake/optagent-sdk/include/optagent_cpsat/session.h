#ifndef OPTAGENT_CPSAT_SESSION_H_
#define OPTAGENT_CPSAT_SESSION_H_

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include "optagent_cpsat/export.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace optagent::cpsat_sdk {

using Response = operations_research::sat::CpSolverResponse;
using Observer = std::function<void(const Response&)>;

enum class RunStatus {
  kOk,
  kAlreadyUsed,
  kInvalidArgument,
  kException,
  kOutOfMemory,
};

// Own the native Model, registrations, observer and teardown inside the SDK.
// Each instance accepts one synchronous Run. Keep it alive until Run and any
// concurrent RequestStop calls finish. Destruction from an observer is invalid.
class OPTAGENT_CPSAT_EXPORT Session final {
 public:
  // A supplied stop flag is retained for the whole session. It preserves an
  // application's shared cancellation signal without exposing native Model/TLS.
  explicit Session(std::shared_ptr<std::atomic<bool>> stop = {});
  ~Session();
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  // Borrow model/parameters/observer until return. Observer responses are only
  // valid during the call; copy explicitly to retain them. Internal and
  // callback allocation failures become kOutOfMemory; other exceptions become
  // kException. Both return an optional best-effort error message after native
  // teardown. Callbacks may run on native workers; protect
  // application state shared with other threads.
  RunStatus Run(const operations_research::sat::CpModelProto& model,
                const operations_research::sat::SatParameters& parameters,
                const Observer& observer, Response* response,
                std::string* error = nullptr) noexcept;

  // Thread-safe, idempotent and nonblocking, including from an observer.
  // Requests made before Run remain latched.
  void RequestStop() noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

OPTAGENT_CPSAT_EXPORT std::string ValidateParameters(
    const operations_research::sat::SatParameters& parameters);
OPTAGENT_CPSAT_EXPORT std::string ValidateModel(
    const operations_research::sat::CpModelProto& model,
    const operations_research::sat::SatParameters& parameters = {});

// Run one synchronous session without external cancellation. Internal and
// observer allocation failures become std::bad_alloc; other exceptions become
// std::runtime_error after native teardown.
OPTAGENT_CPSAT_EXPORT Response
Solve(const operations_research::sat::CpModelProto& model,
      const operations_research::sat::SatParameters& parameters,
      const Observer& observer = {});

}  // namespace optagent::cpsat_sdk

#endif  // OPTAGENT_CPSAT_SESSION_H_
