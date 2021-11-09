/* Copyright (c) 2018-2021, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3.0 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include "Utils.h"

#include <Defines.h>

#include <brayns/common/PropertyMap.h>
#include <brayns/common/geometry/Cone.h>
#include <brayns/common/geometry/Cylinder.h>
#include <brayns/common/geometry/SDFGeometry.h>
#include <brayns/common/geometry/Sphere.h>
#include <brayns/common/mathTypes.h>
#include <brayns/common/types.h>
#include <brayns/engineapi/Model.h>

#include <brain/brain.h>
#include <brion/brion.h>

#include <memory>
#include <vector>

#include <glm/gtx/matrix_decompose.hpp>

namespace circuitexplorer
{
/** Additional marterial attributes */
const std::string MATERIAL_PROPERTY_CAST_USER_DATA = "cast_simulation_data";
const std::string MATERIAL_PROPERTY_SHADING_MODE = "shading_mode";
const std::string MATERIAL_PROPERTY_CLIPPING_MODE = "clipping_mode";
const std::string MATERIAL_PROPERTY_USER_PARAMETER = "user_parameter";

/** */
typedef uint32_t Gid;

/** Circuit color scheme */
enum class CircuitColorScheme
{
    none = 0,
    by_id = 1,
    by_type = 2,
    by_layer = 3,
    by_mtype = 4,
    by_etype = 5,
    by_target = 6
};

const size_t MATERIAL_OFFSET_SOMA = 1;
const size_t MATERIAL_OFFSET_AXON = 2;
const size_t MATERIAL_OFFSET_DENDRITE = 3;
const size_t MATERIAL_OFFSET_APICAL_DENDRITE = 4;
const size_t MATERIAL_OFFSET_AFFERENT_SYNPASE = 5;
const size_t MATERIAL_OFFSET_EFFERENT_SYNPASE = 6;
const size_t MATERIAL_OFFSET_MITOCHONDRION = 7;
const size_t MATERIAL_OFFSET_NUCLEUS = 8;

// Empirical amplitude & frequency
const brayns::Vector3f DISPLACEMENT_PARAMS = {0.05f, 2.0f, 0.f};

/** Morphology color scheme */
enum class MorphologyColorScheme
{
    none = 0,
    neuron_by_segment_type = 1
};

enum class ReportType
{
    undefined = 0,
    voltages_from_file = 1,
    spikes = 2
};

/** User data type */
enum class UserDataType
{
    undefined = 0,
    simulation_offset = 1,
    distance_to_soma = 2
};

enum class MorphologyQuality
{
    low = 0,
    medium = 1,
    high = 2
};

/* Returns a mapping from a name to an enum type. */
template <typename EnumT>
std::vector<std::pair<std::string, EnumT>> enumerateMap();

/* Convert a string to an enum. */
template <typename EnumT>
inline EnumT stringToEnum(const std::string& v)
{
    for (const auto& p : enumerateMap<EnumT>())
        if (p.first == v)
            return p.second;

    throw std::runtime_error("Could not match enum '" + v + "'");
    return static_cast<EnumT>(0);
}

/* Convert an enum to a string. */
template <typename EnumT>
inline std::string enumToString(const EnumT v)
{
    for (const auto& p : enumerateMap<EnumT>())
        if (p.second == v)
            return p.first;

    throw std::runtime_error("Could not match enum");
    return "Invalid";
}

/* Returns all names for given enum type 'EnumT' */
template <typename EnumT>
inline std::vector<std::string> enumerateNames()
{
    std::vector<std::string> v;
    for (const auto& p : enumerateMap<EnumT>())
        v.push_back(p.first);
    return v;
}

template <>
inline std::vector<std::pair<std::string, CircuitColorScheme>> enumerateMap()
{
    return {{"None", CircuitColorScheme::none},
            {"By id", CircuitColorScheme::by_id},
            {"By layer", CircuitColorScheme::by_layer},
            {"By mtype", CircuitColorScheme::by_mtype},
            {"By etype", CircuitColorScheme::by_etype},
            {"By target", CircuitColorScheme::by_target}};
}

template <>
inline std::vector<std::pair<std::string, ReportType>> enumerateMap()
{
    return {{"Undefined", ReportType::undefined},
            {"Voltages from file", ReportType::voltages_from_file},
            {"Spikes", ReportType::spikes}};
}

template <>
inline std::vector<std::pair<std::string, UserDataType>> enumerateMap()
{
    return {{"Undefined", UserDataType::undefined},
            {"Simulation offset", UserDataType::simulation_offset},
            {"Distance to soma", UserDataType::distance_to_soma}};
}

template <>
inline std::vector<std::pair<std::string, MorphologyColorScheme>> enumerateMap()
{
    return {{"None", MorphologyColorScheme::none},
            {"By segment type", MorphologyColorScheme::neuron_by_segment_type}};
}

template <>
inline std::vector<std::pair<std::string, MorphologyQuality>> enumerateMap()
{
    return {{"Low", MorphologyQuality::low},
            {"Medium", MorphologyQuality::medium},
            {"High", MorphologyQuality::high}};
}

template <>
inline std::vector<std::pair<std::string, bool>> enumerateMap()
{
    return {{"On", true}, {"Off", false}};
}

const std::string CIRCUIT_ON_OFF[2] = {"off", "on"};

// clang-format off
const brayns::Property PROP_DB_CONNECTION_STRING = {
    "000DbConnectionString", std::string(""),
    {"Connection string to the database"}};
const brayns::Property PROP_DENSITY = {
    "001Density", 1.0,
    {"Density of cells in the circuit in percent"}};
