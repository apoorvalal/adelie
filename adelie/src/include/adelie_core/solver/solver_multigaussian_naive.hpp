#pragma once
#include <adelie_core/state/state_gaussian_naive.hpp>
#include <adelie_core/state/state_multigaussian_naive.hpp>
#include <adelie_core/solver/solver_gaussian_naive.hpp>
#include <adelie_core/util/functional.hpp>
#include <memory>

namespace adelie_core {
namespace solver {
namespace multigaussian {
namespace naive {

template <class StateType,
          class UpdateCoefficientsType,
          class CUIType=util::no_op>
inline void solve(
    StateType&& state,
    bool display,
    UpdateCoefficientsType update_coefficients_f,
    CUIType check_user_interrupt = CUIType()
)
{
    using state_t = std::decay_t<StateType>;
    using matrix_t = typename state_t::matrix_t;
    using value_t = typename state_t::value_t;
    using index_t = typename state_t::index_t;
    using bool_t = typename state_t::bool_t;
    using vec_value_t = typename state_t::vec_value_t;
    using state_gaussian_naive_t = state::StateGaussianNaive<
        matrix_t,
        value_t,
        index_t,
        bool_t
    >;

    const auto n_classes = state.n_classes;
    const auto multi_intercept = state.multi_intercept;
    auto& betas = state.betas;
    auto& intercepts = state.intercepts;

    gaussian::naive::solve(
        static_cast<state_gaussian_naive_t&>(state),
        display,
        update_coefficients_f,
        check_user_interrupt
    );

    intercepts.resize(betas.size(), n_classes);
    if (multi_intercept) {
        for (int i = 0; i < betas.size(); ++i) {
            intercepts.row(i) = Eigen::Map<const vec_value_t>(betas[i].valuePtr(), n_classes);
            betas[i] = betas[i].tail(betas[i].size() - n_classes);
        }
    } else {
        intercepts.setZero();
    }
}

} // namespace naive
} // namespace multigaussian
} // namespace solver
} // namespace adelie_core