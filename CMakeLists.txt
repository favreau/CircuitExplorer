# Copyright (c) 2020-2021, Cyrille Favreau
# All rights reserved. Do not distribute without permission.
# Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
#
# This file is part of https://github.com/BlueBrain/CircuitExplorer

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

set(NAME CIRCUITEXPLORER)
set(PACKAGE_VERSION 0.1.0)
project(${NAME} VERSION ${PACKAGE_VERSION})

add_subdirectory(core)
