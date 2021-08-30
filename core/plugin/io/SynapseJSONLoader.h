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

#pragma once

#include <plugin/api/CircuitExplorerParams.h>

#include <brayns/common/loader/Loader.h>
#include <brayns/common/types.h>
#include <brayns/parameters/GeometryParameters.h>

#include <brain/brain.h>

#include <vector>

namespace circuitexplorer
{
using namespace brayns;

/**
 * Load circuit from BlueConfig or CircuitConfig file, including simulation.
 */
class SynapseJSONLoader : public Loader
{
public:
    SynapseJSONLoader(Scene& scene, const SynapseAttributes& synapseAttributes);

    std::string getName() const final;

    std::vector<std::string> getSupportedExtensions() const final;

    bool isSupported(const std::string& filename,
                     const std::string& extension) const final;

    ModelDescriptorPtr importFromBlob(
        Blob&& blob, const LoaderProgress& callback,
        const PropertyMap& properties) const final;

    ModelDescriptorPtr importFromFile(
        const std::string& filename, const LoaderProgress& callback,
        const PropertyMap& properties) const final;

    /**
     * @brief Imports synapses from a circuit for the given target name
     * @param circuitConfig URI of the Circuit Config file
     * @param gid Gid of the neuron
     * @param scene Scene into which the circuit is imported
     * @return True if the circuit is successfully loaded, false if the circuit
     * contains no cells.
     */
    ModelDescriptorPtr importSynapsesFromGIDs(
        const SynapseAttributes& synapseAttributes, const Vector3fs& colors);

private:
    const SynapseAttributes& _synapseAttributes;
};
} // namespace circuitexplorer
