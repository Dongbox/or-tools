#include "optagent_cpsat/session.h"

#include <atomic>
#include <memory>
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
      !sdk::ValidateModel(model, parameters).empty())
    return 1;
  sdk::Session session;
  cp::CpSolverResponse response;
  std::string error;
  int candidates = 0;
  const auto status = session.Run(
      model, parameters, [&](const auto&) { ++candidates; }, &response, &error);
  if (status != sdk::RunStatus::kOk || !error.empty() ||
      response.status() != cp::OPTIMAL || response.solution_size() != 1 ||
      response.solution(0) != 1 || candidates == 0)
    return 2;
  if (session.Run(model, parameters, {}, &response, &error) !=
      sdk::RunStatus::kAlreadyUsed)
    return 3;
  auto flag = std::make_shared<std::atomic<bool>>(false);
  sdk::Session cancelled(flag);
  cancelled.RequestStop();
  if (!flag->load()) return 4;
  if (cancelled.Run(model, parameters, {}, &response, &error) !=
      sdk::RunStatus::kOk)
    return 5;
  return 0;
}
