import unittest

from dataset import *
import numpy as np

class TestDataset(unittest.TestCase):
    def setUp(self):
        lx = 2
        ly = 3
        lz = 4
        self.reference_x = np.arange(lx)
        self.reference_y = np.arange(ly)
        self.reference_z = np.arange(lz)
        self.reference_data1 = np.arange(lx*ly*lz).reshape(lz,ly,lx)
        self.reference_data2 = np.ones(lx*ly*lz).reshape(lz,ly,lx)
        self.reference_data3 = np.arange(lx*lz).reshape(lz,lx)

        self.dataset = Dataset()
        self.dataset[Data.Value, "data1"] = ([Dim.Z, Dim.Y, Dim.X], self.reference_data1)
        self.dataset[Data.Value, "data2"] = ([Dim.Z, Dim.Y, Dim.X], self.reference_data2)
        self.dataset[Data.Value, "data3"] = ([Dim.Z, Dim.X], self.reference_data3)
        self.dataset[Coord.X] = ([Dim.X], self.reference_x)
        self.dataset[Coord.Y] = ([Dim.Y], self.reference_y)
        self.dataset[Coord.Z] = ([Dim.Z], self.reference_z)

    def test_insert(self):
        d = Dataset()
        d[Data.Value, "data1"] = ([Dim.Z, Dim.Y, Dim.X], np.arange(24).reshape(4,3,2))
        self.assertEqual(len(d), 1)
        np.testing.assert_array_equal(d[Data.Value, "data1"].numpy, self.reference_data1)

    def test_size(self):
        # X, Y, Z, 3 x Data::Value
        self.assertEqual(self.dataset.size(), 6)

    def test_dimensions(self):
        self.assertEqual(self.dataset.dimensions().size(Dim.X), 2)
        self.assertEqual(self.dataset.dimensions().size(Dim.Y), 3)
        self.assertEqual(self.dataset.dimensions().size(Dim.Z), 4)

    def test_data(self):
        np.testing.assert_array_equal(self.dataset[Coord.X].numpy, self.reference_x)
        np.testing.assert_array_equal(self.dataset[Coord.Y].numpy, self.reference_y)
        np.testing.assert_array_equal(self.dataset[Coord.Z].numpy, self.reference_z)
        np.testing.assert_array_equal(self.dataset[Data.Value, "data1"].numpy, self.reference_data1)
        np.testing.assert_array_equal(self.dataset[Data.Value, "data2"].numpy, self.reference_data2)
        np.testing.assert_array_equal(self.dataset[Data.Value, "data3"].numpy, self.reference_data3)

    def test_view_subdata(self):
        view = self.dataset["data1"]
        # TODO Need consistent dimensions() implementation for Dataset and its views.
        #self.assertEqual(view.dimensions().size(Dim.X), 2)
        #self.assertEqual(view.dimensions().size(Dim.Y), 3)
        #self.assertEqual(view.dimensions().size(Dim.Z), 4)
        self.assertEqual(view.size(), 4)

    def test_slice_dataset(self):
        for x in range(2):
            view = self.dataset[Dim.X, x]
            self.assertRaisesRegex(RuntimeError, 'Dataset does not contain such a variable.', view.__getitem__, Coord.X)
            np.testing.assert_array_equal(view[Coord.Y].numpy, self.reference_y)
            np.testing.assert_array_equal(view[Coord.Z].numpy, self.reference_z)
            np.testing.assert_array_equal(view[Data.Value, "data1"].numpy, self.reference_data1[:,:,x])
            np.testing.assert_array_equal(view[Data.Value, "data2"].numpy, self.reference_data2[:,:,x])
            np.testing.assert_array_equal(view[Data.Value, "data3"].numpy, self.reference_data3[:,x])
        for y in range(3):
            view = self.dataset[Dim.Y, y]
            np.testing.assert_array_equal(view[Coord.X].numpy, self.reference_x)
            self.assertRaisesRegex(RuntimeError, 'Dataset does not contain such a variable.', view.__getitem__, Coord.Y)
            np.testing.assert_array_equal(view[Coord.Z].numpy, self.reference_z)
            np.testing.assert_array_equal(view[Data.Value, "data1"].numpy, self.reference_data1[:,y,:])
            np.testing.assert_array_equal(view[Data.Value, "data2"].numpy, self.reference_data2[:,y,:])
            np.testing.assert_array_equal(view[Data.Value, "data3"].numpy, self.reference_data3)
        for z in range(4):
            view = self.dataset[Dim.Z, z]
            np.testing.assert_array_equal(view[Coord.X].numpy, self.reference_x)
            np.testing.assert_array_equal(view[Coord.Y].numpy, self.reference_y)
            self.assertRaisesRegex(RuntimeError, 'Dataset does not contain such a variable.', view.__getitem__, Coord.Z)
            np.testing.assert_array_equal(view[Data.Value, "data1"].numpy, self.reference_data1[z,:,:])
            np.testing.assert_array_equal(view[Data.Value, "data2"].numpy, self.reference_data2[z,:,:])
            np.testing.assert_array_equal(view[Data.Value, "data3"].numpy, self.reference_data3[z,:])
        for x in range(2):
            for delta in range(3-x):
                view = self.dataset[Dim.X, x:x+delta]
                np.testing.assert_array_equal(view[Coord.X].numpy, self.reference_x[x:x+delta])
                np.testing.assert_array_equal(view[Coord.Y].numpy, self.reference_y)
                np.testing.assert_array_equal(view[Coord.Z].numpy, self.reference_z)
                np.testing.assert_array_equal(view[Data.Value, "data1"].numpy, self.reference_data1[:,:,x:x+delta])
                np.testing.assert_array_equal(view[Data.Value, "data2"].numpy, self.reference_data2[:,:,x:x+delta])
                np.testing.assert_array_equal(view[Data.Value, "data3"].numpy, self.reference_data3[:,x:x+delta])
        for y in range(3):
            for delta in range(4-y):
                view = self.dataset[Dim.Y, y:y+delta]
                np.testing.assert_array_equal(view[Coord.X].numpy, self.reference_x)
                np.testing.assert_array_equal(view[Coord.Y].numpy, self.reference_y[y:y+delta])
                np.testing.assert_array_equal(view[Coord.Z].numpy, self.reference_z)
                np.testing.assert_array_equal(view[Data.Value, "data1"].numpy, self.reference_data1[:,y:y+delta,:])
                np.testing.assert_array_equal(view[Data.Value, "data2"].numpy, self.reference_data2[:,y:y+delta,:])
                np.testing.assert_array_equal(view[Data.Value, "data3"].numpy, self.reference_data3)
        for z in range(4):
            for delta in range(5-z):
                view = self.dataset[Dim.Z, z:z+delta]
                np.testing.assert_array_equal(view[Coord.X].numpy, self.reference_x)
                np.testing.assert_array_equal(view[Coord.Y].numpy, self.reference_y)
                np.testing.assert_array_equal(view[Coord.Z].numpy, self.reference_z[z:z+delta])
                np.testing.assert_array_equal(view[Data.Value, "data1"].numpy, self.reference_data1[z:z+delta,:,:])
                np.testing.assert_array_equal(view[Data.Value, "data2"].numpy, self.reference_data2[z:z+delta,:,:])
                np.testing.assert_array_equal(view[Data.Value, "data3"].numpy, self.reference_data3[z:z+delta,:])

    def test_numpy_interoperable(self):
        self.dataset[Data.Value, 'data2'] = np.exp(self.dataset[Data.Value, 'data1'])
        np.testing.assert_array_equal(self.dataset[Data.Value, "data2"].numpy, np.exp(self.reference_data1))
        # Restore original value.
        self.dataset[Data.Value, 'data2'] = self.reference_data2
        np.testing.assert_array_equal(self.dataset[Data.Value, "data2"].numpy, self.reference_data2)

    def test_slice_numpy_interoperable(self):
        # Dataset subset then view single variable
        self.dataset['data2'][Data.Value, 'data2'] = np.exp(self.dataset[Data.Value, 'data1'])
        np.testing.assert_array_equal(self.dataset[Data.Value, "data2"].numpy, np.exp(self.reference_data1))
        # Slice view of dataset then view single variable
        self.dataset[Dim.X, 0][Data.Value, 'data2'] = np.exp(self.dataset[Dim.X, 1][Data.Value, 'data1'])
        np.testing.assert_array_equal(self.dataset[Data.Value, "data2"].numpy[...,0], np.exp(self.reference_data1[...,1]))
        # View single variable then slice view
        self.dataset[Data.Value, 'data2'][Dim.X, 1] = np.exp(self.dataset[Data.Value, 'data1'][Dim.X, 0])
        np.testing.assert_array_equal(self.dataset[Data.Value, "data2"].numpy[...,1], np.exp(self.reference_data1[...,0]))
        # View single variable then view range of slices
        self.dataset[Data.Value, 'data2'][Dim.Y, 1:3] = np.exp(self.dataset[Data.Value, 'data1'][Dim.Y, 0:2])
        np.testing.assert_array_equal(self.dataset[Data.Value, "data2"].numpy[:,1:3,:], np.exp(self.reference_data1[:,0:2,:]))

        # Restore original value.
        self.dataset[Data.Value, 'data2'] = self.reference_data2
        np.testing.assert_array_equal(self.dataset[Data.Value, "data2"].numpy, self.reference_data2)

if __name__ == '__main__':
    unittest.main()