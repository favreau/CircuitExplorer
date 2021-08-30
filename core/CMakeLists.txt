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

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake)

# Packages
find_package(Brayns REQUIRED)
find_package(Brion REQUIRED)
find_package(ospray REQUIRED)
find_package(OpenMP)

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
set(CIRCUITEXPLORER_SOURCES
    module/ispc/camera/DOFPerspectiveCamera.cpp
    module/ispc/camera/SphereClippingPerspectiveCamera.cpp
    module/ispc/render/CircuitExplorerMaterial.cpp
    module/ispc/render/CircuitExplorerBasicRenderer.cpp
    module/ispc/render/CircuitExplorerAdvancedRenderer.cpp
    module/ispc/render/VoxelizedSimulationRenderer.cpp
    module/ispc/render/ProximityDetectionRenderer.cpp
    module/ispc/render/CellGrowthRenderer.cpp
    module/ispc/render/utils/CircuitExplorerAbstractRenderer.cpp
    module/ispc/render/utils/CircuitExplorerSimulationRenderer.cpp
    module/ispc/render/DefaultMaterial.cpp
    plugin/api/CircuitExplorerParams.cpp
    plugin/meshing/MetaballsGenerator.cpp
    plugin/CircuitExplorerPlugin.cpp
    plugin/io/VoltageSimulationHandler.cpp
    plugin/io/CellGrowthHandler.cpp
    plugin/io/SpikeSimulationHandler.cpp
    plugin/io/MorphologyCollageLoader.cpp
    plugin/io/PairSynapsesLoader.cpp
    plugin/io/AbstractCircuitLoader.cpp
    plugin/io/MeshCircuitLoader.cpp
    plugin/io/AdvancedCircuitLoader.cpp
    plugin/io/AstrocyteLoader.cpp
    plugin/io/SynapseCircuitLoader.cpp
    plugin/io/BrickLoader.cpp
    plugin/io/MorphologyLoader.cpp
    plugin/io/SynapseJSONLoader.cpp
    plugin/io/Utils.cpp
)

set(CIRCUITEXPLORER_PUBLIC_HEADERS
    plugin/api/CircuitExplorerParams.h
    plugin/meshing/MetaballsGenerator.h
    plugin/CircuitExplorerPlugin.h
    plugin/io/CellGrowthHandler.h
    plugin/io/VoltageSimulationHandler.h
    plugin/io/SpikeSimulationHandler.h
    plugin/io/BrickLoader.h
    plugin/io/AbstractCircuitLoader.h
    plugin/io/PairSynapsesLoader.h
    plugin/io/MeshCircuitLoader.h
    plugin/io/MorphologyCollageLoader.h
    plugin/io/AdvancedCircuitLoader.h
    plugin/io/AstrocyteLoader.h
    plugin/io/SynapseCircuitLoader.h
    plugin/io/MorphologyLoader.h
    plugin/io/SynapseJSONLoader.h
    plugin/io/Utils.h
)

if(${CGAL_FOUND})
    list(APPEND ${NAME}_SOURCES plugin/meshing/PointCloudMesher.cpp)
    list(APPEND ${NAME}_PUBLIC_HEADERS plugin/meshing/PointCloudMesher.h)
endif()

if(${PQXX_FOUND})
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
    module/ispc/render/ProximityDetectionRenderer.ispc
    module/ispc/render/CellGrowthRenderer.ispc
    module/ispc/render/utils/CircuitExplorerSimulationRenderer.ispc
    module/ispc/render/utils/CircuitExplorerRandomGenerator.ispc
    module/ispc/render/DefaultMaterial.ispc
)

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
    braynsOSPRayEngine glm vmmlib Brion Brain
    ${FREEIMAGE_LIBRARIES} ${HDF5_LIBRARIES}
)

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
