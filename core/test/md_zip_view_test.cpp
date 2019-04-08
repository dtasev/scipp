/// @file
/// SPDX-License-Identifier: GPL-3.0-or-later
/// @author Simon Heybrock
/// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory, NScD Oak Ridge
/// National Laboratory, and European Spallation Source ERIC.
#include <gtest/gtest.h>

#include <boost/mpl/at.hpp>
#include <boost/mpl/sort.hpp>
#include <boost/mpl/vector_c.hpp>

#include "test_macros.h"

#include "event_list_proxy.h"
#include "md_zip_view.h"

using namespace scipp::core;

TEST(MDZipView, construct) {
  Dataset d;
  d.insert(Data::Value, "", {}, {1.1});
  d.insert(Data::Variance, "", {}, {2});
  // Empty view forbidden by static_assert:
  // zipMD(d);
  ASSERT_NO_THROW(static_cast<void>(zipMD(d, MDRead(Data::Value))));
  ASSERT_NO_THROW(static_cast<void>(zipMD(d, MDRead(Data::Variance))));
  ASSERT_NO_THROW(
      static_cast<void>(zipMD(d, MDRead(Data::Variance), MDRead(Data::Value))));
  ASSERT_THROW(
      static_cast<void>(zipMD(d, MDRead(Data::Events), MDRead(Data::Variance))),
      std::runtime_error);
}

TEST(MDZipView, construct_with_const_Dataset) {
  Dataset d;
  d.insert(Data::Value, "", {Dim::X, 1}, {1.1});
  d.insert(Data::Variance, "", {}, {2});
  const auto const_d(d);
  EXPECT_NO_THROW(zipMD(const_d, MDRead(Data::Value)));
  EXPECT_NO_THROW(zipMD(const_d, {Dim::X}, ConstMDNested(MDRead(Data::Value))));
  EXPECT_NO_THROW(zipMD(const_d, {Dim::X}, ConstMDNested(MDRead(Data::Value)),
                        MDRead(Data::Variance)));
}

TEST(MDZipView, iterator) {
  Dataset d;
  d.insert(Data::Value, "", Dimensions{Dim::X, 2}, {1.1, 1.2});
  d.insert(Data::Variance, "", Dimensions{Dim::X, 2}, {2, 3});
  auto view = zipMD(d, MDWrite(Data::Value));
  ASSERT_NO_THROW(view.begin());
  ASSERT_NO_THROW(view.end());
  auto it = view.begin();
  // Note: Cannot assigned dereferenced iterator by value since Dataset::Item
  // should not live outside and iterator.
  // auto item = *it;
  ASSERT_EQ(it->get(Data::Value), 1.1);
  it->get(Data::Value) = 2.2;
  ASSERT_EQ(it->value(), 2.2);
  ASSERT_EQ(it, it);
  ASSERT_EQ(it, view.begin());
  ASSERT_NE(it, view.end());
  ASSERT_NO_THROW(it++);
  ASSERT_NE(it, view.end());
  ASSERT_EQ(it->value(), 1.2);
  ASSERT_NO_THROW(it++);
  ASSERT_EQ(it, view.end());
}

TEST(MDZipView, single_column) {
  Dataset d;
  d.insert(Data::Value, "", Dimensions(Dim::Tof, 10), 10);
  d.insert(Data::Variance, "", Dimensions(Dim::Tof, 10), 10);
  auto var = d.get(Data::Value);
  var[0] = 0.2;
  var[3] = 3.2;

  auto view = zipMD(d, MDWrite(Data::Value));
  auto it = view.begin();
  ASSERT_EQ(it->get(Data::Value), 0.2);
  it++;
  ASSERT_EQ(it->get(Data::Value), 0.0);
  it++;
  ASSERT_EQ(it->get(Data::Value), 0.0);
  it++;
  ASSERT_EQ(it->get(Data::Value), 3.2);
  it += 7;
  ASSERT_EQ(it, view.end());
}

