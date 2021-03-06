# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2019 Scipp contributors (https://github.com/scipp)
# @file
# @author Neil Vaytet

# flake8: noqa

from .plot import plot


def superplot(dataset, **kwargs):
    return plot(dataset, projection="1d", **kwargs)

def image(dataset, **kwargs):
    return plot(dataset, projection="2d", **kwargs)

def threeslice(dataset, **kwargs):
    return plot(dataset, projection="3d", **kwargs)

def volume(dataset, **kwargs):
    return plot(dataset, projection="volume", **kwargs)
