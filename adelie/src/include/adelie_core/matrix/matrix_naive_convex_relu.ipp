#pragma once
#include <adelie_core/matrix/matrix_naive_convex_relu.hpp>
#include <adelie_core/matrix/utils.hpp>

namespace adelie_core {
namespace matrix {

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE::_ctmul(
    int j, 
    value_t v, 
    Eigen::Ref<vec_value_t> out,
    size_t n_threads
)
{
    const auto d = _mat.cols();
    const auto m = _mask.cols();
    const auto j_sgn = j / (m * d);
    j -= j_sgn * m * d;
    const auto j_m = j / d;
    j -= j_m * d;
    const auto j_d = j;
    dvaddi(
        out, 
        (v * (1-2*j_sgn)) * _mat.col(j_d).cwiseProduct(
            _mask.col(j_m).template cast<value_t>()
        ).array(),
        n_threads
    );
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE_TP
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE::MatrixNaiveConvexReluDense(
    const Eigen::Ref<const dense_t>& mat,
    const Eigen::Ref<const mask_t>& mask,
    size_t n_threads
):
    _mat(mat.data(), mat.rows(), mat.cols()),
    _mask(mask.data(), mask.rows(), mask.cols()),
    _n_threads(n_threads),
    _buff(n_threads * std::min<size_t>(mat.rows(), mat.cols()) + mat.rows())
{
    const auto n = mat.rows();

    if (mask.rows() != n) {
        throw util::adelie_core_error("mask must be (n, m) where mat is (n, d).");
    }
    if (n_threads < 1) {
        throw util::adelie_core_error("n_threads must be >= 1.");
    }
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE_TP
typename ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE::value_t
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE::cmul(
    int j, 
    const Eigen::Ref<const vec_value_t>& v,
    const Eigen::Ref<const vec_value_t>& weights
) 
{
    base_t::check_cmul(j, v.size(), weights.size(), rows(), cols());
    const auto d = _mat.cols();
    const auto m = _mask.cols();
    const auto j_sgn = j / (m * d);
    j -= j_sgn * m * d;
    const auto j_m = j / d;
    j -= j_m * d;
    const auto j_d = j;
    return (1-2*j_sgn) * ddot(
        _mat.col(j_d).cwiseProduct(_mask.col(j_m).template cast<value_t>()),
        (v * weights).matrix(),
        _n_threads,
        _buff
    );
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE::ctmul(
    int j, 
    value_t v, 
    Eigen::Ref<vec_value_t> out
) 
{
    base_t::check_ctmul(j, out.size(), rows(), cols());
    _ctmul(j, v, out, _n_threads);
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE::bmul(
    int j, int q, 
    const Eigen::Ref<const vec_value_t>& v, 
    const Eigen::Ref<const vec_value_t>& weights,
    Eigen::Ref<vec_value_t> out
) 
{
    base_t::check_bmul(j, q, v.size(), weights.size(), out.size(), rows(), cols());
    const auto d = _mat.cols();
    const auto m = _mask.cols();
    int n_processed = 0;
    while (n_processed < q) {
        auto k = j + n_processed;
        const auto k_sgn = k / (m * d);
        k -= k_sgn * m * d;
        const auto k_m = k / d;
        k -= k_m * d;
        const auto k_d = k;
        const auto size = std::min<int>(d-k_d, q-n_processed);
        auto out_m = out.segment(n_processed, size).matrix();
        Eigen::Map<rowmat_value_t> buff(_buff.data(), _n_threads, d);
        dgemv(
            _mat.middleCols(k_d, size),
            (1-2*k_sgn) * _mask.col(k_m).transpose().template cast<value_t>().cwiseProduct((v * weights).matrix()),
            _n_threads,
            buff,
            out_m
        );
        n_processed += size;
    }
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE::btmul(
    int j, int q, 
    const Eigen::Ref<const vec_value_t>& v, 
    Eigen::Ref<vec_value_t> out
) 
{
    base_t::check_btmul(j, q, v.size(), out.size(), rows(), cols());
    const auto n = _mat.rows();
    const auto d = _mat.cols();
    const auto m = _mask.cols();
    int n_processed = 0;
    while (n_processed < q) {
        auto k = j + n_processed;
        const auto k_sgn = k / (m * d);
        k -= k_sgn * m * d;
        const auto k_m = k / d;
        k -= k_m * d;
        const auto k_d = k;
        const auto size = std::min<int>(d-k_d, q-n_processed);
        Eigen::Map<vec_value_t> Xv(_buff.data(), n);
        Eigen::Map<rowmat_value_t> buff(_buff.data() + n, _n_threads, n);
        auto Xv_m = Xv.matrix();
        dgemv(
            _mat.middleCols(k_d, size).transpose(),
            v.segment(n_processed, size).matrix(),
            _n_threads,
            buff,
            Xv_m
        );
        dvaddi(
            out, 
            (1-2*k_sgn) * Xv * _mask.col(k_m).transpose().template cast<value_t>().array(), 
            _n_threads
        );
        n_processed += size;
    }
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE::mul(
    const Eigen::Ref<const vec_value_t>& v, 
    const Eigen::Ref<const vec_value_t>& weights,
    Eigen::Ref<vec_value_t> out
) 
{
    const auto d = _mat.cols();
    const auto m = _mask.cols();
    // NOTE: MSVC does not like it when we try to capture v_weights and buff.
    // This is a bug in MSVC (god I hate microsoft so much...).
    const auto routine = [&](auto i, const auto& v_weights, auto& buff) {
        const auto i_sgn = i / m;
        const auto i_m = i - i_sgn * m;
        auto out_m = out.segment(i * d, d).matrix();
        dgemv(
            _mat,
            (1-2*i_sgn) * _mask.col(i_m).transpose().template cast<value_t>().cwiseProduct(v_weights),
            1,
            buff /* unused */,
            out_m
        );
    };
    const auto v_weights = (v * weights).matrix();
    Eigen::Map<rowmat_value_t> buff(_buff.data(), _n_threads, d);
    if (_n_threads <= 1) {
        for (int i = 0; i < 2 * m; ++i) routine(i, v_weights, buff);
    } else {
        #pragma omp parallel for schedule(static) num_threads(_n_threads)
        for (int i = 0; i < 2 * m; ++i) routine(i, v_weights, buff);
    }
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE_TP
int
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE::rows() const
{
    return _mat.rows();
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE_TP
int
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE::cols() const
{
    return _mat.cols() * _mask.cols() * 2;
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE::cov(
    int j, int q,
    const Eigen::Ref<const vec_value_t>& sqrt_weights,
    Eigen::Ref<colmat_value_t> out,
    Eigen::Ref<colmat_value_t> buffer
) 
{
    base_t::check_cov(
        j, q, sqrt_weights.size(), 
        out.rows(), out.cols(), buffer.rows(), buffer.cols(), 
        rows(), cols()
    );

    const auto d = _mat.cols();
    const auto m = _mask.cols();

    Eigen::setNbThreads(_n_threads);

    int n_processed = 0;
    while (n_processed < q) {
        auto k = j + n_processed;
        const auto k_sgn = k / (m * d);
        k -= k_sgn * m * d;
        const auto k_m = k / d;
        k -= k_m * d;
        const auto k_d = k;
        const auto size = std::min<int>(d-k_d, q-n_processed);
        const auto mat = _mat.middleCols(k_d, size);
        const auto mask = _mask.col(k_m);

        auto curr_block = buffer.middleCols(n_processed, size).array();
        curr_block.array() = (1-2*k_sgn) * (
            mat.array().colwise() * 
            mask.template cast<value_t>().cwiseProduct(sqrt_weights.matrix().transpose()).array()
        );
        n_processed += size;
    }

    out = buffer.transpose() * buffer;
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE::sq_mul(
    const Eigen::Ref<const vec_value_t>& weights,
    Eigen::Ref<vec_value_t> out
) 
{
    const auto d = _mat.cols();
    const auto m = _mask.cols();
    colmat_value_t mat_sq = _mat.array().square().matrix();
    // NOTE: MSVC does not like it when we try to capture v_weights and buff.
    // This is a bug in MSVC (god I hate microsoft so much...).
    const auto routine = [&](auto i, const auto& w, auto& buff) {
        auto out_m = out.segment(i * d, d).matrix();
        dgemv(
            mat_sq,
            _mask.col(i).transpose().template cast<value_t>().cwiseProduct(w.matrix()),
            1,
            buff /* unused */,
            out_m
        );
    };
    Eigen::Map<rowmat_value_t> buff(_buff.data(), _n_threads, d);
    if (_n_threads <= 1) {
        for (int i = 0; i < m; ++i) routine(i, weights, buff);
    } else {
        #pragma omp parallel for schedule(static) num_threads(_n_threads)
        for (int i = 0; i < m; ++i) routine(i, weights, buff);
    }
    out.tail(m * d) = out.head(m * d);
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_DENSE::sp_tmul(
    const sp_mat_value_t& v, 
    Eigen::Ref<rowmat_value_t> out
) 
{
    base_t::check_sp_tmul(
        v.rows(), v.cols(), out.rows(), out.cols(), rows(), cols()
    );
    const auto routine = [&](int k) {
        typename sp_mat_value_t::InnerIterator it(v, k);
        auto out_k = out.row(k);
        out_k.setZero();
        for (; it; ++it) {
            _ctmul(it.index(), it.value(), out_k, 1);
        }
    };
    if (_n_threads <= 1) {
        for (int k = 0; k < v.outerSize(); ++k) routine(k);
    } else {
        #pragma omp parallel for schedule(static) num_threads(_n_threads)
        for (int k = 0; k < v.outerSize(); ++k) routine(k);
    }
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE_TP
typename ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::value_t 
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::_cmul(
    int j, 
    const Eigen::Ref<const vec_value_t>& v,
    const Eigen::Ref<const vec_value_t>& weights,
    size_t n_threads
)
{
    const auto d = _mat.cols();
    const auto m = _mask.cols();
    const auto j_sgn = j / (m * d);
    j -= j_sgn * m * d;
    const auto j_m = j / d;
    j -= j_m * d;
    const auto j_d = j;
    const auto outer = _mat.outerIndexPtr();
    const auto outer_j_d = outer[j_d];
    const auto size_j_d = outer[j_d+1] - outer_j_d;
    const Eigen::Map<const vec_sp_index_t> inner_j_d(
        _mat.innerIndexPtr() + outer_j_d,
        size_j_d
    );
    const Eigen::Map<const vec_sp_value_t> value_j_d(
        _mat.valuePtr() + outer_j_d,
        size_j_d
    );
    return (1-2*j_sgn) * spddot(
        inner_j_d,
        value_j_d,
        (v * weights * _mask.col(j_m).transpose().array().template cast<value_t>()),
        n_threads,
        _buff
    );
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE_TP
void 
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::_ctmul(
    int j, 
    value_t v, 
    Eigen::Ref<vec_value_t> out,
    size_t n_threads
)
{
    const auto d = _mat.cols();
    const auto m = _mask.cols();
    const auto j_sgn = j / (m * d);
    j -= j_sgn * m * d;
    const auto j_m = j / d;
    j -= j_m * d;
    const auto j_d = j;
    const auto outer = _mat.outerIndexPtr();
    const auto outer_j_d = outer[j_d];
    const auto size_j_d = outer[j_d+1] - outer_j_d;
    const Eigen::Map<const vec_sp_index_t> inner_j_d(
        _mat.innerIndexPtr() + outer_j_d,
        size_j_d
    );
    const Eigen::Map<const vec_sp_value_t> value_j_d(
        _mat.valuePtr() + outer_j_d,
        size_j_d
    );
    spdaddi(
        inner_j_d, 
        value_j_d, 
        (v * (1-2*j_sgn)) * _mask.col(j_m).transpose().array().template cast<value_t>(),
        out, 
        n_threads
    );
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE_TP
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::MatrixNaiveConvexReluSparse(
    size_t rows,
    size_t cols,
    size_t nnz,
    const Eigen::Ref<const vec_sp_index_t>& outer,
    const Eigen::Ref<const vec_sp_index_t>& inner,
    const Eigen::Ref<const vec_sp_value_t>& value,
    const Eigen::Ref<const mask_t>& mask,
    size_t n_threads
):
    _mat(rows, cols, nnz, outer.data(), inner.data(), value.data()),
    _mask(mask.data(), mask.rows(), mask.cols()),
    _n_threads(n_threads),
    _buff(n_threads)
{
    const Eigen::Index n = rows;

    if (mask.rows() != n) {
        throw util::adelie_core_error("mask must be (n, m) where mat is (n, d).");
    }
    if (n_threads < 1) {
        throw util::adelie_core_error("n_threads must be >= 1.");
    }
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE_TP
typename ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::value_t
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::cmul(
    int j, 
    const Eigen::Ref<const vec_value_t>& v,
    const Eigen::Ref<const vec_value_t>& weights
) 
{
    base_t::check_cmul(j, v.size(), weights.size(), rows(), cols());
    return _cmul(j, v, weights, _n_threads);
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::ctmul(
    int j, 
    value_t v, 
    Eigen::Ref<vec_value_t> out
) 
{
    base_t::check_ctmul(j, out.size(), rows(), cols());
    _ctmul(j, v, out, _n_threads);
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::bmul(
    int j, int q, 
    const Eigen::Ref<const vec_value_t>& v, 
    const Eigen::Ref<const vec_value_t>& weights,
    Eigen::Ref<vec_value_t> out
) 
{
    base_t::check_bmul(j, q, v.size(), weights.size(), out.size(), rows(), cols());
    for (int k = 0; k < q; ++k) {
        out[k] = _cmul(j+k, v, weights, _n_threads);
    }
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::btmul(
    int j, int q, 
    const Eigen::Ref<const vec_value_t>& v, 
    Eigen::Ref<vec_value_t> out
) 
{
    base_t::check_btmul(j, q, v.size(), out.size(), rows(), cols());
    for (int k = 0; k < q; ++k) {
        _ctmul(j+k, v[k], out, _n_threads);
    }
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::mul(
    const Eigen::Ref<const vec_value_t>& v, 
    const Eigen::Ref<const vec_value_t>& weights,
    Eigen::Ref<vec_value_t> out
) 
{
    const auto routine = [&](int k) {
        out[k] = _cmul(k, v, weights, 1);
    };
    if (_n_threads <= 1) {
        for (int k = 0; k < out.size(); ++k) routine(k);
    } else {
        #pragma omp parallel for schedule(static) num_threads(_n_threads)
        for (int k = 0; k < out.size(); ++k) routine(k);
    }
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE_TP
int
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::rows() const
{
    return _mat.rows();
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE_TP
int
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::cols() const
{
    return _mat.cols() * _mask.cols() * 2;
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::cov(
    int j, int q,
    const Eigen::Ref<const vec_value_t>& sqrt_weights,
    Eigen::Ref<colmat_value_t> out,
    Eigen::Ref<colmat_value_t> buffer
) 
{
    base_t::check_cov(
        j, q, sqrt_weights.size(), 
        out.rows(), out.cols(), buffer.rows(), buffer.cols(), 
        rows(), cols()
    );

    const auto d = _mat.cols();
    const auto m = _mask.cols();

    const auto routine = [&](int i1) {
        auto index_1 = j+i1;
        const auto index_1_sgn = index_1 / (m * d);
        index_1 -= index_1_sgn * m * d;
        const auto index_1_m = index_1 / d;
        index_1 -= index_1_m * d;
        const auto outer_1 = _mat.outerIndexPtr()[index_1];
        const auto size_1 = _mat.outerIndexPtr()[index_1+1] - outer_1;
        const Eigen::Map<const vec_sp_index_t> inner_1(
            _mat.innerIndexPtr() + outer_1, size_1
        );
        const Eigen::Map<const vec_sp_value_t> value_1(
            _mat.valuePtr() + outer_1, size_1
        );
        const auto mask_1 = _mask.col(index_1_m).transpose().array().template cast<value_t>();
        for (int i2 = 0; i2 <= i1; ++i2) {
            auto index_2 = j+i2;
            const auto index_2_sgn = index_2 / (m * d);
            index_2 -= index_2_sgn * m * d;
            const auto index_2_m = index_2 / d;
            index_2 -= index_2_m * d;
            const auto outer_2 = _mat.outerIndexPtr()[index_2];
            const auto size_2 = _mat.outerIndexPtr()[index_2+1] - outer_2;
            const Eigen::Map<const vec_sp_index_t> inner_2(
                _mat.innerIndexPtr() + outer_2, size_2
            );
            const Eigen::Map<const vec_sp_value_t> value_2(
                _mat.valuePtr() + outer_2, size_2
            );
            const auto mask_2 = _mask.col(index_2_m).transpose().array().template cast<value_t>();

            out(i1, i2) = svsvwdot(
                inner_1, value_1,
                inner_2, value_2,
                ((1-2*index_1_sgn) * (1-2*index_2_sgn)) * sqrt_weights.square() * mask_1 * mask_2
            );
        }
    };
    if (_n_threads <= 1) {
        for (int i1 = 0; i1 < q; ++i1) routine(i1);
    } else {
        #pragma omp parallel for schedule(static) num_threads(_n_threads)
        for (int i1 = 0; i1 < q; ++i1) routine(i1);
    }
    for (int i1 = 0; i1 < q; ++i1) {
        for (int i2 = i1+1; i2 < q; ++i2) {
            out(i1, i2) = out(i2, i1);
        }
    }
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::sq_mul(
    const Eigen::Ref<const vec_value_t>& weights,
    Eigen::Ref<vec_value_t> out
) 
{
    const auto d = _mat.cols();
    const auto m = _mask.cols();
    Eigen::SparseMatrix<value_t, Eigen::ColMajor> mat_sq = _mat.cwiseProduct(_mat);
    // NOTE: MSVC does not like it when we try to capture mat_sq.
    // This is a bug in MSVC (god I hate microsoft so much...).
    const auto routine = [&](int k, const auto& mat_sq) {
        out.segment(k * d, d).matrix() = (
            (weights * _mask.col(k).transpose().array().template cast<value_t>()).matrix()
        ) * mat_sq;
    };
    if (_n_threads <= 1) {
        for (int k = 0; k < m; ++k) routine(k, mat_sq);
    } else {
        #pragma omp parallel for schedule(static) num_threads(_n_threads)
        for (int k = 0; k < m; ++k) routine(k, mat_sq);
    }
    out.tail(m * d) = out.head(m * d);
}

ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE_TP
void
ADELIE_CORE_MATRIX_NAIVE_CONVEX_RELU_SPARSE::sp_tmul(
    const sp_mat_value_t& v, 
    Eigen::Ref<rowmat_value_t> out
) 
{
    base_t::check_sp_tmul(
        v.rows(), v.cols(), out.rows(), out.cols(), rows(), cols()
    );
    const auto routine = [&](int k) {
        typename sp_mat_value_t::InnerIterator it(v, k);
        auto out_k = out.row(k);
        out_k.setZero();
        for (; it; ++it) {
            _ctmul(it.index(), it.value(), out_k, 1);
        }
    };
    if (_n_threads <= 1) {
        for (int k = 0; k < v.outerSize(); ++k) routine(k);
    } else {
        #pragma omp parallel for schedule(static) num_threads(_n_threads)
        for (int k = 0; k < v.outerSize(); ++k) routine(k);
    }
}

} // namespace matrix
} // namespace adelie_core 