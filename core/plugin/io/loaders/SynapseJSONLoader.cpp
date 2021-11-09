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

#include <common/CommonTypes.h>
#include <common/Logs.h>

#include "SynapseJSONLoader.h"

#include <brion/brion.h>

#include <brayns/engineapi/Material.h>
#include <brayns/engineapi/Model.h>
#include <brayns/engineapi/Scene.h>
#include <brayns/parameters/ParametersManager.h>

namespace circuitexplorer
{
namespace io
{
namespace loader
{
const std::string LOADER_NAME = "Synapses (JSON)";
const std::string SUPPORTED_EXTENTION_JSON = "json";

SynapseJSONLoader::SynapseJSONLoader(Scene& scene,
                                     const SynapseAttributes& synapseAttributes)
    : Loader(scene)
    , _synapseAttributes(synapseAttributes)
{
    PLUGIN_INFO("Registering " << LOADER_NAME);
}

std::string SynapseJSONLoader::getName() const
{
    return LOADER_NAME;
}

std::vector<std::string> SynapseJSONLoader::getSupportedExtensions() const
{
    return {SUPPORTED_EXTENTION_JSON};
}

bool SynapseJSONLoader::isSupported(const std::string& /*filename*/,
                                    const std::string& extension) const
{
    const std::set<std::string> types = {SUPPORTED_EXTENTION_JSON};
    return types.find(extension) != types.end();
}

ModelDescriptorPtr SynapseJSONLoader::importFromBlob(
    Blob&& /*blob*/, const LoaderProgress& /*callback*/,
    const PropertyMap& /*properties*/) const
{
    throw std::runtime_error("Loading circuit from blob is not supported");
}

ModelDescriptorPtr SynapseJSONLoader::importFromFile(
    const std::string& /*filename*/, const LoaderProgress& /*callback*/,
    const PropertyMap& /*properties*/) const
{
    throw std::runtime_error("Loading circuit from file is not supported");
}

ModelDescriptorPtr SynapseJSONLoader::importSynapsesFromGIDs(
    const SynapseAttributes& synapseAttributes, const Vector3fs& colors)
{
    const brion::BlueConfig bc(synapseAttributes.circuitConfiguration);
    const brain::Circuit circuit(bc);
    const brain::GIDSet gids = {static_cast<uint32_t>(synapseAttributes.gid)};

    const brain::Synapses& synapses =
        circuit.getAfferentSynapses(gids, brain::SynapsePrefetch::all);

    if (synapses.empty())
        throw std::runtime_error(
            "No synapse could be found for the give GID set");

    // Load synapses
    PLUGIN_DEBUG("Loading " << synapses.size() << " synapses");
    auto model = _scene.createModel();
    size_t i = 0;
    for (auto synapse : synapses)
    {
        if (i >= colors.size())
            throw std::runtime_error(
                "Invalid number of colors. Expected " +
                std::to_string(synapses.size()) +
                ", provided: " + std::to_string(colors.size()));

        auto material = model->createMaterial(i, std::to_string(i), {});
        material->setDiffuseColor(colors[i]);
        material->updateProperty(MATERIAL_PROPERTY_SHADING_MODE,
                                 static_cast<int32_t>(
                                     MaterialShadingMode::none));
        material->setEmission(synapseAttributes.lightEmission);

        const auto pre = synapse.getPresynapticCenterPosition();
        const auto radius = _synapseAttributes.radius;
        model->addSphere(i, {{pre.x, pre.y, pre.z}, radius});
        ++i;
    }

    // Construct model
    Transformation transformation;
    transformation.setRotationCenter(model->getBounds().getCenter());
    ModelMetadata metaData = {{"Circuit",
                               synapseAttributes.circuitConfiguration},
                              {"Number of synapses",
                               std::to_string(synapses.size())}};

    auto modelDescriptor =
        std::make_shared<ModelDescriptor>(std::move(model),
                                          std::to_string(synapseAttributes.gid),
                                          metaData);
    modelDescriptor->setTransformation(transformation);
    return modelDescriptor;
}
} // namespace loader
} // namespace io
} // namespace circuitexplorer
