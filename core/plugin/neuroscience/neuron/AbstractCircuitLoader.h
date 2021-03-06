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

#pragma once

#include <common/Types.h>
#include <plugin/api/CircuitExplorerParams.h>
#include <plugin/neuroscience/common/Types.h>

#include <brayns/common/loader/Loader.h>
#include <brayns/common/types.h>

#include <set>
#include <vector>

namespace servus
{
class URI;
}

namespace circuitexplorer
{
namespace neuroscience
{
namespace neuron
{
using namespace brayns;
using namespace common;

/**
 * Load circuit from BlueConfig or CircuitConfig file, including simulation.
 */
class AbstractCircuitLoader : public Loader
{
public:
    AbstractCircuitLoader(Scene &scene,
                          const ApplicationParameters &applicationParameters,
                          PropertyMap &&loaderParams);

    PropertyMap getProperties() const final;

    std::vector<std::string> getSupportedExtensions() const;

    bool isSupported(const std::string &filename,
                     const std::string &extension) const;

    ModelDescriptorPtr importFromBlob(Blob &&blob,
                                      const LoaderProgress &callback,
                                      const PropertyMap &properties) const;

    /**
     * @brief Imports morphology from a circuit for the given target name
     * @param circuitConfig URI of the Circuit Config file
     * @return ModelDescriptor if the circuit is successfully loaded, nullptr if
     * the circuit contains no cells.
     */
    ModelDescriptorPtr importCircuit(const std::string &circuitConfig,
                                     const PropertyMap &properties,
                                     const LoaderProgress &callback) const;

    /**
     * @brief _populateLayerIds populates the neuron layer IDs. This is
     * currently only supported for the MVD2 format.
     * @param blueConfig Configuration of the circuit
     * @param gids GIDs of the neurons
     */
    size_ts _populateLayerIds(const PropertyMap &props,
                              const brion::BlueConfig &blueConfig,
                              const brain::GIDSet &gids) const;

    static void setSimulationTransferFunction(TransferFunction &tf,
                                              const float finalOpacity = 1.f);

protected:
    const ApplicationParameters &_applicationParameters;
    PropertyMap _defaults;
    PropertyMap _fixedDefaults;

private:
    std::vector<std::string> _getTargetsAsStrings(
        const std::string &targets) const;

    brain::GIDSet _getGids(const PropertyMap &properties,
                           const brion::BlueConfig &blueConfiguration,
                           const brain::Circuit &circuit,
                           GIDOffsets &targetGIDOffsets) const;

    std::string _getMeshFilenameFromGID(const PropertyMap &props,
                                        const uint64_t gid) const;

    float _importMorphologies(
        const PropertyMap &props, const brain::Circuit &circuit, Model &model,
        const brain::GIDSet &gids, const Matrix4fs &transformations,
        const GIDOffsets &targetGIDOffsets,
        CompartmentReportPtr compartmentReport, const size_ts &layerIds,
        const size_ts &morphologyTypes, const size_ts &electrophysiologyTypes,
        const LoaderProgress &callback,
        const size_t materialId = NO_MATERIAL) const;

    /**
     * @brief _getMaterialFromSectionType return a material determined by the
     * --color-scheme geometry parameter
     * @param index Index of the element to which the material will attached
     * @param material Material that is forced in case geometry parameters
     * do not apply
     * @param sectionType Section type of the geometry to which the material
     * will be applied
     * @return Material ID determined by the geometry parameters
     */
    size_t _getMaterialFromCircuitAttributes(
        const PropertyMap &props, const uint64_t index, const size_t material,
        const GIDOffsets &targetGIDOffsets, const size_ts &layerIds,
        const size_ts &morphologyTypes, const size_ts &electrophysiologyTypes,
        const bool forSimulationModel) const;

    void _importMeshes(const PropertyMap &props, Model &model,
                       const brain::GIDSet &gids,
                       const Matrix4fs &transformations,
                       const GIDOffsets &targetGIDOffsets,
                       const size_ts &layerIds, const size_ts &morphologyTypes,
                       const size_ts &electrophysiologyTypes,
                       const LoaderProgress &callback) const;

    CompartmentReportPtr _attachSimulationHandler(
        const PropertyMap &properties,
        const brion::BlueConfig &blueConfiguration, Model &model,
        const ReportType &reportType, brain::GIDSet &gids) const;

    void _filterGIDsWithClippingPlanes(brain::GIDSet &gids,
                                       Matrix4fs &transformations) const;

    void _filterGIDsWithAreasOfInterest(const uint16_t areasOfInterest,
                                        brain::GIDSet &gids,
                                        Matrix4fs &transformations) const;

    bool _isClipped(const Vector3f &position) const;

    void _setDefaultCircuitColorMap(Model &model) const;

// Synapses
#if 0
    void _loadPairSynapses(const PropertyMap &properties,
                           const brain::Circuit &circuit,
                           const uint32_t &preGid, const uint32_t &postGid,
                           const float synapseRadius, Model &model) const;
#endif
};
} // namespace neuron
} // namespace neuroscience
} // namespace circuitexplorer
