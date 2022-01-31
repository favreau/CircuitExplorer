# Copyright (c) 2015-2021, EPFL/Blue Brain Project
# All rights reserved. Do not distribute without permission.
#
# This file is part of Brayns <https://github.com/BlueBrain/Brayns>

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

# ==============================================================================
# Project
# ==============================================================================
set(LIBRARY_NAME CircuitExplorer)
set(${NAME}_VERSION_ABI 1)

# ==============================================================================
# Packages and settings
# ==============================================================================
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake)
set(${NAME}_MODULES_DIR ${PROJECT_SOURCE_DIR})
set(CMAKE_CXX_STANDARD 14) # C++ 14

# Packages
find_package(Brayns REQUIRED)
find_package(ospray REQUIRED)

option(${NAME}_USE_MORPHOLOGIES "Use morphologies features" ON)

set(USE_MORPHOLOGIES 0)
if(${NAME}_USE_MORPHOLOGIES)
  find_package(Brion REQUIRED)
  set(USE_MORPHOLOGIES 1)
endif()

# OpenMP
find_package(OpenMP)
if(OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

# Optional packages (managed by Defines.h.in)
set(CGAL_FOUND 0)
option(${NAME}_USE_CGAL "Use CGAL meshing features" ON)
if(${${NAME}_USE_CGAL})
    find_package(CGAL REQUIRED)
endif()

set(PQXX_FOUND 0)
option(${NAME}_USE_PQXX "Use PostgreSQL database" ON)
if(${${NAME}_USE_PQXX})
    unset(PQXX_INCLUDE_DIRECTORIES CACHE)
    unset(PQXX_LIBRARIES CACHE)
    find_package(PQXX REQUIRED)
endif()

configure_file( common/Defines.h.in ${CMAKE_BINARY_DIR}/generated/circuitexplorer/Defines.h )
include_directories( ${PROJECT_SOURCE_DIR}/core ${CMAKE_BINARY_DIR}/generated/circuitexplorer )

# ==============================================================================
# Sources
# ==============================================================================
set(${NAME}_SOURCES
    common/Utils.cpp
    plugin/CircuitExplorerPlugin.cpp
    plugin/api/CircuitExplorerParams.cpp
    plugin/io/filesystem/BrickLoader.cpp
    module/ispc/camera/DOFPerspectiveCamera.cpp
    module/ispc/camera/SphereClippingPerspectiveCamera.cpp
    module/ispc/render/CircuitExplorerMaterial.cpp
    module/ispc/render/CircuitExplorerBasicRenderer.cpp
    module/ispc/render/CircuitExplorerAdvancedRenderer.cpp
    module/ispc/render/VoxelizedSimulationRenderer.cpp
    module/ispc/render/utils/CircuitExplorerAbstractRenderer.cpp
    module/ispc/render/utils/CircuitExplorerSimulationRenderer.cpp
    module/ispc/render/DefaultMaterial.cpp
)

set(${NAME}_PUBLIC_HEADERS
    common/Utils.h
    plugin/CircuitExplorerPlugin.h
    plugin/api/CircuitExplorerParams.h
    plugin/io/filesystem/BrickLoader.h
)

if(${NAME}_USE_MORPHOLOGIES)
    list(APPEND CIRCUITEXPLORER_SOURCES
        module/ispc/render/ProximityDetectionRenderer.cpp
        module/ispc/render/CellGrowthRenderer.cpp
        plugin/neuroscience/common/MorphologyLoader.cpp
        plugin/neuroscience/common/ParallelModelContainer.cpp
        plugin/neuroscience/neuron/CellGrowthHandler.cpp
        plugin/neuroscience/neuron/VoltageSimulationHandler.cpp
        plugin/neuroscience/neuron/SpikeSimulationHandler.cpp
        plugin/neuroscience/astrocyte/AstrocyteLoader.cpp
        plugin/neuroscience/neuron/AbstractCircuitLoader.cpp
        plugin/neuroscience/neuron/PairSynapsesLoader.cpp
        plugin/neuroscience/neuron/MeshCircuitLoader.cpp
        plugin/neuroscience/neuron/MorphologyCollageLoader.cpp
        plugin/neuroscience/neuron/AdvancedCircuitLoader.cpp
        plugin/neuroscience/neuron/SynapseCircuitLoader.cpp
        plugin/meshing/MetaballsGenerator.cpp
        )
    list(APPEND CIRCUITEXPLORER_PUBLIC_HEADERS
        plugin/neuroscience/common/ParallelModelContainer.h
        plugin/neuroscience/common/MorphologyLoader.h
        plugin/neuroscience/neuron/CellGrowthHandler.h
        plugin/neuroscience/neuron/VoltageSimulationHandler.h
        plugin/neuroscience/neuron/SpikeSimulationHandler.h
        plugin/neuroscience/astrocyte/AstrocyteLoader.h
        plugin/neuroscience/neuron/AbstractCircuitLoader.h
        plugin/neuroscience/neuron/PairSynapsesLoader.h
        plugin/neuroscience/neuron/MeshCircuitLoader.h
        plugin/neuroscience/neuron/MorphologyCollageLoader.h
        plugin/neuroscience/neuron/AdvancedCircuitLoader.h
        plugin/neuroscience/neuron/SynapseCircuitLoader.h
        plugin/meshing/MetaballsGenerator.h
        )
endif()

if(${CGAL_FOUND})
    list(APPEND ${NAME}_SOURCES plugin/meshing/PointCloudMesher.cpp)
    list(APPEND ${NAME}_PUBLIC_HEADERS plugin/meshing/PointCloudMesher.h)
endif()

if(${PQXX_FOUND} AND ${NAME}_USE_MORPHOLOGIES)
    list(APPEND ${NAME}_SOURCES plugin/io/db/DBConnector.cpp)
    list(APPEND ${NAME}_PUBLIC_HEADERS plugin/io/db/DBConnector.h)
endif()

set(${NAME}_ISPC_SOURCES
    module/ispc/camera/utils.ispc
    module/ispc/camera/DOFPerspectiveCamera.ispc
    module/ispc/camera/SphereClippingPerspectiveCamera.ispc
    module/ispc/render/CircuitExplorerMaterial.ispc
    module/ispc/render/CircuitExplorerBasicRenderer.ispc
    module/ispc/render/CircuitExplorerAdvancedRenderer.ispc
    module/ispc/render/VoxelizedSimulationRenderer.ispc
    module/ispc/render/utils/CircuitExplorerSimulationRenderer.ispc
    module/ispc/render/utils/CircuitExplorerRandomGenerator.ispc
    module/ispc/render/DefaultMaterial.ispc
)

if(${NAME}_USE_MORPHOLOGIES)
    list(APPEND ${NAME}_ISPC_SOURCES
        module/ispc/render/ProximityDetectionRenderer.ispc
        module/ispc/render/CellGrowthRenderer.ispc
    )
endif()

# ==============================================================================
# Compile ispc code
# ==============================================================================
list(APPEND CMAKE_MODULE_PATH ${OSPRAY_CMAKE_ROOT})
if(CMAKE_BUILD_TYPE STREQUAL Debug)
  set(OSPRAY_DEBUG_BUILD ON)
endif()
include(ispc)
list(APPEND ALL_ISPC_INCLUDES ${OSPRAY_INCLUDE_DIRS})
include_directories_ispc(${ALL_ISPC_INCLUDES} )
ospray_ispc_compile(${${NAME}_ISPC_SOURCES})
list(APPEND ${NAME}_SOURCES ${ISPC_OBJECTS})

# ==============================================================================
# Compile C++ code
# ==============================================================================
include_directories(
    SYSTEM
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(${NAME}_LINK_LIBRARIES
    PUBLIC ${OSPRAY_LIBRARIES}
    PRIVATE braynsCommon braynsParameters braynsIO braynsEngine braynsPluginAPI
    braynsOSPRayEngine glm 
    ${FREEIMAGE_LIBRARIES} ${HDF5_LIBRARIES}
)

if(${NAME}_USE_MORPHOLOGIES)
    list(APPEND ${NAME}_LINK_LIBRARIES Brion Brain)
endif()

if(${CGAL_FOUND})
    list(APPEND ${NAME}_LINK_LIBRARIES ${CGAL_LIBRARIES})
endif()

if(${PQXX_FOUND})
    list(APPEND ${NAME}_LINK_LIBRARIES ${PQXX_LIBRARIES})
endif()

add_library(
    ${LIBRARY_NAME}
    SHARED ${${NAME}_SOURCES})

target_link_libraries(
    ${LIBRARY_NAME}
    ${${NAME}_LINK_LIBRARIES})

# ==============================================================================
# Install binaries
# ==============================================================================
INSTALL(TARGETS ${LIBRARY_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)
