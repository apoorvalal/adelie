#pragma once
#include <adelie_core/matrix/matrix_naive_kronecker_eye.hpp>
#include <adelie_core/matrix/utils.hpp>

namespace adelie_core {
namespace matrix {

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_TP 
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE::MatrixNaiveKroneckerEye(
    base_t& mat,
    size_t K,
    size_t n_threads
): 
    _mat(&mat),
    _K(K),
    _n_threads(n_threads),
    _buff(3 * mat.rows() + mat.cols())
{
    if (K < 1) {
        throw util::adelie_core_error("K must be >= 1.");
    }
    if (n_threads < 1) {
        throw util::adelie_core_error("n_threads must be >= 1.");
    }
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_TP 
typename ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE::value_t
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE::cmul(
    int j, 
    const Eigen::Ref<const vec_value_t>& v,
    const Eigen::Ref<const vec_value_t>& weights
) 
{
    base_t::check_cmul(j, v.size(), weights.size(), rows(), cols());
    Eigen::Map<const rowmat_value_t> V(v.data(), rows() / _K, _K);
    Eigen::Map<const rowmat_value_t> W(weights.data(), V.rows(), V.cols());
    int i = j / _K;
    int l = j - _K * i;
    Eigen::Map<vec_value_t> _v(_buff.data(), V.rows());
    Eigen::Map<vec_value_t> _w(_buff.data() + V.rows(), V.rows());
    dvveq(_v, V.col(l), _n_threads);
    dvveq(_w, W.col(l), _n_threads);
    return _mat->cmul(i, _v, _w);
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_TP 
void
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE::ctmul(
    int j, 
    value_t v, 
    Eigen::Ref<vec_value_t> out
)
{
    base_t::check_ctmul(j, out.size(), rows(), cols());
    Eigen::Map<rowmat_value_t> Out(out.data(), rows() / _K, _K);
    int i = j / _K;
    int l = j - _K * i;
    Eigen::Map<vec_value_t> _out(_buff.data(), Out.rows());
    dvzero(_out, _n_threads);
    _mat->ctmul(i, v, _out);
    auto Out_l = Out.col(l).array();
    dvaddi(Out_l, _out, _n_threads);
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_TP 
void
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE::bmul(
    int j, int q, 
    const Eigen::Ref<const vec_value_t>& v, 
    const Eigen::Ref<const vec_value_t>& weights,
    Eigen::Ref<vec_value_t> out
)
{
    base_t::check_bmul(j, q, v.size(), weights.size(), out.size(), rows(), cols());
    Eigen::Map<const rowmat_value_t> V(v.data(), rows() / _K, _K);
    Eigen::Map<const rowmat_value_t> W(weights.data(), V.rows(), V.cols());
    Eigen::Map<vec_value_t> _v(_buff.data(), V.rows());
    Eigen::Map<vec_value_t> _w(_buff.data() + V.rows(), V.rows());
    for (int l = 0; l < static_cast<int>(_K); ++l) {
        const auto j_l = std::max(j-l, 0);
        const auto i_begin = j_l / static_cast<int>(_K) + ((j_l % _K) != 0);
        if (j-l+q <= 0) continue;
        const auto i_end = (j-l+q-1) / static_cast<int>(_K) + 1;
        const auto i_q = i_end - i_begin;
        if (i_q <= 0) continue;
        dvveq(_v, V.col(l), _n_threads);
        dvveq(_w, W.col(l), _n_threads);
        Eigen::Map<vec_value_t> _out(_buff.data() + 2 * V.rows(), i_q);
        _mat->bmul(i_begin, i_q, _v, _w, _out);
        Eigen::Map<rowmat_value_t> Out(out.data(), out.size() / _K, _K);
        auto Out_curr = Out.col(l-j).segment(i_begin, i_q);
        dvveq(Out_curr, _out, _n_threads);
    }
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_TP 
void
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE::btmul(
    int j, int q, 
    const Eigen::Ref<const vec_value_t>& v, 
    Eigen::Ref<vec_value_t> out
)
{
    base_t::check_btmul(j, q, v.size(), out.size(), rows(), cols());
    Eigen::Map<rowmat_value_t> Out(out.data(), rows() / _K, _K);
    for (int l = 0; l < static_cast<int>(_K); ++l) {
        const auto j_l = std::max(j-l, 0);
        const auto i_begin = j_l / static_cast<int>(_K) + ((j_l % _K) != 0);
        if (j-l+q <= 0) continue;
        const auto i_end = (j-l+q-1) / static_cast<int>(_K) + 1;
        const auto i_q = i_end - i_begin;
        if (i_q <= 0) continue;
        Eigen::Map<vec_value_t> _v(_buff.data(), i_q);
        Eigen::Map<const rowmat_value_t> V(v.data(), v.size() / _K, _K);
        dvveq(_v, V.col(l-j).segment(i_begin, i_q), _n_threads);
        Eigen::Map<vec_value_t> _out(_buff.data() + i_q, Out.rows());
        dvzero(_out, _n_threads);
        _mat->btmul(i_begin, i_q, _v, _out);
        auto Out_l = Out.col(l).array();
        dvaddi(Out_l, _out, _n_threads);
    }
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_TP 
void
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE::mul(
    const Eigen::Ref<const vec_value_t>& v, 
    const Eigen::Ref<const vec_value_t>& weights,
    Eigen::Ref<vec_value_t> out
)
{
    Eigen::Map<const rowmat_value_t> V(v.data(), rows() / _K, _K);
    Eigen::Map<const rowmat_value_t> W(weights.data(), V.rows(), V.cols());
    Eigen::Map<vec_value_t> _v(_buff.data(), V.rows());
    Eigen::Map<vec_value_t> _w(_buff.data() + V.rows(), V.rows());
    const auto p = _mat->cols();
    for (int l = 0; l < static_cast<int>(_K); ++l) {
        dvveq(_v, V.col(l), _n_threads);
        dvveq(_w, W.col(l), _n_threads);
        Eigen::Map<vec_value_t> _out(_buff.data() + 2 * V.rows(), p);
        _mat->mul(_v, _w, _out);
        Eigen::Map<rowmat_value_t> Out(out.data(), out.size() / _K, _K);
        auto Out_l = Out.col(l).array();
        dvveq(Out_l, _out, _n_threads);
    }
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_TP 
void
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE::cov(
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
    Eigen::Map<const rowmat_value_t> sqrt_W(sqrt_weights.data(), rows() / _K, _K);
    out.setZero();
    for (int l = 0; l < static_cast<int>(_K); ++l) {
        const auto j_l = std::max(j-l, 0);
        const auto i_begin = j_l / static_cast<int>(_K) + ((j_l % _K) != 0);
        if (j-l+q <= 0) continue;
        const auto i_end = (j-l+q-1) / static_cast<int>(_K) + 1;
        const auto i_q = i_end - i_begin;
        if (i_q <= 0) continue;
        if (_buff.size() < sqrt_W.rows() + i_q * i_q) {
            _buff.resize(_buff.size() + i_q * i_q);
        }
        Eigen::Map<vec_value_t> _sqrt_weights(_buff.data(), sqrt_W.rows());
        dvveq(_sqrt_weights, sqrt_W.col(l), _n_threads);
        Eigen::Map<colmat_value_t> _out(
            _buff.data() + _sqrt_weights.size(),
            i_q, i_q
        );
        Eigen::Map<colmat_value_t> _buffer(
            buffer.data(),
            _mat->rows(),
            i_q
        );
        _mat->cov(i_begin, i_q, _sqrt_weights, _out, _buffer);
        for (int i1 = 0; i1 < i_q; ++i1) {
            for (int i2 = 0; i2 < i_q; ++i2) {
                out((i1+i_begin)*_K+l-j, (i2+i_begin)*_K+l-j) = _out(i1, i2);
            }
        }
    }
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_TP 
int
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE::rows() const 
{ 
    return _K * _mat->rows(); 
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_TP 
void
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE::sq_mul(
    const Eigen::Ref<const vec_value_t>& weights,
    Eigen::Ref<vec_value_t> out
)
{
    Eigen::Map<const rowmat_value_t> W(weights.data(), rows() / _K, _K);
    Eigen::Map<vec_value_t> _w(_buff.data() + W.rows(), W.rows());
    const auto p = _mat->cols();
    for (int l = 0; l < static_cast<int>(_K); ++l) {
        dvveq(_w, W.col(l), _n_threads);
        Eigen::Map<vec_value_t> _out(_buff.data() + 2 * W.rows(), p);
        _mat->sq_mul(_w, _out);
        Eigen::Map<rowmat_value_t> Out(out.data(), out.size() / _K, _K);
        auto Out_l = Out.col(l).array();
        dvveq(Out_l, _out, _n_threads);
    }
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_TP 
int
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE::cols() const 
{ 
    return _K * _mat->cols(); 
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_TP 
void
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE::sp_tmul(
    const sp_mat_value_t& v,
    Eigen::Ref<rowmat_value_t> out
)
{
    base_t::check_sp_tmul(
        v.rows(), v.cols(), out.rows(), out.cols(), rows(), cols()
    );

    std::vector<int> outers(v.outerSize()+1); 
    outers[0] = 0;
    std::vector<int> inners;
    std::vector<value_t> values;
    inners.reserve(v.nonZeros());
    values.reserve(v.nonZeros());

    rowmat_value_t _out(out.rows(), rows() / _K);

    for (int l = 0; l < static_cast<int>(_K); ++l) {
        inners.clear();
        values.clear();

        // populate current v
        for (int k = 0; k < v.outerSize(); ++k) {
            typename sp_mat_value_t::InnerIterator it(v, k);
            int n_read = 0;
            for (; it; ++it) {
                if (it.index() % _K != static_cast<size_t>(l)) continue;
                inners.push_back(it.index() / _K);
                values.push_back(it.value());
                ++n_read;
            }
            outers[k+1] = outers[k] + n_read;
        }
        Eigen::Map<sp_mat_value_t> _v(
            v.rows(),
            v.cols() / _K,
            inners.size(),
            outers.data(),
            inners.data(),
            values.data()
        );

        _mat->sp_tmul(_v, _out);

        const auto routine = [&](int k) {
            Eigen::Map<rowmat_value_t> out_k(
                out.row(k).data(), _out.cols(), _K                
            );
            out_k.col(l) = _out.row(k);
        };
        if (_n_threads <= 1) {
            for (int k = 0; k < out.rows(); ++k) routine(k);
        } else {
            #pragma omp parallel for schedule(static) num_threads(_n_threads)
            for (int k = 0; k < out.rows(); ++k) routine(k);
        }
    }
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE_TP 
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE::MatrixNaiveKroneckerEyeDense(
    const Eigen::Ref<const dense_t>& mat,
    size_t K,
    size_t n_threads
): 
    _mat(mat.data(), mat.rows(), mat.cols()),
    _K(K),
    _n_threads(n_threads),
    _buff(_n_threads, K),
    _vbuff(mat.rows() * K)
{
    if (K < 1) {
        throw util::adelie_core_error("K must be >= 1.");
    }
    if (n_threads < 1) {
        throw util::adelie_core_error("n_threads must be >= 1.");
    }
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE_TP 
typename ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE::value_t
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE::cmul(
    int j, 
    const Eigen::Ref<const vec_value_t>& v,
    const Eigen::Ref<const vec_value_t>& weights
) 
{
    base_t::check_cmul(j, v.size(), weights.size(), rows(), cols());
    Eigen::Map<const rowmat_value_t> V(v.data(), rows() / _K, _K);
    Eigen::Map<const rowmat_value_t> W(weights.data(), V.rows(), V.cols());
    const int i = j / _K;
    const int l = j - _K * i;
    Eigen::Map<vec_value_t> vbuff(_buff.data(), _n_threads);
    return ddot(V.col(l).cwiseProduct(W.col(l)), _mat.col(i), _n_threads, vbuff);
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE_TP 
void
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE::ctmul(
    int j, 
    value_t v, 
    Eigen::Ref<vec_value_t> out
)
{
    base_t::check_ctmul(j, out.size(), rows(), cols());
    Eigen::Map<rowmat_value_t> Out(out.data(), rows() / _K, _K);
    const int i = j / _K;
    const int l = j - _K * i;
    auto _out = Out.col(l);
    dvaddi(_out, v * _mat.col(i), _n_threads);
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE_TP 
void
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE::bmul(
    int j, int q, 
    const Eigen::Ref<const vec_value_t>& v, 
    const Eigen::Ref<const vec_value_t>& weights,
    Eigen::Ref<vec_value_t> out
)
{
    base_t::check_bmul(j, q, v.size(), weights.size(), out.size(), rows(), cols());
    dvveq(_vbuff, v * weights, _n_threads);
    Eigen::Map<const rowmat_value_t> VW(_vbuff.data(), rows() / _K, _K);
    int n_processed = 0;
    while (n_processed < q) {
        const int i = (j + n_processed) / _K;
        const int l = (j + n_processed) - _K * i;
        const int size = std::min<int>(_K-l, q-n_processed);
        auto _out = out.segment(n_processed, size).matrix();
        dgemv(
            VW.middleCols(l, size), 
            _mat.col(i).transpose(), 
            _n_threads, 
            _buff, 
            _out
        );
        n_processed += size;
    }
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE_TP 
void
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE::btmul(
    int j, int q, 
    const Eigen::Ref<const vec_value_t>& v, 
    Eigen::Ref<vec_value_t> out
)
{
    base_t::check_btmul(j, q, v.size(), out.size(), rows(), cols());
    Eigen::Map<rowmat_value_t> Out(out.data(), rows() / _K, _K);
    int n_processed = 0;
    while (n_processed < q) {
        const int i = (j + n_processed) / _K;
        const int l = (j + n_processed) - _K * i;
        const int size = std::min<int>(_K-l, q-n_processed);
        Eigen::Map<const vec_value_t> _v(v.data() + n_processed, size);
        for (int j = 0; j < _v.size(); ++j) {
            auto Out_curr = Out.col(l+j);
            dvaddi(Out_curr, _v[j] * _mat.col(i), _n_threads);
        }
        n_processed += size;
    }
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE_TP 
void
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE::mul(
    const Eigen::Ref<const vec_value_t>& v, 
    const Eigen::Ref<const vec_value_t>& weights,
    Eigen::Ref<vec_value_t> out
)
{
    base_t::check_bmul(0, cols(), v.size(), weights.size(), out.size(), rows(), cols());
    dvveq(_vbuff, v * weights, _n_threads);
    Eigen::Map<const rowmat_value_t> VW(_vbuff.data(), rows() / _K, _K);
    Eigen::Map<rowmat_value_t> Out(out.data(), cols() / _K, _K);
    Eigen::setNbThreads(_n_threads);
    Out.noalias() = _mat.transpose() * VW;
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE_TP 
void
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE::cov(
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
    Eigen::Map<const rowmat_value_t> sqrt_W(sqrt_weights.data(), rows() / _K, _K);
    out.setZero(); // do NOT parallelize!
    for (int l = 0; l < static_cast<int>(_K); ++l) {
        const auto j_l = std::max(j-l, 0);
        const auto i_begin = j_l / static_cast<int>(_K) + ((j_l % _K) != 0);
        if (j-l+q <= 0) continue;
        const auto i_end = (j-l+q-1) / static_cast<int>(_K) + 1;
        const auto i_q = i_end - i_begin;
        if (i_q <= 0) continue;

        Eigen::Map<colmat_value_t> sqrt_WX(buffer.data(), _mat.rows(), i_q);
        auto sqrt_WX_array = sqrt_WX.array();
        dmmeq(
            sqrt_WX_array,
            _mat.middleCols(i_begin, i_q).array().colwise() * sqrt_W.col(l).array(),
            _n_threads
        );

        if (_vbuff.size() < i_q * i_q) _vbuff.resize(i_q * i_q);
        Eigen::Map<colmat_value_t> XTWX(_vbuff.data(), i_q, i_q);
        XTWX.setZero(); // do NOT parallelize!
        XTWX.template selfadjointView<Eigen::Lower>().rankUpdate(sqrt_WX.transpose());
        for (int i1 = 0; i1 < i_q; ++i1) {
            for (int i2 = 0; i2 <= i1; ++i2) {
                const auto fi1 = (i1+i_begin)*_K+l-j;
                const auto fi2 = (i2+i_begin)*_K+l-j;
                const auto val = XTWX(i1, i2);
                out(fi1, fi2) = val;
                out(fi2, fi1) = val;
            }
        }
    }
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE_TP 
int
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE::rows() const 
{ 
    return _K * _mat.rows(); 
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE_TP 
int
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE::cols() const 
{ 
    return _K * _mat.cols(); 
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE_TP 
void
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE::sq_mul(
    const Eigen::Ref<const vec_value_t>& weights,
    Eigen::Ref<vec_value_t> out
)
{
    Eigen::Map<const rowmat_value_t> W(weights.data(), rows() / _K, _K);
    Eigen::Map<rowmat_value_t> Out(out.data(), cols() / _K, _K);
    Eigen::setNbThreads(_n_threads);
    Out.noalias() = _mat.array().square().matrix().transpose() * W;
}

ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE_TP 
void
ADELIE_CORE_MATRIX_NAIVE_KRONECKER_EYE_DENSE::sp_tmul(
    const sp_mat_value_t& v,
    Eigen::Ref<rowmat_value_t> out
)
{
    base_t::check_sp_tmul(
        v.rows(), v.cols(), out.rows(), out.cols(), rows(), cols()
    );

    std::vector<int> outers(v.outerSize()+1); 
    outers[0] = 0;
    std::vector<int> inners;
    std::vector<value_t> values;
    inners.reserve(v.nonZeros());
    values.reserve(v.nonZeros());

    rowmat_value_t _out(out.rows(), rows() / _K);

    for (int l = 0; l < static_cast<int>(_K); ++l) {
        inners.clear();
        values.clear();

        // populate current v
        for (int k = 0; k < v.outerSize(); ++k) {
            typename sp_mat_value_t::InnerIterator it(v, k);
            int n_read = 0;
            for (; it; ++it) {
                if (it.index() % _K != static_cast<size_t>(l)) continue;
                inners.push_back(it.index() / _K);
                values.push_back(it.value());
                ++n_read;
            }
            outers[k+1] = outers[k] + n_read;
        }
        Eigen::Map<sp_mat_value_t> _v(
            v.rows(),
            v.cols() / _K,
            inners.size(),
            outers.data(),
            inners.data(),
            values.data()
        );

        const auto routine = [&](int k) {
            Eigen::Map<rowmat_value_t> out_k(
                out.row(k).data(), _out.cols(), _K                
            );
            out_k.col(l) = _out.row(k);
        };

        if (_n_threads <= 1) {
            _out.noalias() = _v * _mat.transpose();
            for (int k = 0; k < _v.outerSize(); ++k) routine(k);
            continue;
        }

        const auto outer = _v.outerIndexPtr();
        const auto inner = _v.innerIndexPtr();
        const auto value = _v.valuePtr();
        #pragma omp parallel for schedule(static) num_threads(_n_threads)
        for (int k = 0; k < _v.outerSize(); ++k) {
            const Eigen::Map<const sp_mat_value_t> vk(
                1,
                _v.cols(),
                outer[k+1] - outer[k],
                outer + k,
                inner,
                value
            );
            auto out_k = _out.row(k);
            out_k = vk * _mat.transpose();
            routine(k);
        };
    }
}

} // namespace matrix
} // namespace adelie_core