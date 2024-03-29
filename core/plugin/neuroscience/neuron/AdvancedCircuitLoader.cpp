/* Copyright (c) 2018-2022, EPFL/Blue Brain Project
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

#include "AdvancedCircuitLoader.h"

#include <common/Logs.h>

namespace circuitexplorer
{
namespace neuroscience
{
namespace neuron
{
const std::string LOADER_NAME = "Advanced circuit (Experimental)";

AdvancedCircuitLoader::AdvancedCircuitLoader(
    Scene &scene, const ApplicationParameters &applicationParameters,
    PropertyMap &&loaderParams)
    : AbstractCircuitLoader(scene, applicationParameters,
                            std::move(loaderParams))
{
    PLUGIN_INFO("Registering " << LOADER_NAME);
    _fixedDefaults.setProperty(
        {PROP_PRESYNAPTIC_NEURON_GID.name, std::string("")});
    _fixedDefaults.setProperty(
        {PROP_POSTSYNAPTIC_NEURON_GID.name, std::string("")});
}

ModelDescriptorPtr AdvancedCircuitLoader::importFromFile(
    const std::string &filename, const LoaderProgress &callback,
    const PropertyMap &properties) const
{
    PLUGIN_INFO("Loading circuit from " << filename);
    callback.updateProgress("Loading circuit ...", 0);
    PropertyMap props = _defaults;
    props.merge(_fixedDefaults);
    props.merge(properties);
    return importCircuit(filename, props, callback);
}

std::string AdvancedCircuitLoader::getName() const
{
    return LOADER_NAME;
}

PropertyMap AdvancedCircuitLoader::getCLIProperties()
{
    PropertyMap pm(LOADER_NAME);
    pm.setProperty(PROP_DB_CONNECTION_STRING);
    pm.setProperty(PROP_DENSITY);
    pm.setProperty(PROP_REPORT);
    pm.setProperty(PROP_REPORT_TYPE);
    pm.setProperty(PROP_SYNCHRONOUS_MODE);
    pm.setProperty(PROP_TARGETS);
    pm.setProperty(PROP_GIDS);
    pm.setProperty(PROP_CIRCUIT_COLOR_SCHEME);
    pm.setProperty(PROP_RANDOM_SEED);
    pm.setProperty(PROP_MESH_FOLDER);
    pm.setProperty(PROP_MESH_FILENAME_PATTERN);
    pm.setProperty(PROP_MESH_TRANSFORMATION);
    pm.setProperty(PROP_RADIUS_MULTIPLIER);
    pm.setProperty(PROP_RADIUS_CORRECTION);
    pm.setProperty(PROP_SECTION_TYPE_SOMA);
    pm.setProperty(PROP_SECTION_TYPE_AXON);
    pm.setProperty(PROP_SECTION_TYPE_DENDRITE);
    pm.setProperty(PROP_SECTION_TYPE_APICAL_DENDRITE);
    pm.setProperty(PROP_USE_SDF_SOMA);
    pm.setProperty(PROP_USE_SDF_BRANCHES);
    pm.setProperty(PROP_USE_SDF_NUCLEUS);
    pm.setProperty(PROP_USE_SDF_MITOCHONDRIA);
    pm.setProperty(PROP_USE_SDF_SYNAPSES);
    pm.setProperty(PROP_USE_SDF_MYELIN_STEATH);
    pm.setProperty(PROP_DAMPEN_BRANCH_THICKNESS_CHANGERATE);
    pm.setProperty(PROP_USER_DATA_TYPE);
    pm.setProperty(PROP_ASSET_COLOR_SCHEME);
    pm.setProperty(PROP_ASSET_QUALITY);
    pm.setProperty(PROP_MORPHOLOGY_MAX_DISTANCE_TO_SOMA);
    pm.setProperty(PROP_CELL_CLIPPING);
    pm.setProperty(PROP_AREAS_OF_INTEREST);
    pm.setProperty(PROP_LOAD_AFFERENT_SYNAPSES);
    pm.setProperty(PROP_LOAD_EFFERENT_SYNAPSES);
    pm.setProperty(PROP_INTERNALS);
    pm.setProperty(PROP_EXTERNALS);
    pm.setProperty(PROP_ALIGN_TO_GRID);
    return pm;
}
} // namespace neuron
} // namespace neuroscience
} // namespace circuitexplorer
