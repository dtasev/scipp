/// @file
/// SPDX-License-Identifier: GPL-3.0-or-later
/// @author Simon Heybrock
/// Copyright &copy; 2019 ISIS Rutherford Appleton Laboratory, NScD Oak Ridge
/// National Laboratory, and European Spallation Source ERIC.
#ifndef BOOL_H
#define BOOL_H

namespace scipp::core {

class Bool {
public:
  Bool(const bool value = false) : m_value(value) {}
  operator const bool &() const { return m_value; }
  operator bool &() { return m_value; }

private:
  bool m_value;
};

} // namespace scipp::core

#endif // BOOL_H