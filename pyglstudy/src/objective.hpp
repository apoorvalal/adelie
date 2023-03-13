#pragma once
#include <pybind11/pybind11.h>
#include <pybind11/eigen.h>
#include <pybind11/stl.h>
#include <Eigen/Core>
#include <objective.hpp>

namespace py = pybind11;
using namespace pybind11::literals; // to bring in the `_a` literal

static double compute_h_min(
    const Eigen::Ref<Eigen::VectorXd>& vbuffer1,
    const Eigen::Ref<Eigen::VectorXd>& v,
    double l1
) 
{
    return glstudy::compute_h_min(vbuffer1, v, l1); 
}

static py::dict compute_h_max(
    const Eigen::Ref<Eigen::VectorXd>& vbuffer1,
    const Eigen::Ref<Eigen::VectorXd>& v,
    double l1,
    double zero_tol=1e-10
)
{
    const auto out = glstudy::compute_h_max(vbuffer1, v, l1, zero_tol); 
    py::dict d("h_max"_a=std::get<0>(out), "vbuffer1_min_nzn"_a=std::get<1>(out));
    return d;
}

static double block_norm_objective(
    double h,
    const Eigen::Ref<Eigen::VectorXd>& D,
    const Eigen::Ref<Eigen::VectorXd>& v,
    double l1
)
{
    return glstudy::block_norm_objective(h, D, v, l1);
}