TEST(MDZipView, multi_column) {
  Dataset d;
  d.insert(Data::Value, "", Dimensions(Dim::Tof, 2), 2);
  d.insert(Data::Variance, "", Dimensions(Dim::Tof, 2), 2);
  auto var = d.get(Data::Value);
  var[0] = 0.2;
  var[1] = 3.2;

  auto view = zipMD(d, MDRead(Data::Value), MDRead(Data::Variance));
  auto it = view.begin();
  ASSERT_EQ(it->get(Data::Value), 0.2);
  ASSERT_EQ(it->get(Data::Variance), 0);
  it++;
  ASSERT_EQ(it->get(Data::Value), 3.2);
  ASSERT_EQ(it->get(Data::Variance), 0);
}

TEST(MDZipView, multi_column_mixed_dimension) {
  Dataset d;
  d.insert(Data::Value, "", Dimensions(Dim::Tof, 2), 2);
  d.insert(Data::Variance, "", {}, 1);
  auto var = d.get(Data::Value);
  var[0] = 0.2;
  var[1] = 3.2;

  ASSERT_ANY_THROW(zipMD(d, MDWrite(Data::Value), MDWrite(Data::Variance)));
  ASSERT_NO_THROW(zipMD(d, MDWrite(Data::Value), MDRead(Data::Variance)));
  auto view = zipMD(d, MDWrite(Data::Value), MDRead(Data::Variance));
  auto it = view.begin();
  ASSERT_EQ(it->get(Data::Value), 0.2);
  ASSERT_EQ(it->get(Data::Variance), 0);
  it++;
  ASSERT_EQ(it->get(Data::Value), 3.2);
  ASSERT_EQ(it->get(Data::Variance), 0);
}

