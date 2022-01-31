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

#include <plugin/api/CircuitExplorerParams.h>

#include <array>
#include <brayns/common/types.h>
#include <brayns/pluginapi/ExtensionPlugin.h>
#include <vector>

namespace circuitexplorer
{
using namespace brayns;
using namespace api;

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
    void _markModified() { _dirty = true; };

#ifdef USE_MORPHOLOGIES
    // Handlers
    Response _attachCellGrowthHandler(const AttachCellGrowthHandler& payload);
    Response _attachCircuitSimulationHandler(
        const AttachCircuitSimulationHandler& payload);
    Response _setConnectionsPerValue(const ConnectionsPerValue&);

#ifdef USE_PQXX
    // Database
    Response _importVolume(const ImportVolume&);
    Response _importCompartmentSimulation(const ImportCompartmentSimulation&);
    Response _importMorphology(const ImportMorphology&);
    Response _importMorphologyAsSDF(const ImportMorphology&);
#endif

    SynapseAttributes _synapseAttributes;
#endif

    // Rendering
    Response _setMaterial(const MaterialDescriptor&);
    Response _setMaterials(const MaterialsDescriptor&);
    Response _setMaterialRange(const MaterialRangeDescriptor&);
    Response _setMaterialExtraAttributes(const MaterialExtraAttributes&);
    MaterialIds _getMaterialIds(const ModelId& modelId);

    // Experimental
    Response _exportModelToFile(const ExportModelToFile&);
    Response _exportModelToMesh(const ExportModelToMesh&);

    // Add geometry
    void _createShapeMaterial(ModelPtr& model, const size_t id,
                              const Vector3d& color, const double& opacity);
    Response _addSphere(const AddSphere& payload);
    Response _addPill(const AddPill& payload);
    Response _addCylinder(const AddCylinder& payload);
    Response _addBox(const AddBox& payload);

    // Predefined models
    Response _addColumn(const AddColumn& payload);

    bool _dirty{false};
};
} // namespace circuitexplorer