const brayns::Property PROP_RANDOM_SEED = {
    "002RandomSeed", 0.0,
    {"Random seed for target subsetting"}};
const brayns::Property PROP_TARGETS = {
    "010Targets",  std::string(""),
    {"Circuit targets [comma separated list of labels]"}};
const brayns::Property PROP_GIDS = {
    "011Gids",  std::string(""),
    {"Circuit GIDs [comma separated list of GIDs]"}};
const brayns::Property PROP_PRESYNAPTIC_NEURON_GID = {
    "012PreNeuron",  std::string(""),
    {"Pre-synaptic neuron GID"}};
const brayns::Property PROP_POSTSYNAPTIC_NEURON_GID = {
    "013PostNeuron",  std::string(""),
    {"Post-synaptic neuron GID"}};
const brayns::Property PROP_REPORT{
    "020Report", std::string(),
    {"Circuit report"}};
const brayns::Property PROP_REPORT_TYPE = {
    "021ReportType", enumToString(ReportType::undefined),
    enumerateNames<ReportType>(),
    {"Type of simulation report"}};
const brayns::Property PROP_USER_DATA_TYPE = {
    "022UserDataType", enumToString(UserDataType::undefined),
    enumerateNames<UserDataType>(),
    {"Type of data attached to morphology segments"}};
const brayns::Property PROP_SYNCHRONOUS_MODE = {
    "023SynchronousMode", false, {"Synchronous mode"}};
const brayns::Property PROP_CIRCUIT_COLOR_SCHEME = {
    "030CircuitColorScheme", enumToString(CircuitColorScheme::none),
    enumerateNames<CircuitColorScheme>(),
    {"Color scheme to be applied to the circuit"}};
const brayns::Property PROP_MESH_FOLDER = {
    "040MeshFolder", std::string(), {"Folder constaining meshes"}};
const brayns::Property PROP_MESH_FILENAME_PATTERN = {
    "041MeshFilenamePattern", std::string("mesh_{gid}.obj"), {"File name pattern for meshes"}};
const brayns::Property PROP_MESH_TRANSFORMATION = {
    "042MeshTransformation", false, {"Apply circuit transformation to meshes"}};
const brayns::Property PROP_RADIUS_MULTIPLIER = {
    "050RadiusMultiplier", double(1.0),
    {"Multiplier applied to morphology radius"}};
const brayns::Property PROP_RADIUS_CORRECTION = {
    "051RadiusCorrection", double(0.0),
    {"Value overrideing the radius of the morphology"}};
const brayns::Property PROP_SECTION_TYPE_SOMA = {
    "052SectionTypeSoma", true,
    {"Soma"}};
const brayns::Property PROP_SECTION_TYPE_AXON = {
    "053SectionTypeAxon", true,
    {"Axon"}};
const brayns::Property PROP_SECTION_TYPE_DENDRITE = {
    "054SectionTypeDendrite", true,
    {"Dendrite"}};
const brayns::Property PROP_SECTION_TYPE_APICAL_DENDRITE = {
    "055SectionTypeApicalDendrite", true,
    {"Apical Dendrite"}};
const brayns::Property PROP_USE_SDF_GEOMETRY = {
    "060UseSdfgeometry", true,
    { "Use signed distance field geometry"}};
const brayns::Property PROP_DAMPEN_BRANCH_THICKNESS_CHANGERATE = {
    "061DampenBranchThicknessChangerate", true,
    {"Dampen branch thickness changerate"}};
const brayns::Property PROP_MORPHOLOGY_COLOR_SCHEME = {
    "080MorphologyColorScheme", enumToString(MorphologyColorScheme::none),
    enumerateNames<MorphologyColorScheme>(),
    {"Color scheme to be applied to the morphology"}};
const brayns::Property PROP_MORPHOLOGY_QUALITY = {
    "090MorphologyQuality", enumToString(MorphologyQuality::high),
    enumerateNames<MorphologyQuality>(),
    {"Quality of the morphology"}};
const brayns::Property PROP_MORPHOLOGY_MAX_DISTANCE_TO_SOMA = {
    "091MaxDistanceToSoma", std::numeric_limits<double>::max(),
    {"Maximum distance to soma"}};
const brayns::Property PROP_CELL_CLIPPING = {
    "100CellClipping", false,
    {"Clip cells according to scene-defined clipping planes"}};
const brayns::Property PROP_AREAS_OF_INTEREST = {
    "101AreasOfInterest", 0,
    {"Loads only one cell per area of interest"}};
const brayns::Property PROP_LOAD_AFFERENT_SYNAPSES = {
    "110LoadAfferentSynapses", false, {"Loads afferent synapses"}};
const brayns::Property PROP_LOAD_EFFERENT_SYNAPSES = {
    "111LoadEfferentSynapses", false, {"Loads efferent synapses"}};
const brayns::Property PROP_INTERNALS = {
    "120Internals", false, {"Generate internals (mitochondria and nucleus)"}};
// clang-format on

struct MorphologyInfo
{
    brayns::Vector3d somaPosition;
    brayns::Boxd bounds;
    float maxDistanceToSoma;
};

// Synapses
struct SynapsesInfo
{
    std::unique_ptr<brain::Synapses> afferentSynapses{nullptr};
    std::unique_ptr<brain::Synapses> efferentSynapses{nullptr};
    bool prePostSynapticUsecase{false};
    Gid preGid;
    Gid postGid;
};

enum class SynapseType
{
    afferent,
    efferent
};

} // namespace circuitexplorer
