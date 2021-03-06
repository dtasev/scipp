// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2019 Scipp contributors (https://github.com/scipp)
/// @file
/// @author Simon Heybrock

#include "scipp/neutron/beamline.h"
#include "scipp/core/dataset.h"
#include "scipp/core/transform.h"

using namespace scipp::core;

namespace scipp::neutron {

namespace beamline_impl {
template <class T> static auto component_positions(const T &d) {
  return d.labels()["component_info"]
      .template values<Dataset>()[0]["position"]
      .data();
}

template <class T> static VariableConstProxy position(const T &d) {
  if (d.coords().contains(Dim::Position))
    return d.coords()[Dim::Position];
  else
    return d.labels()["position"];
}

template <class T> static Variable source_position(const T &d) {
  // TODO Need a better mechanism to identify source and sample.
  return Variable(component_positions(d).slice({Dim::Row, 0}));
}

template <class T> static Variable sample_position(const T &d) {
  return Variable(component_positions(d).slice({Dim::Row, 1}));
}

template <class T> static Variable l1(const T &d) {
  return norm(sample_position(d) - source_position(d));
}

template <class T> static Variable l2(const T &d) {
  // Use transform to avoid temporaries. For certain unit conversions this can
  // cause a speedup >50%. Short version would be:
  //   return norm(position(d) - sample_position(d));
  return transform<pair_self_t<Eigen::Vector3d>>(
      position(d), sample_position(d),
      overloaded{
          [](const auto &x, const auto &y) { return (x - y).norm(); },
          [](const units::Unit &x, const units::Unit &y) { return x - y; }});
}

template <class T> static Variable scattering_angle(const T &d) {
  return 0.5 * two_theta(d);
}

template <class T> static Variable two_theta(const T &d) {
  auto beam = sample_position(d) - source_position(d);
  const auto l1 = norm(beam);
  beam /= l1;
  auto scattered = position(d) - sample_position(d);
  const auto l2 = norm(scattered);
  scattered /= l2;

  return acos(dot(beam, scattered));
}
} // namespace beamline_impl

core::VariableConstProxy position(const core::DatasetConstProxy &d) {
  return beamline_impl::position(d);
}
core::Variable source_position(const core::DatasetConstProxy &d) {
  return beamline_impl::source_position(d);
}
core::Variable sample_position(const core::DatasetConstProxy &d) {
  return beamline_impl::sample_position(d);
}
core::Variable l1(const core::DatasetConstProxy &d) {
  return beamline_impl::l1(d);
}
core::Variable l2(const core::DatasetConstProxy &d) {
  return beamline_impl::l2(d);
}
core::Variable scattering_angle(const core::DatasetConstProxy &d) {
  return beamline_impl::scattering_angle(d);
}
core::Variable two_theta(const core::DatasetConstProxy &d) {
  return beamline_impl::two_theta(d);
}

core::VariableConstProxy position(const core::DataConstProxy &d) {
  return beamline_impl::position(d);
}
core::Variable source_position(const core::DataConstProxy &d) {
  return beamline_impl::source_position(d);
}
core::Variable sample_position(const core::DataConstProxy &d) {
  return beamline_impl::sample_position(d);
}
core::Variable l1(const core::DataConstProxy &d) {
  return beamline_impl::l1(d);
}
core::Variable l2(const core::DataConstProxy &d) {
  return beamline_impl::l2(d);
}
core::Variable scattering_angle(const core::DataConstProxy &d) {
  return beamline_impl::scattering_angle(d);
}
core::Variable two_theta(const core::DataConstProxy &d) {
  return beamline_impl::two_theta(d);
}

} // namespace scipp::neutron
