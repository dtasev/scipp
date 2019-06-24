// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2019 Scipp contributors (https://github.com/scipp)
#include "test_macros.h"
#include <gtest/gtest.h>

#include "scipp/core/dataset.h"
#include "scipp/core/dimensions.h"

#include "dataset_test_common.h"
#include <initializer_list>

using namespace scipp;
using namespace scipp::core;

class DataProxyBinaryOpEqualsTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<Dataset::value_type> {};

DatasetFactory3D datasetFactory;
auto d = datasetFactory.make();

INSTANTIATE_TEST_SUITE_P(AllItems, DataProxyBinaryOpEqualsTest,
                         ::testing::ValuesIn(d));

TEST_P(DataProxyBinaryOpEqualsTest, other_data_unchanged) {
  const auto &item = GetParam().second;
  auto dataset = datasetFactory.make();
  const auto original(dataset);
  const auto target = dataset["data_zyx"];

  ASSERT_NO_THROW(target += item);

  for (const auto & [ name, data ] : dataset) {
    if (name != "data_zyx") {
      EXPECT_EQ(data, original[name]);
    }
  }
}

TEST_P(DataProxyBinaryOpEqualsTest, plus_lhs_with_variance) {
  const auto &item = GetParam().second;
  auto dataset = datasetFactory.make();
  const auto target = dataset["data_zyx"];
  auto reference = target.data() + item.data();

  ASSERT_NO_THROW(target += item);
  EXPECT_EQ(target.data(), reference);
}

TEST_P(DataProxyBinaryOpEqualsTest, plus_lhs_without_variance) {
  const auto &item = GetParam().second;
  auto dataset = datasetFactory.make();
  const auto target = dataset["data_xyz"];
  if (item.hasVariances()) {
    ASSERT_ANY_THROW(target += item);
  } else {
    auto reference = target.data() + item.data();
    ASSERT_NO_THROW(target += item);
    EXPECT_EQ(target.data(), reference);
    EXPECT_FALSE(target.hasVariances());
  }
}

TEST_P(DataProxyBinaryOpEqualsTest, times_lhs_with_variance) {
  const auto &item = GetParam().second;
  auto dataset = datasetFactory.make();
  const auto target = dataset["data_zyx"];
  auto reference = target.data() * item.data();

  ASSERT_NO_THROW(target *= item);
  EXPECT_EQ(target.data(), reference);
}

TEST_P(DataProxyBinaryOpEqualsTest, times_lhs_without_variance) {
  const auto &item = GetParam().second;
  auto dataset = datasetFactory.make();
  const auto target = dataset["data_xyz"];
  if (item.hasVariances()) {
    ASSERT_ANY_THROW(target *= item);
  } else {
    auto reference = target.data() * item.data();
    ASSERT_NO_THROW(target *= item);
    EXPECT_EQ(target.data(), reference);
    EXPECT_FALSE(target.hasVariances());
  }
}

TEST_P(DataProxyBinaryOpEqualsTest, plus_slice_lhs_with_variance) {
  const auto &item = GetParam().second;
  auto dataset = datasetFactory.make();
  const auto target = dataset["data_zyx"];
  const auto &dims = item.dims();
  for (const Dim dim : dims.labels()) {
    auto reference = target.data() + item.data().slice({dim, 2});

    // Fails if any *other* multi-dimensional coord/label also depends on the
    // slicing dimension, since it will have mismatching values. Note that this
    // behavior is intended and important. It is crucial for preventing
    // operations between misaligned data in case a coordinate is
    // multi-dimensional.
    const auto coords = item.coords();
    const auto labels = item.labels();
    if (std::all_of(coords.begin(), coords.end(),
                    [dim](const auto &coord) {
                      return coord.first == dim ||
                             !coord.second.dims().contains(dim);
                    }) &&
        std::all_of(labels.begin(), labels.end(), [dim](const auto &labels_) {
          return labels_.second.dims().inner() == dim ||
                 !labels_.second.dims().contains(dim);
        })) {
      ASSERT_NO_THROW(target += item.slice({dim, 2}));

      EXPECT_EQ(target.data(), reference);
    } else {
      ASSERT_ANY_THROW(target += item.slice({dim, 2}));
    }
  }
}

