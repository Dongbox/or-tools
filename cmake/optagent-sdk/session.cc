#include "optagent_cpsat/session.h"

#include <atomic>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/parameters_validation.h"
#include "ortools/sat/sat_parameters.pb.h"
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
    // Outlive native Model teardown. Native worker callbacks must never unwind
    // through a std::thread entry point or across the SDK boundary.
    std::exception_ptr observer_error;
    std::mutex observer_error_mutex;
    cp::Model native;
    try {
      native.Add(cp::NewSatParameters(parameters));
      native.GetOrCreate<operations_research::TimeLimit>()
          ->RegisterExternalBooleanAsLimit(impl_->stop.get());
      if (observer) {
        native.Add(
            cp::NewFeasibleSolutionObserver([&](const Response& candidate) {
              try {
                observer(candidate);
              } catch (...) {
                std::lock_guard<std::mutex> lock(observer_error_mutex);
                if (!observer_error) observer_error = std::current_exception();
                impl_->stop->store(true);
              }
            }));
      }
      *response = cp::SolveCpModel(model, &native);
      if (observer_error) std::rethrow_exception(observer_error);
    } catch (...) {
      // Signal before Model teardown joins workers, including when the native
      // solve itself fails rather than an observer.
      impl_->stop->store(true);
      throw;
    }
    if (error != nullptr) error->clear();
    return RunStatus::kOk;
  } catch (const std::bad_alloc&) {
    impl_->stop->store(true);
    SetError(error, "CP-SAT allocation failed");
    return RunStatus::kOutOfMemory;
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

Response Solve(const cp::CpModelProto& model,
               const cp::SatParameters& parameters, const Observer& observer) {
  Session session;
  Response response;
  std::string error;
  const auto status =
      session.Run(model, parameters, observer, &response, &error);
  if (status == RunStatus::kOutOfMemory) throw std::bad_alloc();
  if (status != RunStatus::kOk) {
    throw std::runtime_error(error);
  }
  return response;
}

}  // namespace optagent::cpsat_sdk
