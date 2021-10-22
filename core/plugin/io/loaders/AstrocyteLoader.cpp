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

#include "AstrocyteLoader.h"

#include <brayns/engineapi/Model.h>
#include <brayns/engineapi/Scene.h>
#include <common/CommonTypes.h>
#include <common/Logs.h>

#include <fstream>

namespace circuitexplorer
{
namespace io
{
namespace loader
{
const std::string LOADER_NAME = "Astrocytes";
const std::string SUPPORTED_EXTENTION_ASTROCYTES = "astrocytes";
const float DEFAULT_ASTROCYTE_MITOCHONDRIA_DENSITY = 0.05f;
const size_t NB_MATERIALS_PER_INSTANCE = 10;

AstrocyteLoader::AstrocyteLoader(
    Scene &scene, const ApplicationParameters &applicationParameters,
    PropertyMap &&loaderParams)
    : AbstractCircuitLoader(scene, applicationParameters,
                            std::move(loaderParams))
{
    PLUGIN_INFO("Registering " << LOADER_NAME);
    _fixedDefaults.setProperty(
        {PROP_DB_CONNECTION_STRING.name, std::string("")});
    _fixedDefaults.setProperty({PROP_DENSITY.name, 1.0});
    _fixedDefaults.setProperty({PROP_RANDOM_SEED.name, 0.0});
    _fixedDefaults.setProperty({PROP_REPORT.name, std::string("")});
    _fixedDefaults.setProperty({PROP_TARGETS.name, std::string("")});
    _fixedDefaults.setProperty({PROP_GIDS.name, std::string("")});
    _fixedDefaults.setProperty(
        {PROP_REPORT_TYPE.name, enumToString(ReportType::undefined)});
    _fixedDefaults.setProperty({PROP_RADIUS_MULTIPLIER.name, 1.0});
    _fixedDefaults.setProperty({PROP_RADIUS_CORRECTION.name, 0.0});
    _fixedDefaults.setProperty(
        {PROP_DAMPEN_BRANCH_THICKNESS_CHANGERATE.name, true});
    _fixedDefaults.setProperty({PROP_USE_REALISTIC_SOMA.name, false});
    _fixedDefaults.setProperty({PROP_METABALLS_SAMPLES_FROM_SOMA.name, 0});
    _fixedDefaults.setProperty({PROP_METABALLS_GRID_SIZE.name, 0});
    _fixedDefaults.setProperty({PROP_METABALLS_THRESHOLD.name, 0.0});
    _fixedDefaults.setProperty(
        {PROP_USER_DATA_TYPE.name, enumToString(UserDataType::undefined)});
    _fixedDefaults.setProperty({PROP_MORPHOLOGY_MAX_DISTANCE_TO_SOMA.name,
                                std::numeric_limits<double>::max()});
    _fixedDefaults.setProperty({PROP_MESH_FOLDER.name, std::string("")});
    _fixedDefaults.setProperty(
        {PROP_MESH_FILENAME_PATTERN.name, std::string("")});
    _fixedDefaults.setProperty({PROP_MESH_TRANSFORMATION.name, false});
    _fixedDefaults.setProperty({PROP_CELL_CLIPPING.name, false});
    _fixedDefaults.setProperty({PROP_AREAS_OF_INTEREST.name, 0});
    _fixedDefaults.setProperty({PROP_LOAD_AFFERENT_SYNAPSES.name, true});
    _fixedDefaults.setProperty({PROP_LOAD_EFFERENT_SYNAPSES.name, true});
    _fixedDefaults.setProperty(
        {PROP_PRESYNAPTIC_NEURON_GID.name, std::string("")});
    _fixedDefaults.setProperty(
        {PROP_POSTSYNAPTIC_NEURON_GID.name, std::string("")});
}

std::vector<std::string> AstrocyteLoader::getSupportedExtensions() const
{
    return {SUPPORTED_EXTENTION_ASTROCYTES};
}

bool AstrocyteLoader::isSupported(const std::string & /*filename*/,
                                  const std::string &extension) const
{
    const std::set<std::string> types = {SUPPORTED_EXTENTION_ASTROCYTES};
    return types.find(extension) != types.end();
}

ModelDescriptorPtr AstrocyteLoader::importFromFile(
    const std::string &filename, const LoaderProgress &callback,
    const PropertyMap &properties) const
{
    PropertyMap props = _defaults;
    props.merge(_fixedDefaults);
    props.merge(properties);

    std::vector<std::string> uris;
    std::ifstream file(filename);
    if (file.is_open())
    {
        std::string line;
        while (getline(file, line))
            uris.push_back(line);
        file.close();
    }

    PLUGIN_INFO("Loading " << uris.size() << " astrocytes from " << filename);
    callback.updateProgress("Loading astrocytes ...", 0);
    auto model = _scene.createModel();

    ModelDescriptorPtr modelDescriptor;
    _importMorphologiesFromURIs(props, uris, callback, *model);
    modelDescriptor =
        std::make_shared<ModelDescriptor>(std::move(model), filename);
    return modelDescriptor;
}

std::string AstrocyteLoader::getName() const
{
    return LOADER_NAME;
}

PropertyMap AstrocyteLoader::getCLIProperties()
{
    PropertyMap pm(LOADER_NAME);
    pm.setProperty(PROP_SECTION_TYPE_SOMA);
    pm.setProperty(PROP_SECTION_TYPE_AXON);
    pm.setProperty(PROP_SECTION_TYPE_DENDRITE);
    pm.setProperty(PROP_SECTION_TYPE_APICAL_DENDRITE);
    pm.setProperty(PROP_USE_SDF_GEOMETRY);
    pm.setProperty(PROP_CIRCUIT_COLOR_SCHEME);
    pm.setProperty(PROP_MORPHOLOGY_COLOR_SCHEME);
    pm.setProperty(PROP_MORPHOLOGY_QUALITY);
    pm.setProperty(PROP_INTERNALS);
    return pm;
}

void AstrocyteLoader::_importMorphologiesFromURIs(
    const PropertyMap &properties, const std::vector<std::string> &uris,
    const LoaderProgress &callback, Model &model) const
{
    PropertyMap morphologyProps(properties);
    MorphologyLoader loader(_scene, std::move(morphologyProps));

    const auto colorScheme = stringToEnum<CircuitColorScheme>(
        properties.getProperty<std::string>(PROP_CIRCUIT_COLOR_SCHEME.name));
    const auto generateInternals =
        properties.getProperty<bool>(PROP_INTERNALS.name);

    const float mitochondriaDensity =
        generateInternals ? DEFAULT_ASTROCYTE_MITOCHONDRIA_DENSITY : 0.f;

    for (uint64_t i = 0; i < uris.size(); ++i)
    {
        const auto uri = servus::URI(uris[i]);
        PLUGIN_DEBUG("Loading astrocyte from " << uri);
        const auto materialId = (colorScheme == CircuitColorScheme::by_id
                                     ? i * NB_MATERIALS_PER_INSTANCE
                                     : NO_MATERIAL);

        loader.setBaseMaterialId(materialId);

        MorphologyInfo morphologyInfo;
        morphologyInfo = loader.importMorphology(i, morphologyProps, uri, model,
                                                 i, SynapsesInfo(), Matrix4f(),
                                                 nullptr, mitochondriaDensity);
        callback.updateProgress("Loading astrocytes...",
                                (float)i / (float)uris.size());
    }
    PropertyMap materialProps;
    materialProps.setProperty({MATERIAL_PROPERTY_CAST_USER_DATA, false});
    materialProps.setProperty({MATERIAL_PROPERTY_SHADING_MODE,
                               static_cast<int>(MaterialShadingMode::diffuse)});
    materialProps.setProperty(
        {MATERIAL_PROPERTY_CLIPPING_MODE,
         static_cast<int>(MaterialClippingMode::no_clipping)});
    MorphologyLoader::createMissingMaterials(model, materialProps);
}
} // namespace loader
} // namespace io
} // namespace circuitexplorer