// We need the decltype(auto) since we are using these operators for both
// proxies and non-proxies. The former return by reference, the latter by value.
struct plus_equals {
  template <class A, class B>
  decltype(auto) operator()(A &&a, const B &b) const {
    return a += b;
  }
};
struct times_equals {
  template <class A, class B>
  decltype(auto) operator()(A &&a, const B &b) const {
    return a *= b;
  }
};

using BinaryEquals = ::testing::Types<plus_equals, times_equals>;

template <class Op>
class DatasetBinaryOpTest : public ::testing::Test,
                            public ::testing::WithParamInterface<Op> {
protected:
  Op op;
};

template <class Op>
class DatasetProxyBinaryOpTest : public ::testing::Test,
                                 public ::testing::WithParamInterface<Op> {
protected:
  Op op;
};

TYPED_TEST_SUITE(DatasetBinaryOpTest, BinaryEquals);
TYPED_TEST_SUITE(DatasetProxyBinaryOpTest, BinaryEquals);

// DataProxyBinaryOpEqualsTest ensures correctness of operations between
// DataProxy with itself, so we can rely on that for building the reference.
TYPED_TEST(DatasetBinaryOpTest, return_value) {
  auto a = datasetFactory.make();
  auto b = datasetFactory.make();

  ASSERT_TRUE((std::is_same_v<decltype(TestFixture::op(a, b["data_scalar"])),
                              Dataset &>));
  const auto &result1 = TestFixture::op(a, b["data_scalar"]);
  ASSERT_EQ(&result1, &a);

  ASSERT_TRUE((std::is_same_v<decltype(TestFixture::op(a, b)), Dataset &>));
  const auto &result2 = TestFixture::op(a, b);
  ASSERT_EQ(&result2, &a);

  ASSERT_TRUE(
      (std::is_same_v<decltype(TestFixture::op(a, b.slice({Dim::Z, 3}))),
                      Dataset &>));
  const auto &result3 = TestFixture::op(a, b.slice({Dim::Z, 3}));
  ASSERT_EQ(&result3, &a);
}

TYPED_TEST(DatasetBinaryOpTest, rhs_DataProxy_self_overlap) {
  auto dataset = datasetFactory.make();
  auto original(dataset);
  auto reference(dataset);

  ASSERT_NO_THROW(TestFixture::op(dataset, dataset["data_scalar"]));
  for (const auto[name, item] : dataset) {
    EXPECT_EQ(item, TestFixture::op(reference[name], original["data_scalar"]));
  }
}

TYPED_TEST(DatasetBinaryOpTest, rhs_DataProxy_self_overlap_slice) {
  auto dataset = datasetFactory.make();
  auto original(dataset);
  auto reference(dataset);

  ASSERT_NO_THROW(
      TestFixture::op(dataset, dataset["values_x"].slice({Dim::X, 1})));
  for (const auto[name, item] : dataset) {
    EXPECT_EQ(item, TestFixture::op(reference[name],
                                    original["values_x"].slice({Dim::X, 1})));
  }
}

TYPED_TEST(DatasetBinaryOpTest, rhs_Dataset) {
  auto a = datasetFactory.make();
  auto b = datasetFactory.make();
  auto reference(a);

  ASSERT_NO_THROW(TestFixture::op(a, b));
  for (const auto[name, item] : a) {
    EXPECT_EQ(item, TestFixture::op(reference[name], b[name]));
  }
}

TYPED_TEST(DatasetBinaryOpTest, rhs_Dataset_coord_mismatch) {
  auto a = datasetFactory.make();
  DatasetFactory3D otherCoordsFactory;
  auto b = otherCoordsFactory.make();

  ASSERT_THROW(TestFixture::op(a, b), except::CoordMismatchError);
}

TYPED_TEST(DatasetBinaryOpTest, rhs_Dataset_with_missing_items) {
  auto a = datasetFactory.make();
  a.setData("extra", makeVariable<double>({}));
  auto b = datasetFactory.make();
  auto reference(a);

  ASSERT_NO_THROW(TestFixture::op(a, b));
  for (const auto[name, item] : a) {
    if (name == "extra") {
      EXPECT_EQ(item, reference[name]);
    } else {
      EXPECT_EQ(item, TestFixture::op(reference[name], b[name]));
    }
  }
}