TEST(MDZipView, multi_column_transposed) {
  Dataset d;
  Dimensions dimsXY;
  dimsXY.add(Dim::X, 2);
  dimsXY.add(Dim::Y, 3);
  Dimensions dimsYX;
  dimsYX.add(Dim::Y, 3);
  dimsYX.add(Dim::X, 2);

  d.insert(Data::Value, "", dimsXY, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
  d.insert(Data::Variance, "", dimsYX, {1, 3, 5, 2, 4, 6});
  // TODO Current dimension check is too strict and fails unless data with
  // transposed dimensions is accessed as const.
  auto view = zipMD(d, MDWrite(Data::Value), MDRead(Data::Variance));
  auto it = view.begin();
  ASSERT_NE(++it, view.end());
  ASSERT_EQ(it->get(Data::Value), 2.0);
  ASSERT_EQ(it->get(Data::Variance), 2);
  for (const auto &item : view)
    ASSERT_EQ(item.get(Data::Value), item.get(Data::Variance));
}

TEST(MDZipView, multi_column_unrelated_dimension) {
  Dataset d;
  d.insert(Data::Value, "", Dimensions(Dim::X, 2), 2);
  d.insert(Data::Variance, "", Dimensions(Dim::Y, 3), 3);
  auto view = zipMD(d, MDWrite(Data::Value));
  auto it = view.begin();
  ASSERT_TRUE(it < view.end());
  it += 2;
  // We iterate only Data::Value, so there should be no iteration in
  // Dim::Y.
  ASSERT_EQ(it, view.end());
}

TEST(MDZipView, multi_column_orthogonal_fail) {
  Dataset d;
  d.insert(Data::Value, "", Dimensions(Dim::X, 2), 2);
  d.insert(Data::Variance, "", Dimensions(Dim::Y, 3), 3);
  EXPECT_THROW_MSG(zipMD(d, MDRead(Data::Value), MDRead(Data::Variance)),
                   std::runtime_error,
                   "Variables requested for iteration do not span a joint "
                   "space. In case one of the variables represents bin edges "
                   "direct joint iteration is not possible. Use the Bin<> "
                   "wrapper to iterate over bins defined by edges instead.");
}

TEST(MDZipView, nested_MDZipView) {
  Dataset d;
  d.insert(Data::Value, "", {{Dim::Y, 3}, {Dim::X, 2}},
           {1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
  d.insert(Data::Variance, "", {Dim::X, 2}, {10, 20});
  auto nested = MDNested(MDRead(Data::Value));
  // TODO This is a very bad way of indexing nested views.
  auto Nested = decltype(nested)::type(d);
  ;
  auto view = zipMD(d, {Dim::Y}, nested, MDRead(Data::Variance));
  ASSERT_EQ(view.size(), 2);
  double base = 0.0;
  for (const auto &item : view) {
    auto subview = item.get(Nested);
    ASSERT_EQ(subview.size(), 3);
    auto it = subview.begin();
    EXPECT_EQ(it++->get(Data::Value), base + 1.0);
    EXPECT_EQ(it++->get(Data::Value), base + 3.0);
    EXPECT_EQ(it++->get(Data::Value), base + 5.0);
    base += 1.0;
  }
}

TEST(MDZipView, nested_MDZipView_all_subdimension_combinations_3D) {
  Dataset d;
  d.insert(Data::Value, "", Dimensions({{Dim::Z, 2}, {Dim::Y, 3}, {Dim::X, 4}}),
           {1.0,  2.0,  3.0,  4.0,  5.0,  6.0,  7.0,  8.0,
            9.0,  10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0,
            17.0, 18.0, 19.0, 20.0, 21.0, 22.0, 23.0, 24.0});

  auto nested = zipMD(d, MDRead(Data::Value));
  auto viewX = zipMD(d, {Dim::Y, Dim::Z}, MDWrite(nested));
  ASSERT_EQ(viewX.size(), 4);
  double base = 0.0;
  for (const auto &item : viewX) {
    auto subview = item.get(nested);
    ASSERT_EQ(subview.size(), 6);
    auto it = subview.begin();
    EXPECT_EQ(it++->get(Data::Value), base + 1.0);
    EXPECT_EQ(it++->get(Data::Value), base + 5.0);
    EXPECT_EQ(it++->get(Data::Value), base + 9.0);
    EXPECT_EQ(it++->get(Data::Value), base + 13.0);
    EXPECT_EQ(it++->get(Data::Value), base + 17.0);
    EXPECT_EQ(it++->get(Data::Value), base + 21.0);
    base += 1.0;
  }

  auto viewY = zipMD(d, {Dim::X, Dim::Z}, MDWrite(nested));
  ASSERT_EQ(viewY.size(), 3);
  base = 0.0;
  for (const auto &item : viewY) {
    auto subview = item.get(nested);
    ASSERT_EQ(subview.size(), 8);
    auto it = subview.begin();
    EXPECT_EQ(it++->get(Data::Value), base + 1.0);
    EXPECT_EQ(it++->get(Data::Value), base + 2.0);
    EXPECT_EQ(it++->get(Data::Value), base + 3.0);
    EXPECT_EQ(it++->get(Data::Value), base + 4.0);
    EXPECT_EQ(it++->get(Data::Value), base + 13.0);
    EXPECT_EQ(it++->get(Data::Value), base + 14.0);
    EXPECT_EQ(it++->get(Data::Value), base + 15.0);
    EXPECT_EQ(it++->get(Data::Value), base + 16.0);
    base += 4.0;
  }

  auto viewZ = zipMD(d, {Dim::X, Dim::Y}, MDWrite(nested));
  ASSERT_EQ(viewZ.size(), 2);
  base = 0.0;
  for (const auto &item : viewZ) {
    auto subview = item.get(nested);
    ASSERT_EQ(subview.size(), 12);
    auto it = subview.begin();
    EXPECT_EQ(it++->get(Data::Value), base + 1.0);
    EXPECT_EQ(it++->get(Data::Value), base + 2.0);
    EXPECT_EQ(it++->get(Data::Value), base + 3.0);
    EXPECT_EQ(it++->get(Data::Value), base + 4.0);
    EXPECT_EQ(it++->get(Data::Value), base + 5.0);
    EXPECT_EQ(it++->get(Data::Value), base + 6.0);
    EXPECT_EQ(it++->get(Data::Value), base + 7.0);
    EXPECT_EQ(it++->get(Data::Value), base + 8.0);
    EXPECT_EQ(it++->get(Data::Value), base + 9.0);
    EXPECT_EQ(it++->get(Data::Value), base + 10.0);
    EXPECT_EQ(it++->get(Data::Value), base + 11.0);
    EXPECT_EQ(it++->get(Data::Value), base + 12.0);
    base += 12.0;
  }

  auto viewYZ = zipMD(d, {Dim::X}, MDWrite(nested));
  ASSERT_EQ(viewYZ.size(), 6);
  base = 0.0;
  for (const auto &item : viewYZ) {
    auto subview = item.get(nested);
    ASSERT_EQ(subview.size(), 4);
    auto it = subview.begin();
    EXPECT_EQ(it++->get(Data::Value), base + 1.0);
    EXPECT_EQ(it++->get(Data::Value), base + 2.0);
    EXPECT_EQ(it++->get(Data::Value), base + 3.0);
    EXPECT_EQ(it++->get(Data::Value), base + 4.0);
    base += 4.0;
  }

  auto viewXZ = zipMD(d, {Dim::Y}, MDWrite(nested));
  ASSERT_EQ(viewXZ.size(), 8);
  base = 0.0;
  for (const auto &item : viewXZ) {
    auto subview = item.get(nested);
    ASSERT_EQ(subview.size(), 3);
    auto it = subview.begin();
    EXPECT_EQ(it++->get(Data::Value), base + 1.0);
    EXPECT_EQ(it++->get(Data::Value), base + 5.0);
    EXPECT_EQ(it++->get(Data::Value), base + 9.0);
    base += 1.0;
    // Jump to next Z
    if (base == 4.0)
      base += 8.0;
  }

  auto viewXY = zipMD(d, {Dim::Z}, MDWrite(nested));
  ASSERT_EQ(viewXY.size(), 12);
  base = 0.0;
  for (const auto &item : viewXY) {
    auto subview = item.get(nested);
    ASSERT_EQ(subview.size(), 2);
    auto it = subview.begin();
    EXPECT_EQ(it++->get(Data::Value), base + 1.0);
    EXPECT_EQ(it++->get(Data::Value), base + 13.0);
    base += 1.0;
  }
}

TEST(MDZipView, nested_MDZipView_constant_variable) {
  Dataset d;
  d.insert(Data::Value, "", Dimensions({{Dim::Z, 2}, {Dim::X, 4}}),
           {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0});
  d.insert(Coord::X, {Dim::X, 4}, {10.0, 20.0, 30.0, 40.0});

  // Coord::X has fewer dimensions, throws if not const when not nested...
  EXPECT_THROW_MSG(
      zipMD(d, MDRead(Data::Value), MDWrite(Coord::X)), std::runtime_error,
      "Variables requested for iteration have different dimensions");
  // ... and also when nested.
  // TODO Dim::Z is just a dummy so we can create the nested label without
  // failing, it is actually ignored.
  auto nested = zipMD(d, {Dim::Z}, MDRead(Data::Value), MDWrite(Coord::X));
  EXPECT_THROW_MSG(
      zipMD(d, {Dim::X}, MDWrite(nested)), std::runtime_error,
      "Variables requested for iteration have different dimensions");

  auto goodNested = zipMD(d, MDRead(Data::Value), MDRead(Coord::X));
  auto view = zipMD(d, {Dim::X}, MDWrite(goodNested));
  ASSERT_EQ(view.size(), 2);
  double value = 0.0;
  for (const auto &item : view) {
    auto subview = item.get(goodNested);
    ASSERT_EQ(subview.size(), 4);
    double x = 0.0;
    for (const auto &subitem : subview) {
      x += 10.0;
      value += 1.0;
      EXPECT_EQ(subitem.get(Coord::X), x);
      EXPECT_EQ(subitem.get(Data::Value), value);
    }
  }
}

TEST(MDZipView, histogram_using_nested_MDZipView) {
  Dataset d;
  // Edges do not have Dim::Spectrum, "shared" by all histograms.
  d.insert(Coord::Tof, Dimensions(Dim::Tof, 3), {10.0, 20.0, 30.0});
  Dimensions dims;
  dims.add(Dim::Tof, 2);
  dims.add(Dim::Spectrum, 4);
  d.insert(Data::Value, "sample", dims,
           {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0});
  d.insert(Data::Variance, "sample", dims, 8);
  d.insert(Coord::SpectrumNumber, {Dim::Spectrum, 4}, {1, 2, 3, 4});

  auto nested = MDNested(MDRead(Bin<decltype(Coord::Tof)>{}),
                         MDWrite(Data::Value, "sample"),
                         MDWrite(Data::Variance, "sample"));
  // TODO Nested access needs to change!
  auto HistogramView = decltype(nested)::type(d, "sample", {Dim::Spectrum});
  auto view = zipMD(d, {Dim::Tof}, nested, MDWrite(Coord::SpectrumNumber));

  EXPECT_EQ(view.size(), 4);
  int32_t specNum = 1;
  double value = 1.0;
  for (const auto &item : view) {
    EXPECT_EQ(item.get(Coord::SpectrumNumber), specNum++);
    auto histview = item.get(HistogramView);
    EXPECT_EQ(histview.size(), 2);
    double edge = 10.0;
    for (const auto &bin : histview) {
      EXPECT_EQ(bin.left(), edge);
      EXPECT_EQ(bin.right(), edge + 10.0);
      edge += 10.0;
      EXPECT_EQ(bin.value(), value++);
    }
  }

  auto it = view.begin();
  auto histogram = it->get(HistogramView);
  EXPECT_EQ(histogram.size(), 2);
  auto bin = histogram.begin();
  EXPECT_EQ(bin->value(), 1.0);
  ++bin;
  EXPECT_EQ(bin->value(), 2.0);
  bin->value() += 0.2;
  EXPECT_EQ(d.get(Data::Value, "sample")[1], 2.2);
  it++;
  EXPECT_EQ(it->get(HistogramView).begin()->value(), 3.0);
}

TEST(MDZipView, single_column_edges) {
  Dataset d;
  d.insert(Coord::Tof, Dimensions(Dim::Tof, 3), 3);
  d.insert(Data::Value, "name2", Dimensions(Dim::Tof, 2), 2);
  auto var = d.get(Coord::Tof);
  ASSERT_EQ(var.size(), 3);
  var[0] = 0.2;
  var[2] = 2.2;

  auto view = zipMD(d, MDRead(Coord::Tof));
  auto it = view.begin();
  ASSERT_LT(it, view.end());
  ASSERT_EQ(it->get(Coord::Tof), 0.2);
  it++;
  ASSERT_LT(it, view.end());
  ASSERT_EQ(it->get(Coord::Tof), 0.0);
  ASSERT_LT(it, view.end());
  it++;
  ASSERT_EQ(it->get(Coord::Tof), 2.2);
  ASSERT_LT(it, view.end());
  it++;
  ASSERT_EQ(it, view.end());
}

TEST(MDZipView, single_column_bins) {
  Dataset d;
  d.insert(Coord::Tof, Dimensions(Dim::Tof, 3), 3);
  d.insert(Data::Value, "name2", Dimensions(Dim::Tof, 2), 2);
  auto var = d.get(Coord::Tof);
  ASSERT_EQ(var.size(), 3);
  var[0] = 0.2;
  var[1] = 1.2;
  var[2] = 2.2;

  auto view = zipMD(d, MDRead(Bin<decltype(Coord::Tof)>{}));
  auto it = view.begin();
  it++;
  ASSERT_NE(it, view.end());
  it++;
  // Lenth of edges is 3, but there are only 2 bins!
  ASSERT_EQ(it, view.end());
}

TEST(MDZipView, multi_column_edges) {
  Dataset d;
  d.insert(Coord::Tof, Dimensions(Dim::Tof, 3), 3);
  d.insert(Data::Value, "", Dimensions(Dim::Tof, 2), 2);
  auto var = d.get(Coord::Tof);
  var[0] = 0.2;
  var[1] = 1.2;
  var[2] = 2.2;

  // Cannot simultaneously iterate edges and non-edges, so this throws.
  EXPECT_THROW_MSG(zipMD(d, MDRead(Coord::Tof), MDRead(Data::Value)),
                   std::runtime_error,
                   "Variables requested for iteration do not span a joint "
                   "space. In case one of the variables represents bin edges "
                   "direct joint iteration is not possible. Use the Bin<> "
                   "wrapper to iterate over bins defined by edges instead.");

  auto view =
      zipMD(d, MDRead(Bin<decltype(Coord::Tof)>{}), MDWrite(Data::Value));
  // TODO What are good names for named getters? tofCenter(), etc.?
  const auto &bin = view.begin()->get(Bin<decltype(Coord::Tof)>{});
  EXPECT_EQ(bin.center(), 0.7);
  EXPECT_EQ(bin.width(), 1.0);
  EXPECT_EQ(bin.left(), 0.2);
  EXPECT_EQ(bin.right(), 1.2);
}

TEST(MDZipView, multi_dimensional_edges) {
  Dataset d;
  d.insert(Coord::X, Dimensions({{Dim::Y, 2}, {Dim::X, 3}}),
           {1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
  // TODO There is currently a bug in MDZipView: If `Bin` iteration is
  // requested but the dataset contains only edges the shape calculation gives
  // wrong results.
  d.insert(Data::Value, "", {Dim::X, 2});

  auto view = zipMD(d, MDRead(Bin<decltype(Coord::X)>{}));
  ASSERT_EQ(view.size(), 4);
  auto it = view.begin();
  EXPECT_EQ(it++->get(Bin<decltype(Coord::X)>{}).left(), 1.0);
  EXPECT_EQ(it++->get(Bin<decltype(Coord::X)>{}).left(), 2.0);
  EXPECT_EQ(it++->get(Bin<decltype(Coord::X)>{}).left(), 4.0);
  EXPECT_EQ(it++->get(Bin<decltype(Coord::X)>{}).left(), 5.0);
  it -= 4;
  EXPECT_EQ(it++->get(Bin<decltype(Coord::X)>{}).right(), 2.0);
  EXPECT_EQ(it++->get(Bin<decltype(Coord::X)>{}).right(), 3.0);
  EXPECT_EQ(it++->get(Bin<decltype(Coord::X)>{}).right(), 5.0);
  EXPECT_EQ(it++->get(Bin<decltype(Coord::X)>{}).right(), 6.0);
}

TEST(MDZipView, edges_are_not_inner_dimension) {
  Dataset d;
  d.insert(Coord::Y, Dimensions({{Dim::Y, 2}, {Dim::X, 3}}),
           {1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
  d.insert(Data::Value, "", {Dim::Y, 1});

  auto view = zipMD(d, MDRead(Bin<decltype(Coord::Y)>{}));
  ASSERT_EQ(view.size(), 3);
  auto it = view.begin();
  EXPECT_EQ(it++->get(Bin<decltype(Coord::Y)>{}).left(), 1.0);
  EXPECT_EQ(it++->get(Bin<decltype(Coord::Y)>{}).left(), 2.0);
  EXPECT_EQ(it++->get(Bin<decltype(Coord::Y)>{}).left(), 3.0);
  it -= 3;
  EXPECT_EQ(it++->get(Bin<decltype(Coord::Y)>{}).right(), 4.0);
  EXPECT_EQ(it++->get(Bin<decltype(Coord::Y)>{}).right(), 5.0);
  EXPECT_EQ(it++->get(Bin<decltype(Coord::Y)>{}).right(), 6.0);
}

TEST(MDZipView, named_getter) {
  Dataset d;
  d.insert(Coord::Tof, Dimensions(Dim::Tof, 3), 3);
  auto var = d.get(Coord::Tof);
  ASSERT_EQ(var.size(), 3);
  var[0] = 0.2;
  var[2] = 2.2;

  auto view = zipMD(d, MDRead(Coord::Tof));
  auto it = view.begin();
  ASSERT_EQ(it->tof(), 0.2);
  it++;
  ASSERT_EQ(it->tof(), 0.0);
  it++;
  ASSERT_EQ(it->tof(), 2.2);
}

TEST(MDZipView, duplicate_data_tag) {
  Dataset d;
  d.insert(Data::Value, "name1", {}, 1);
  d.insert(Data::Value, "name2", {}, 1);

  EXPECT_THROW_MSG_SUBSTR(zipMD(d, MDRead(Data::Value)), std::runtime_error,
                          "could not find variable with tag "
                          "Data::Value and name");
  EXPECT_NO_THROW(zipMD(d, MDRead(Data::Value, "name2")));
}

TEST(MDZipView, named_variable_and_coordinate) {
  Dataset d;
  d.insert(Coord::X, {}, 1);
  d.insert(Data::Value, "name", {}, 1);

  EXPECT_NO_THROW(zipMD(d, MDRead(Coord::X), MDRead(Data::Value, "name")));
}

TEST(MDZipView, spectrum_position) {
  Dataset dets;
  dets.insert(Coord::Position, {Dim::Detector, 4},
              {Eigen::Vector3d{1.0, 0.0, 0.0}, Eigen::Vector3d{2.0, 0.0, 0.0},
               Eigen::Vector3d{4.0, 0.0, 0.0}, Eigen::Vector3d{8.0, 0.0, 0.0}});

  Dataset d;
  d.insert(Coord::DetectorInfo, {}, {dets});
  Vector<boost::container::small_vector<scipp::index, 1>> grouping = {
      {0, 2}, {1}, {}};
  d.insert(Coord::DetectorGrouping, {Dim::Spectrum, 3}, grouping);

  auto view = zipMD(d, MDRead(Coord::Position));
  auto it = view.begin();
  EXPECT_EQ(it->get(Coord::Position)[0], 2.5);
  ++it;
  EXPECT_EQ(it->get(Coord::Position)[0], 2.0);
  ++it;
  EXPECT_THROW_MSG(it->get(Coord::Position), std::runtime_error,
                   "Spectrum has no detectors, cannot get position.");
  ++it;
  ASSERT_EQ(it, view.end());
}

TEST(MDZipView, derived_standard_deviation) {
  Dataset d;
  d.insert(Data::Variance, "", {Dim::X, 3}, {4.0, 9.0, -1.0});
  auto view = zipMD(d, MDRead(Data::StdDev));
  auto it = view.begin();
  EXPECT_EQ(it->get(Data::StdDev), 2.0);
  ++it;
  EXPECT_EQ(it->get(Data::StdDev), 3.0);
  ++it;
  EXPECT_TRUE(std::isnan(it->get(Data::StdDev)));
}

TEST(MDZipView, create_from_labels) {
  Dataset d;
  d.insert(Data::Value, "", Dimensions(Dim::X, 2), 2);
  d.insert(Data::Variance, "", {}, 1);
  auto var = d.get(Data::Value);
  var[0] = 0.2;
  var[1] = 3.2;

  ASSERT_ANY_THROW(zipMD(d, MDWrite(Data::Value), MDWrite(Data::Variance)));
  ASSERT_NO_THROW(zipMD(d, MDWrite(Data::Value), MDRead(Data::Variance)));
  auto view = zipMD(d, MDWrite(Data::Value), MDRead(Data::Variance));
  auto it = view.begin();
  ASSERT_EQ(it->get(Data::Value), 0.2);
  ASSERT_EQ(it->get(Data::Variance), 0);
  it++;
  ASSERT_EQ(it->get(Data::Value), 3.2);
  ASSERT_EQ(it->get(Data::Variance), 0);
}

TEST(MDZipView, create_from_labels_with_name) {
  Dataset d;
  d.insert(Data::Value, "name", Dimensions(Dim::X, 2), 2);
  d.insert(Data::Variance, "name", {}, 1);
  auto var = d.get(Data::Value, "name");
  var[0] = 0.2;
  var[1] = 3.2;

  auto view =
      zipMD(d, MDWrite(Data::Value, "name"), MDRead(Data::Variance, "name"));
  auto it = view.begin();
  ASSERT_EQ(it->get(Data::Value), 0.2);
  ASSERT_EQ(it->get(Data::Variance), 0);
  it++;
  ASSERT_EQ(it->get(Data::Value), 3.2);
  ASSERT_EQ(it->get(Data::Variance), 0);
}

TEST(MDZipView, create_from_labels_nested) {
  Dataset d;
  d.insert(Data::Value, "", {{Dim::Y, 3}, {Dim::X, 2}}, {1, 2, 3, 4, 5, 6});
  d.insert(Data::Variance, "", {Dim::X, 2}, {10, 20});

  auto nested = MDNested(MDRead(Data::Value));
  auto Nested = decltype(nested)::type(d);
  auto view = zipMD(d, {Dim::Y}, nested, MDRead(Data::Variance));

  ASSERT_EQ(view.size(), 2);
  double base = 0.0;
  for (const auto &item : view) {
    auto subview = item.get(Nested);
    ASSERT_EQ(subview.size(), 3);
    auto it = subview.begin();
    EXPECT_EQ(it++->get(Data::Value), base + 1.0);
    EXPECT_EQ(it++->get(Data::Value), base + 3.0);
    EXPECT_EQ(it++->get(Data::Value), base + 5.0);
    base += 1.0;
  }
}

TEST(MDZipView, event_lists_mutable) {
  Dataset eventList;
  eventList.insert(Data::Tof, "", {Dim::Event, 4}, {1, 2, 3, 4});
  eventList.insert(Data::PulseTime, "", {Dim::Event, 4}, {5, 6, 7, 8});
  ASSERT_EQ(eventList(Data::Tof).size(), 4);

  Dataset d;
  d.insert(Data::Events, "", {Dim::Spectrum, 3}, 3, eventList);

  auto view = zipMD(d, MDWrite(Data::Events));
  for (const auto &item : view) {
    // TODO Would be nice to simplify this, such that the event-list column
    // types are specified only once, at construction time of the main view, not
    // for each item, i.e., item.get(Data::Events) should directly return a
    // typed proxy.
    auto events = item.get(Data::Events).getMutable(Data::Tof, Data::PulseTime);
    events.push_back({1, 2});
  }
  // This only works since we are using a direct event storage as datasets.
  // Otherwise we need to use a proxy.
  const auto eventLists = d.get(Data::Events);
  ASSERT_EQ(eventLists.size(), 3);
  for (const auto &eventList : eventLists) {
    EXPECT_EQ(eventList(Data::Tof).size(), 5);
    EXPECT_EQ(eventList(Data::PulseTime).size(), 5);
  }
}

Dataset makeEventsNested() {
  Dataset d;
  d.insert(Data::Events, "", {Dim::Spectrum, 3});
  const auto eventLists = d.get(Data::Events);

  Dataset eventList0;
  eventList0.insert(Data::Tof, "", {Dim::Event, 2}, {1.0, 2.0});
  eventList0.insert(Data::PulseTime, "", {Dim::Event, 2}, {3.0, 4.0});
  eventLists[0] = eventList0;

  Dataset eventList1;
  eventList1.insert(Data::Tof, "", {Dim::Event, 0});
  eventList1.insert(Data::PulseTime, "", {Dim::Event, 0});
  eventLists[1] = eventList1;

  Dataset eventList2;
  eventList2.insert(Data::Tof, "", {Dim::Event, 3}, {1.0, 2.0, 3.0});
  eventList2.insert(Data::PulseTime, "", {Dim::Event, 3}, {3.0, 4.0, 5.0});
  eventLists[2] = eventList2;

  return d;
}

Dataset makeEventsSOA() {
  Dataset d;
  d.insert(Data::EventTofs, "", {Dim::Spectrum, 3});
  d.insert(Data::EventPulseTimes, "", {Dim::Spectrum, 3});
  auto tofs = d.get(Data::EventTofs);
  auto pulseTimes = d.get(Data::EventPulseTimes);
  tofs[0] = {1.0, 2.0};
  tofs[2] = {1.0, 2.0, 3.0};
  pulseTimes[0] = {3.0, 4.0};
  pulseTimes[2] = {3.0, 4.0, 5.0};
  return d;
}

template <class Event>
bool eventEq(const Event &e, double tof, double pulseTime) {
  return (std::get<0>(e) == tof) && (std::get<1>(e) == pulseTime);
}

// TODO Cannot pass as const due to current shortcoming of ZipView.
void testEvents(Dataset d) {
  auto view = zipMD(d, MDWrite(Data::Events));
  auto it = view.begin();
  EXPECT_EQ((it + 0)->get(Data::Events).get().size(), 2ul);
  EXPECT_EQ((it + 1)->get(Data::Events).get().size(), 0ul);
  EXPECT_EQ((it + 2)->get(Data::Events).get().size(), 3ul);
  auto el = it++->get(Data::Events).get();
  EXPECT_TRUE(eventEq(el[0], 1.0, 3.0));
  EXPECT_TRUE(eventEq(el[1], 2.0, 4.0));
  el = it++->get(Data::Events).get();
  el = it++->get(Data::Events).get();
  EXPECT_TRUE(eventEq(el[0], 1.0, 3.0));
  EXPECT_TRUE(eventEq(el[1], 2.0, 4.0));
  EXPECT_TRUE(eventEq(el[2], 3.0, 5.0));
}

TEST(MDZipView, event_lists_different_storage_same_API) {
  // Independent of the underlying storage format, we have the same API when
  // using zipMD.
  testEvents(makeEventsNested());
  testEvents(makeEventsSOA());
}