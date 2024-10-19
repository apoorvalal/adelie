#include "py_decl.hpp"

namespace py = pybind11;
namespace ad = adelie_core;

// =================================================================
// Helper functions
// =================================================================
template <class T>
ad::util::rowvec_type<T> compute_penalty_sparse(
    const Eigen::Ref<const ad::util::rowvec_type<int>>& groups,
    const Eigen::Ref<const ad::util::rowvec_type<int>>& group_sizes,
    const Eigen::Ref<const ad::util::rowvec_type<T>>& penalty,
    T alpha,
    const Eigen::SparseMatrix<T, Eigen::RowMajor>& betas,
    size_t n_threads
)
{
    using value_t = T;
    using vec_value_t = ad::util::rowvec_type<value_t>;
    using sp_mat_value_t = Eigen::SparseMatrix<T, Eigen::RowMajor>;

    vec_value_t out(betas.outerSize());

    const auto routine = [&](int k) {
        typename sp_mat_value_t::InnerIterator it(betas, k);
        value_t pnlty = 0;
        for (int i = 0; i < groups.size(); ++i) {
            if (!it) break;
            const auto g = groups[i];
            const auto gs = group_sizes[i];
            const auto pg = penalty[i];
            value_t norm = 0;
            while (it && (it.index() >= g) && (it.index() < g + gs)) {
                norm += it.value() * it.value();
                ++it;
            }
            norm = std::sqrt(norm);
            pnlty += pg * norm * (alpha + 0.5 * (1-alpha) * norm);
        }
        out[k] = pnlty;
    };
    if (n_threads <= 1) {
        for (int k = 0; k < betas.outerSize(); ++k) routine(k);
    } else {
        #pragma omp parallel for schedule(static) num_threads(n_threads)
        for (int k = 0; k < betas.outerSize(); ++k) routine(k);
    }

    return out;
}

template <class T>
ad::util::rowvec_type<T> compute_penalty_dense(
    const Eigen::Ref<const ad::util::rowvec_type<int>>& groups,
    const Eigen::Ref<const ad::util::rowvec_type<int>>& group_sizes,
    const Eigen::Ref<const ad::util::rowvec_type<T>>& penalty,
    T alpha,
    const Eigen::Ref<const ad::util::rowmat_type<T>>& betas,
    size_t n_threads
)
{
    using value_t = T;
    using vec_value_t = ad::util::rowvec_type<value_t>;

    vec_value_t out(betas.rows());

    const auto routine = [&](int k) {
        const auto beta_k = betas.row(k);
        value_t pnlty = 0;
        for (int i = 0; i < groups.size(); ++i) {
            const auto g = groups[i];
            const auto gs = group_sizes[i];
            const auto pg = penalty[i];
            value_t norm = Eigen::Map<const vec_value_t>(
                beta_k.data() + g, gs
            ).matrix().norm();
            pnlty += pg * norm * (alpha + 0.5 * (1-alpha) * norm);
        }
        out[k] = pnlty;
    };
    if (n_threads <= 1) {
        for (int k = 0; k < betas.rows(); ++k) routine(k);
    } else {
        #pragma omp parallel for schedule(static) num_threads(n_threads)
        for (int k = 0; k < betas.rows(); ++k) routine(k);
    }

    return out;
}

void register_solver(py::module_& m)
{
    /* helper functions */
    m.def("compute_penalty_sparse", &compute_penalty_sparse<double>);
    m.def("compute_penalty_dense", &compute_penalty_dense<double>);
}