TYPED_TEST(DatasetBinaryOpTest, rhs_Dataset_with_extra_items) {
  auto a = datasetFactory.make();
  auto b = datasetFactory.make();
  b.setData("extra", makeVariable<double>({}));

  ASSERT_ANY_THROW(TestFixture::op(a, b));
}

TYPED_TEST(DatasetBinaryOpTest, rhs_DatasetProxy_self_overlap) {
  auto dataset = datasetFactory.make();
  const auto slice = dataset.slice({Dim::Z, 3});
  auto reference(dataset);

  ASSERT_NO_THROW(TestFixture::op(dataset, slice));
  for (const auto[name, item] : dataset) {
    // Items independent of Z are removed when creating `slice`.
    if (item.dims().contains(Dim::Z)) {
      EXPECT_EQ(item, TestFixture::op(reference[name],
                                      reference[name].slice({Dim::Z, 3})));
    } else {
      EXPECT_EQ(item, reference[name]);
    }
  }
}

TYPED_TEST(DatasetBinaryOpTest, rhs_DatasetProxy_coord_mismatch) {
  auto dataset = datasetFactory.make();

  // Non-range sliced throws for X and Y due to multi-dimensional coords.
  ASSERT_THROW(TestFixture::op(dataset, dataset.slice({Dim::X, 3})),
               except::CoordMismatchError);
  ASSERT_THROW(TestFixture::op(dataset, dataset.slice({Dim::Y, 3})),
               except::CoordMismatchError);

  ASSERT_THROW(TestFixture::op(dataset, dataset.slice({Dim::X, 3, 4})),
               except::CoordMismatchError);
  ASSERT_THROW(TestFixture::op(dataset, dataset.slice({Dim::Y, 3, 4})),
               except::CoordMismatchError);
  ASSERT_THROW(TestFixture::op(dataset, dataset.slice({Dim::Z, 3, 4})),
               except::CoordMismatchError);
}

Dataset make_simple_sparse(std::initializer_list<double> values,
                           std::string key = "sparse") {
  Dataset ds;
  auto var = makeVariable<double>({Dim::X, Dimensions::Sparse});
  var.sparseValues<double>()[0] = values;
  ds.setData(key, var);
  return ds;
}

Dataset make_sparse_2d(std::initializer_list<double> values,
                       std::string key = "sparse") {
  Dataset ds;
  auto var = makeVariable<double>({Dim::X, Dim::Y}, {2, Dimensions::Sparse});
  var.sparseValues<double>()[0] = values;
  var.sparseValues<double>()[1] = values;
  ds.setData(key, var);
  return ds;
}

TYPED_TEST(DatasetBinaryOpTest,
           with_single_var_with_single_sparse_dimensions_sized_same) {
  Dataset a = make_simple_sparse({1.1, 2.2});
  Dataset b = make_simple_sparse({3.3, 4.4});
  Dataset c = TestFixture::op(a, b);
  auto c_data = c["sparse"].data().sparseValues<double>()[0];
  ASSERT_EQ(c_data[0], TestFixture::op(1.1, 3.3));
  ASSERT_EQ(c_data[1], TestFixture::op(2.2, 4.4));
}

TYPED_TEST(DatasetBinaryOpTest, with_single_var_dense_and_sparse_dimension) {
  Dataset a = make_sparse_2d({1.1, 2.2});
  Dataset b = make_sparse_2d({3.3, 4.4});
  Dataset c = TestFixture::op(a, b);
  ASSERT_EQ(c["sparse"].data().sparseValues<double>().size(), 2);
  auto c_data = c["sparse"].data().sparseValues<double>()[0];
  ASSERT_EQ(c_data[0], TestFixture::op(1.1, 3.3));
  ASSERT_EQ(c_data[1], TestFixture::op(2.2, 4.4));
}

