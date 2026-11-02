#include "optagent_cpsat/session.h"

#include <atomic>
#include <initializer_list>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"

int main() {
  namespace cp = operations_research::sat;
  namespace sdk = optagent::cpsat_sdk;
  cp::CpModelProto model;
  auto* variable = model.add_variables();
  variable->add_domain(0);
  variable->add_domain(1);
  model.mutable_objective()->add_vars(0);
  model.mutable_objective()->add_coeffs(-1);
  cp::SatParameters parameters;
  parameters.set_num_search_workers(1);
  if (!sdk::ValidateParameters(parameters).empty() ||
      !sdk::ValidateModel(model, parameters).empty()) {
    return 1;
  }
  sdk::Session session;
  cp::CpSolverResponse response;
  std::string error;
  int candidates = 0;
  const auto status = session.Run(
      model, parameters, [&](const auto&) { ++candidates; }, &response, &error);
  if (status != sdk::RunStatus::kOk || !error.empty() ||
      response.status() != cp::OPTIMAL || response.solution_size() != 1 ||
      response.solution(0) != 1 || candidates == 0) {
    return 2;
  }
  if (session.Run(model, parameters, {}, &response, &error) !=
      sdk::RunStatus::kAlreadyUsed) {
    return 3;
  }
  auto flag = std::make_shared<std::atomic<bool>>(false);
  sdk::Session cancelled(flag);
  cancelled.RequestStop();
  if (!flag->load()) return 4;
  if (cancelled.Run(model, parameters, {}, &response, &error) !=
      sdk::RunStatus::kOk) {
    return 5;
  }
  if (response.status() != cp::UNKNOWN) return 6;
  const auto solved = sdk::Solve(model, parameters);
  if (solved.status() != cp::OPTIMAL || solved.solution(0) != 1) return 7;
  sdk::Session invalid;
  if (invalid.Run(model, parameters, {}, nullptr, &error) !=
          sdk::RunStatus::kInvalidArgument ||
      error.empty()) {
    return 8;
  }
  sdk::Session callback_stop;
  if (callback_stop.Run(
          model, parameters, [&](const auto&) { callback_stop.RequestStop(); },
          &response, &error) != sdk::RunStatus::kOk ||
      (response.status() != cp::FEASIBLE && response.status() != cp::OPTIMAL)) {
    return 9;
  }
  for (const int workers : {1, 2}) {
    parameters.set_num_search_workers(workers);
    sdk::Session callback_failure;
    if (callback_failure.Run(
            model, parameters,
            [](const auto&) { throw std::runtime_error("observer failure"); },
            &response, &error) != sdk::RunStatus::kException ||
        error != "observer failure") {
      return 10;
    }
    auto allocation_stop = std::make_shared<std::atomic<bool>>(false);
    sdk::Session allocation_failure(allocation_stop);
    if (allocation_failure.Run(
            model, parameters, [](const auto&) { throw std::bad_alloc(); },
            &response, &error) != sdk::RunStatus::kOutOfMemory ||
        !allocation_stop->load() || error.empty()) {
      return 11;
    }
  }
  bool allocation_rethrown = false;
  try {
    sdk::Solve(model, parameters, [](const auto&) { throw std::bad_alloc(); });
  } catch (const std::bad_alloc&) {
    allocation_rethrown = true;
  }
  if (!allocation_rethrown) return 12;
  return 0;
}
