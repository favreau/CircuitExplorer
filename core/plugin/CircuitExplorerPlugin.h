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

#include <plugin/api/CircuitExplorerParams.h>
#include <plugin/io/AbstractCircuitLoader.h>

#include <array>
#include <brayns/common/types.h>
#include <brayns/pluginapi/ExtensionPlugin.h>
#include <vector>

namespace circuitexplorer
{
using namespace brayns;

/**
 * @brief The CircuitExplorerPlugin class manages the loading and visualization
 * of the Blue Brain Project micro-circuits, and allows visualisation of voltage
 * simulations
 */
class CircuitExplorerPlugin : public ExtensionPlugin
{
public:
    CircuitExplorerPlugin();

    void init() final;

    /**
     * @brief preRender Updates the scene according to latest data load
     */
    void preRender() final;

private:
    // Plug-in
    Response _getVersion() const;

    // Rendering
    void _setCamera(const CameraDefinition&);
    CameraDefinition _getCamera();
    void _setMaterial(const MaterialDescriptor&);
    void _setMaterials(const MaterialsDescriptor&);
    void _setMaterialRange(const MaterialRangeDescriptor&);
    void _setMaterialExtraAttributes(const MaterialExtraAttributes&);
    MaterialIds _getMaterialIds(const ModelId& modelId);

    // Experimental
    void _setSynapseAttributes(const SynapseAttributes&);
    void _setConnectionsPerValue(const ConnectionsPerValue&);
    void _setMetaballsPerSimulationValue(const MetaballsFromSimulationValue&);
    void _exportModelToFile(const ExportModelToFile&);
    void _exportModelToMesh(const ExportModelToMesh&);

    // Handlers
    void _attachCellGrowthHandler(const AttachCellGrowthHandler& payload);
    void _attachCircuitSimulationHandler(
        const AttachCircuitSimulationHandler& payload);

    // Anterograde tracing
    AnterogradeTracingResult _traceAnterogrades(
        const AnterogradeTracing& payload);

    // Add geometry
    void _createShapeMaterial(ModelPtr& model, const size_t id,
                              const Vector3d& color, const double& opacity);
    AddShapeResult _addSphere(const AddSphere& payload);
    AddShapeResult _addPill(const AddPill& payload);
    AddShapeResult _addCylinder(const AddCylinder& payload);
    AddShapeResult _addBox(const AddBox& payload);

    // Predefined models
    void _addGrid(const AddGrid& payload);
    void _addColumn(const AddColumn& payload);

#ifdef USE_PQXX
    // Database
    Response _importVolume(const ImportVolume&);
    Response _importCompartmentSimulation(const ImportCompartmentSimulation&);
    Response _importMorphology(const ImportMorphology&);
    Response _importMorphologyAsSDF(const ImportMorphology&);
#endif

    SynapseAttributes _synapseAttributes;

    bool _dirty{false};
};
} // namespace circuitexplorer