TYPED_TEST(DatasetBinaryOpTest, with_multiple_variables) {
  Dataset a = make_simple_sparse({1.1, 2.2});
  a.setData("sparse2", a["sparse"].data());
  Dataset b = make_simple_sparse({3.3, 4.4});
  b.setData("sparse2", b["sparse"].data());
  Dataset c = TestFixture::op(a, b);
  ASSERT_EQ(c.size(), 2);
  auto c_data = c["sparse"].data().sparseValues<double>()[0];
  ASSERT_EQ(c_data[0], TestFixture::op(1.1, 3.3));
  ASSERT_EQ(c_data[1], TestFixture::op(2.2, 4.4));
  c_data = c["sparse2"].data().sparseValues<double>()[1];
  ASSERT_EQ(c_data[0], TestFixture::op(1.1, 3.3));
  ASSERT_EQ(c_data[1], TestFixture::op(2.2, 4.4));
}

TYPED_TEST(DatasetBinaryOpTest, with_sparse_dimensions_of_different_sizes) {
  Dataset a = make_simple_sparse({1.1, 2.2});
  Dataset b = make_simple_sparse({3.3, 4.4, 5.5});
  ASSERT_THROW(TestFixture::op(a, b), std::runtime_error);
}

TYPED_TEST(DatasetProxyBinaryOpTest, return_value) {
  auto a = datasetFactory.make();
  auto b = datasetFactory.make();
  DatasetProxy proxy(a);

  ASSERT_TRUE(
      (std::is_same_v<decltype(TestFixture::op(proxy, b["data_scalar"])),
                      DatasetProxy>));
  const auto &result1 = TestFixture::op(proxy, b["data_scalar"]);
  EXPECT_EQ(&result1["data_scalar"].template values<double>()[0],
            &a["data_scalar"].template values<double>()[0]);

  ASSERT_TRUE(
      (std::is_same_v<decltype(TestFixture::op(proxy, b)), DatasetProxy>));
  const auto &result2 = TestFixture::op(proxy, b);
  EXPECT_EQ(&result2["data_scalar"].template values<double>()[0],
            &a["data_scalar"].template values<double>()[0]);

  ASSERT_TRUE(
      (std::is_same_v<decltype(TestFixture::op(proxy, b.slice({Dim::Z, 3}))),
                      DatasetProxy>));
  const auto &result3 = TestFixture::op(proxy, b.slice({Dim::Z, 3}));
  EXPECT_EQ(&result3["data_scalar"].template values<double>()[0],
            &a["data_scalar"].template values<double>()[0]);
}

TYPED_TEST(DatasetProxyBinaryOpTest, rhs_DataProxy_self_overlap) {
  auto dataset = datasetFactory.make();
  auto reference(dataset);
  TestFixture::op(reference, dataset["data_scalar"]);

  for (scipp::index z = 0; z < dataset.coords()[Dim::Z].dims()[Dim::Z]; ++z) {
    for (const auto & [ name, item ] : dataset)
      if (item.dims().contains(Dim::Z)) {
        EXPECT_NE(item, reference[name]);
      }
    ASSERT_NO_THROW(
        TestFixture::op(dataset.slice({Dim::Z, z}), dataset["data_scalar"]));
  }
  for (const auto & [ name, item ] : dataset)
    if (item.dims().contains(Dim::Z)) {
      EXPECT_EQ(item, reference[name]);
    }
}

TYPED_TEST(DatasetProxyBinaryOpTest, rhs_DataProxy_self_overlap_slice) {
  auto dataset = datasetFactory.make();
  auto reference(dataset);
  TestFixture::op(reference, dataset["values_x"].slice({Dim::X, 1}));

  for (scipp::index z = 0; z < dataset.coords()[Dim::Z].dims()[Dim::Z]; ++z) {
    for (const auto & [ name, item ] : dataset)
      if (item.dims().contains(Dim::Z)) {
        EXPECT_NE(item, reference[name]);
      }
    ASSERT_NO_THROW(TestFixture::op(dataset.slice({Dim::Z, z}),
                                    dataset["values_x"].slice({Dim::X, 1})));
  }
  for (const auto & [ name, item ] : dataset)
    if (item.dims().contains(Dim::Z)) {
      EXPECT_EQ(item, reference[name]);
    }
}

