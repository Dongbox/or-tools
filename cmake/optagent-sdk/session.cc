#include "optagent_cpsat/session.h"

#include <atomic>
#include <exception>
#include <memory>
#include <string>
#include <utility>

#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/parameters_validation.h"
#include "ortools/util/time_limit.h"

namespace optagent::cpsat_sdk {
namespace cp = operations_research::sat;
namespace {
void SetError(std::string* destination, const char* message) noexcept {
  if (destination == nullptr) return;
  try {
    *destination = message;
  } catch (...) {
    // Preserve the status result even when reporting runs out of memory.
  }
}
}  // namespace

class Session::Impl {
 public:
  explicit Impl(std::shared_ptr<std::atomic<bool>> flag)
      : stop(flag ? std::move(flag)
                  : std::make_shared<std::atomic<bool>>(false)) {}
  std::shared_ptr<std::atomic<bool>> stop;
  std::atomic<bool> used{false};
};

Session::Session(std::shared_ptr<std::atomic<bool>> stop)
    : impl_(std::make_unique<Impl>(std::move(stop))) {}
Session::~Session() = default;

RunStatus Session::Run(const cp::CpModelProto& model,
                       const cp::SatParameters& parameters,
                       const Observer& observer, Response* response,
                       std::string* error) noexcept {
  if (impl_->used.exchange(true)) {
    SetError(error, "A CP-SAT session accepts only one Run");
    return RunStatus::kAlreadyUsed;
  }
  if (response == nullptr) {
    SetError(error, "A CP-SAT response destination is required");
    return RunStatus::kInvalidArgument;
  }
  try {
    cp::Model native;
    native.Add(cp::NewSatParameters(parameters));
    native.GetOrCreate<operations_research::TimeLimit>()
        ->RegisterExternalBooleanAsLimit(impl_->stop.get());
    if (observer) {
      native.Add(cp::NewFeasibleSolutionObserver(observer));
    }
    *response = cp::SolveCpModel(model, &native);
    if (error != nullptr) error->clear();
    return RunStatus::kOk;
  } catch (const std::exception& exception) {
    impl_->stop->store(true);
    SetError(error, exception.what());
  } catch (...) {
    impl_->stop->store(true);
    SetError(error, "Unknown CP-SAT exception");
  }
  return RunStatus::kException;
}

void Session::RequestStop() noexcept { impl_->stop->store(true); }

std::string ValidateParameters(const cp::SatParameters& parameters) {
  return cp::ValidateParameters(parameters);
}

std::string ValidateModel(const cp::CpModelProto& model,
                          const cp::SatParameters& parameters) {
  return cp::ValidateInputCpModel(parameters, model);
}

}  // namespace optagent::cpsat_sdk