TYPED_TEST(DatasetProxyBinaryOpTest, rhs_Dataset_coord_mismatch) {
  DatasetFactory3D otherCoordsFactory;
  auto a = otherCoordsFactory.make();
  auto b = datasetFactory.make();

  ASSERT_THROW(TestFixture::op(DatasetProxy(a), b), except::CoordMismatchError);
}

TYPED_TEST(DatasetProxyBinaryOpTest, rhs_Dataset_with_missing_items) {
  auto a = datasetFactory.make();
  a.setData("extra", makeVariable<double>({}));
  auto b = datasetFactory.make();
  auto reference(a);

  ASSERT_NO_THROW(TestFixture::op(DatasetProxy(a), b));
  for (const auto[name, item] : a) {
    if (name == "extra") {
      EXPECT_EQ(item, reference[name]);
    } else {
      EXPECT_EQ(item, TestFixture::op(reference[name], b[name]));
    }
  }
}

TYPED_TEST(DatasetProxyBinaryOpTest, rhs_Dataset_with_extra_items) {
  auto a = datasetFactory.make();
  auto b = datasetFactory.make();
  b.setData("extra", makeVariable<double>({}));

  ASSERT_ANY_THROW(TestFixture::op(DatasetProxy(a), b));
}

TYPED_TEST(DatasetProxyBinaryOpTest, rhs_DatasetProxy_self_overlap) {
  auto dataset = datasetFactory.make();
  const auto slice = dataset.slice({Dim::Z, 3});
  auto reference(dataset);

  ASSERT_NO_THROW(TestFixture::op(dataset.slice({Dim::Z, 0, 3}), slice));
  ASSERT_NO_THROW(TestFixture::op(dataset.slice({Dim::Z, 3, 6}), slice));
  for (const auto[name, item] : dataset) {
    // Items independent of Z are removed when creating `slice`.
    if (item.dims().contains(Dim::Z)) {
      EXPECT_EQ(item, TestFixture::op(reference[name],
                                      reference[name].slice({Dim::Z, 3})));
    } else {
      EXPECT_EQ(item, reference[name]);
    }
  }
}

TYPED_TEST(DatasetProxyBinaryOpTest,
           rhs_DatasetProxy_self_overlap_undetectable) {
  auto dataset = datasetFactory.make();
  const auto slice = dataset.slice({Dim::Z, 3});
  auto reference(dataset);

  // Same as `rhs_DatasetProxy_self_overlap` above, but reverse slice order. The
  // second line will see the updated slice 3, and there is no way to detect and
  // prevent this.
  ASSERT_NO_THROW(TestFixture::op(dataset.slice({Dim::Z, 3, 6}), slice));
  ASSERT_NO_THROW(TestFixture::op(dataset.slice({Dim::Z, 0, 3}), slice));
  for (const auto[name, item] : dataset) {
    // Items independent of Z are removed when creating `slice`.
    if (item.dims().contains(Dim::Z)) {
      EXPECT_NE(item, TestFixture::op(reference[name],
                                      reference[name].slice({Dim::Z, 3})));
    } else {
      EXPECT_EQ(item, reference[name]);
    }
  }
}

TYPED_TEST(DatasetProxyBinaryOpTest, rhs_DatasetProxy_coord_mismatch) {
  auto dataset = datasetFactory.make();
  const DatasetProxy proxy(dataset);

  // Non-range sliced throws for X and Y due to multi-dimensional coords.
  ASSERT_THROW(TestFixture::op(proxy, dataset.slice({Dim::X, 3})),
               except::CoordMismatchError);
  ASSERT_THROW(TestFixture::op(proxy, dataset.slice({Dim::Y, 3})),
               except::CoordMismatchError);

  ASSERT_THROW(TestFixture::op(proxy, dataset.slice({Dim::X, 3, 4})),
               except::CoordMismatchError);
  ASSERT_THROW(TestFixture::op(proxy, dataset.slice({Dim::Y, 3, 4})),
               except::CoordMismatchError);
  ASSERT_THROW(TestFixture::op(proxy, dataset.slice({Dim::Z, 3, 4})),
               except::CoordMismatchError);
}
