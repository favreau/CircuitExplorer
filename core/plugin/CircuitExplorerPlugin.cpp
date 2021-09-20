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

#include "CircuitExplorerPlugin.h"
#include <common/CommonTypes.h>
#include <common/Logs.h>

#include <plugin/io/AdvancedCircuitLoader.h>
#include <plugin/io/AstrocyteLoader.h>
#include <plugin/io/BrickLoader.h>
#include <plugin/io/CellGrowthHandler.h>
#include <plugin/io/MeshCircuitLoader.h>
#include <plugin/io/MorphologyCollageLoader.h>
#include <plugin/io/MorphologyLoader.h>
#include <plugin/io/PairSynapsesLoader.h>
#include <plugin/io/SynapseCircuitLoader.h>
#include <plugin/io/SynapseJSONLoader.h>
#include <plugin/io/VoltageSimulationHandler.h>
#include <plugin/meshing/PointCloudMesher.h>

#ifdef USE_PQXX
#include <plugin/io/db/DBConnector.h>
#endif

#include <brayns/common/ActionInterface.h>
#include <brayns/common/Progress.h>
#include <brayns/common/Timer.h>
#include <brayns/common/geometry/Streamline.h>
#include <brayns/common/utils/imageUtils.h>
#include <brayns/engineapi/Camera.h>
#include <brayns/engineapi/Engine.h>
#include <brayns/engineapi/FrameBuffer.h>
#include <brayns/engineapi/Material.h>
#include <brayns/engineapi/Model.h>
#include <brayns/engineapi/Scene.h>
#include <brayns/parameters/ParametersManager.h>
#include <brayns/pluginapi/Plugin.h>

#include <brion/brion.h>

#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <random>
#include <regex>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#if 1
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/Skin_surface_3.h>
#include <CGAL/Union_of_balls_3.h>
#include <CGAL/mesh_skin_surface_3.h>
#include <CGAL/mesh_union_of_balls_3.h>
#include <CGAL/subdivide_union_of_balls_mesh_3.h>

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef CGAL::Skin_surface_traits_3<K> Traits;
typedef K::Point_3 Point_3;
typedef K::Weighted_point_3 Weighted_point;
typedef CGAL::Polyhedron_3<K> Polyhedron;
typedef CGAL::Skin_surface_traits_3<K> Traits;
typedef CGAL::Skin_surface_3<Traits> Skin_surface_3;
typedef CGAL::Union_of_balls_3<Traits> Union_of_balls_3;
#endif

namespace circuitexplorer
{
using namespace brayns;

#define REGISTER_LOADER(LOADER, FUNC) \
    registry.registerLoader({std::bind(&LOADER::getSupportedDataTypes), FUNC});

const std::string PLUGIN_API_PREFIX = "ce-";

const std::string ANTEROGRADE_TYPE_AFFERENT = "afferent";
const std::string ANTEROGRADE_TYPE_AFFERENTEXTERNAL = "projection";
const std::string ANTEROGRADE_TYPE_EFFERENT = "efferent";

void _addAdvancedSimulationRenderer(Engine& engine)
{
    PLUGIN_INFO("Registering advanced renderer");
    PropertyMap properties;
    properties.setProperty(
        {"giDistance", 10000., {"Global illumination distance"}});
    properties.setProperty(
        {"giWeight", 0., 1., 1., {"Global illumination weight"}});
    properties.setProperty(
        {"giSamples", 0, 0, 64, {"Global illumination samples"}});
    properties.setProperty({"shadows", 0., 0., 1., {"Shadow intensity"}});
    properties.setProperty({"softShadows", 0., 0., 1., {"Shadow softness"}});
    properties.setProperty(
        {"softShadowsSamples", 1, 1, 64, {"Soft shadow samples"}});
    properties.setProperty(
        {"epsilonFactor", 1., 1., 1000., {"Epsilon factor"}});
    properties.setProperty({"samplingThreshold",
                            0.001,
                            0.001,
                            1.,
                            {"Threshold under which sampling is ignored"}});
    properties.setProperty({"volumeSpecularExponent",
                            20.,
                            1.,
                            100.,
                            {"Volume specular exponent"}});
    properties.setProperty(
        {"volumeAlphaCorrection", 0.5, 0.001, 1., {"Volume alpha correction"}});
    properties.setProperty({"maxDistanceToSecondaryModel",
                            30.,
                            0.1,
                            100.,
                            {"Maximum distance to secondary model"}});
    properties.setProperty({"exposure", 1., 0.01, 10., {"Exposure"}});
    properties.setProperty({"fogStart", 0., 0., 1e6, {"Fog start"}});
    properties.setProperty({"fogThickness", 1e6, 1e6, 1e6, {"Fog thickness"}});
    properties.setProperty(
        {"maxBounces", 3, 1, 100, {"Maximum number of ray bounces"}});
    properties.setProperty({"useHardwareRandomizer",
                            false,
                            {"Use hardware accelerated randomizer"}});
    engine.addRendererType("circuit_explorer_advanced", properties);
}

void _addBasicSimulationRenderer(Engine& engine)
{
    PLUGIN_INFO("Registering basic renderer");

    PropertyMap properties;
    properties.setProperty(
        {"alphaCorrection", 0.5, 0.001, 1., {"Alpha correction"}});
    properties.setProperty(
        {"simulationThreshold", 0., 0., 1., {"Simulation threshold"}});
    properties.setProperty({"maxDistanceToSecondaryModel",
                            30.,
                            0.1,
                            100.,
                            {"Maximum distance to secondary model"}});
    properties.setProperty({"exposure", 1., 0.01, 10., {"Exposure"}});
    properties.setProperty(
        {"maxBounces", 3, 1, 100, {"Maximum number of ray bounces"}});
    properties.setProperty({"useHardwareRandomizer",
                            false,
                            {"Use hardware accelerated randomizer"}});
    engine.addRendererType("circuit_explorer_basic", properties);
}

void _addVoxelizedSimulationRenderer(Engine& engine)
{
    PLUGIN_INFO("Registering voxelized Simulation renderer");

    PropertyMap properties;
    properties.setProperty(
        {"alphaCorrection", 0.5, 0.001, 1., {"Alpha correction"}});
    properties.setProperty(
        {"simulationThreshold", 0., 0., 1., {"Simulation threshold"}});
    properties.setProperty({"exposure", 1., 0.01, 10., {"Exposure"}});
    properties.setProperty({"fogStart", 0., 0., 1e6, {"Fog start"}});
    properties.setProperty({"fogThickness", 1e6, 1e6, 1e6, {"Fog thickness"}});
    properties.setProperty(
        {"maxBounces", 3, 1, 100, {"Maximum number of ray bounces"}});
    properties.setProperty({"useHardwareRandomizer",
                            false,
                            {"Use hardware accelerated randomizer"}});
    engine.addRendererType("circuit_explorer_voxelized_simulation", properties);
}

void _addGrowthRenderer(Engine& engine)
{
    PLUGIN_INFO("Registering cell growth renderer");

    PropertyMap properties;
    properties.setProperty(
        {"alphaCorrection", 0.5, 0.001, 1., {"Alpha correction"}});
    properties.setProperty(
        {"simulationThreshold", 0., 0., 1., {"Simulation threshold"}});
    properties.setProperty({"exposure", 1., 0.01, 10., {"Exposure"}});
    properties.setProperty({"fogStart", 0., 0., 1e6, {"Fog start"}});
    properties.setProperty({"fogThickness", 1e6, 1e6, 1e6, {"Fog thickness"}});
    properties.setProperty({"tfColor", false, {"Use transfer function color"}});
    properties.setProperty({"shadows", 0., 0., 1., {"Shadow intensity"}});
    properties.setProperty({"softShadows", 0., 0., 1., {"Shadow softness"}});
    properties.setProperty(
        {"shadowDistance", 1e4, 0., 1e4, {"Shadow distance"}});
    properties.setProperty({"useHardwareRandomizer",
                            false,
                            {"Use hardware accelerated randomizer"}});
    engine.addRendererType("circuit_explorer_cell_growth", properties);
}

void _addProximityRenderer(Engine& engine)
{
    PLUGIN_INFO("Registering proximity detection renderer");

    PropertyMap properties;
    properties.setProperty(
        {"alphaCorrection", 0.5, 0.001, 1., {"Alpha correction"}});
    properties.setProperty({"detectionDistance", 1., {"Detection distance"}});
    properties.setProperty({"detectionFarColor",
                            std::array<double, 3>{{1., 0., 0.}},
                            {"Detection far color"}});
    properties.setProperty({"detectionNearColor",
                            std::array<double, 3>{{0., 1., 0.}},
                            {"Detection near color"}});
    properties.setProperty({"detectionOnDifferentMaterial",
                            false,
                            {"Detection on different material"}});
    properties.setProperty(
        {"surfaceShadingEnabled", true, {"Surface shading"}});
    properties.setProperty(
        {"maxBounces", 3, 1, 100, {"Maximum number of ray bounces"}});
    properties.setProperty({"exposure", 1., 0.01, 10., {"Exposure"}});
    properties.setProperty({"useHardwareRandomizer",
                            false,
                            {"Use hardware accelerated randomizer"}});
    engine.addRendererType("circuit_explorer_proximity_detection", properties);
}

void _addDOFPerspectiveCamera(Engine& engine)
{
    PLUGIN_INFO("Registering DOF perspective camera");

    PropertyMap properties;
    properties.setProperty({"fovy", 45., .1, 360., {"Field of view"}});
    properties.setProperty({"aspect", 1., {"Aspect ratio"}});
    properties.setProperty({"apertureRadius", 0., {"Aperture radius"}});
    properties.setProperty({"focusDistance", 1., {"Focus Distance"}});
    properties.setProperty({"enableClippingPlanes", true, {"Clipping"}});
    engine.addCameraType("circuit_explorer_dof_perspective", properties);
}

void _addSphereClippingPerspectiveCamera(Engine& engine)
{
    PLUGIN_INFO("Registering sphere clipping perspective camera");

    PropertyMap properties;
    properties.setProperty({"fovy", 45., .1, 360., {"Field of view"}});
    properties.setProperty({"aspect", 1., {"Aspect ratio"}});
    properties.setProperty({"apertureRadius", 0., {"Aperture radius"}});
    properties.setProperty({"focusDistance", 1., {"Focus Distance"}});
    properties.setProperty({"enableClippingPlanes", true, {"Clipping"}});
    engine.addCameraType("circuit_explorer_sphere_clipping", properties);
}

std::string _sanitizeString(const std::string& input)
{
    static const std::vector<std::string> sanitetizeItems = {"\"", "\\", "'",
                                                             ";",  "&",  "|",
                                                             "`"};

    std::string result = "";

    for (size_t i = 0; i < input.size(); i++)
    {
        bool found = false;
        for (const auto& token : sanitetizeItems)
        {
            if (std::string(1, input[i]) == token)
            {
                result += "\\" + token;
                found = true;
                break;
            }
        }
        if (!found)
        {
            result += std::string(1, input[i]);
        }
    }
    return result;
}

std::vector<std::string> _splitString(const std::string& source,
                                      const char token)
{
    std::vector<std::string> result;
    std::string split;
    std::istringstream ss(source);
    while (std::getline(ss, split, token))
        result.push_back(split);

    return result;
}

CircuitExplorerPlugin::CircuitExplorerPlugin()
    : ExtensionPlugin()
{
}

void CircuitExplorerPlugin::init()
{
    auto& scene = _api->getScene();
    auto& registry = scene.getLoaderRegistry();
    auto& pm = _api->getParametersManager();

    // Loaders
    registry.registerLoader(
        std::make_unique<BrickLoader>(scene, BrickLoader::getCLIProperties()));

    registry.registerLoader(
        std::make_unique<SynapseJSONLoader>(scene,
                                            std::move(_synapseAttributes)));

    registry.registerLoader(std::make_unique<SynapseCircuitLoader>(
        scene, pm.getApplicationParameters(),
        SynapseCircuitLoader::getCLIProperties()));

    registry.registerLoader(std::make_unique<MorphologyLoader>(
        scene, MorphologyLoader::getCLIProperties()));

    registry.registerLoader(std::make_unique<AdvancedCircuitLoader>(
        scene, pm.getApplicationParameters(),
        AdvancedCircuitLoader::getCLIProperties()));

    registry.registerLoader(std::make_unique<MorphologyCollageLoader>(
        scene, pm.getApplicationParameters(),
        MorphologyCollageLoader::getCLIProperties()));

    registry.registerLoader(std::make_unique<MeshCircuitLoader>(
        scene, pm.getApplicationParameters(),
        MeshCircuitLoader::getCLIProperties()));

    registry.registerLoader(std::make_unique<PairSynapsesLoader>(
        scene, pm.getApplicationParameters(),
        PairSynapsesLoader::getCLIProperties()));

    registry.registerLoader(
        std::make_unique<AstrocyteLoader>(scene, pm.getApplicationParameters(),
                                          AstrocyteLoader::getCLIProperties()));

    // Renderers
    auto& engine = _api->getEngine();
    _addAdvancedSimulationRenderer(engine);
    _addBasicSimulationRenderer(engine);
    _addVoxelizedSimulationRenderer(engine);
    _addGrowthRenderer(engine);
    _addProximityRenderer(engine);
    _addDOFPerspectiveCamera(engine);
    _addSphereClippingPerspectiveCamera(engine);

    _api->getParametersManager().getRenderingParameters().setCurrentRenderer(
        "circuit_explorer_advanced");

    // End-points
    auto actionInterface = _api->getActionInterface();
    if (actionInterface)
    {
        std::string endPoint = PLUGIN_API_PREFIX + "get-version";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        actionInterface->registerRequest<Response>(endPoint, [&]() {
            return _getVersion();
        });

        endPoint = PLUGIN_API_PREFIX + "set-material";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        actionInterface->registerNotification<MaterialDescriptor>(
            endPoint,
            [&](const MaterialDescriptor& param) { _setMaterial(param); });

        endPoint = PLUGIN_API_PREFIX + "set-materials";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        actionInterface->registerNotification<MaterialsDescriptor>(
            endPoint,
            [&](const MaterialsDescriptor& param) { _setMaterials(param); });

        endPoint = PLUGIN_API_PREFIX + "set-material-range";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        actionInterface->registerNotification<MaterialRangeDescriptor>(
            endPoint, [&](const MaterialRangeDescriptor& param) {
                _setMaterialRange(param);
            });

        endPoint = PLUGIN_API_PREFIX + "get-material-ids";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        actionInterface->registerRequest<ModelId, MaterialIds>(
            endPoint, [&](const ModelId& modelId) -> MaterialIds {
                return _getMaterialIds(modelId);
            });

        endPoint = PLUGIN_API_PREFIX + "set-material-extra-attributes";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        actionInterface->registerNotification<MaterialExtraAttributes>(
            endPoint, [&](const MaterialExtraAttributes& param) {
                _setMaterialExtraAttributes(param);
            });

        endPoint = PLUGIN_API_PREFIX + "set-synapses-attributes";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        actionInterface->registerNotification<SynapseAttributes>(
            endPoint, [&](const SynapseAttributes& param) {
                _setSynapseAttributes(param);
            });

        endPoint = PLUGIN_API_PREFIX + "save-model-to-cache";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        actionInterface->registerNotification<ExportModelToFile>(
            endPoint,
            [&](const ExportModelToFile& param) { _exportModelToFile(param); });

        endPoint = PLUGIN_API_PREFIX + "save-model-to-mesh";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        actionInterface->registerNotification<ExportModelToMesh>(
            endPoint,
            [&](const ExportModelToMesh& param) { _exportModelToMesh(param); });

        endPoint = PLUGIN_API_PREFIX + "set-connections-per-value";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        actionInterface->registerNotification<ConnectionsPerValue>(
            endPoint, [&](const ConnectionsPerValue& param) {
                _setConnectionsPerValue(param);
            });

        endPoint = PLUGIN_API_PREFIX + "set-metaballs-per-simulation-value";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        actionInterface->registerNotification<MetaballsFromSimulationValue>(
            endPoint, [&](const MetaballsFromSimulationValue& param) {
                _setMetaballsPerSimulationValue(param);
            });

        endPoint = PLUGIN_API_PREFIX + "set-odu-camera";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        _api->getActionInterface()->registerNotification<CameraDefinition>(
            endPoint, [&](const CameraDefinition& s) { _setCamera(s); });

        endPoint = PLUGIN_API_PREFIX + "get-odu-camera";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        _api->getActionInterface()->registerRequest<CameraDefinition>(
            endPoint, [&]() -> CameraDefinition { return _getCamera(); });

        endPoint = PLUGIN_API_PREFIX + "attach-cell-growth-handler";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        _api->getActionInterface()
            ->registerNotification<AttachCellGrowthHandler>(
                endPoint, [&](const AttachCellGrowthHandler& s) {
                    _attachCellGrowthHandler(s);
                });

        endPoint = PLUGIN_API_PREFIX + "attach-circuit-simulation-handler";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        _api->getActionInterface()
            ->registerNotification<AttachCircuitSimulationHandler>(
                endPoint, [&](const AttachCircuitSimulationHandler& s) {
                    _attachCircuitSimulationHandler(s);
                });

        endPoint = PLUGIN_API_PREFIX + "trace-anterograde";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        _api->getActionInterface()
            ->registerRequest<AnterogradeTracing, AnterogradeTracingResult>(
                endPoint, [&](const AnterogradeTracing& payload) {
                    return _traceAnterogrades(payload);
                });

        endPoint = PLUGIN_API_PREFIX + "add-grid";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        _api->getActionInterface()->registerNotification<AddGrid>(
            endPoint, [&](const AddGrid& payload) { _addGrid(payload); });

        endPoint = PLUGIN_API_PREFIX + "add-column";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        _api->getActionInterface()->registerNotification<AddColumn>(
            endPoint, [&](const AddColumn& payload) { _addColumn(payload); });

        endPoint = PLUGIN_API_PREFIX + "add-sphere";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        _api->getActionInterface()->registerRequest<AddSphere, AddShapeResult>(
            endPoint,
            [&](const AddSphere& payload) { return _addSphere(payload); });

        endPoint = PLUGIN_API_PREFIX + "add-pill";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        _api->getActionInterface()->registerRequest<AddPill, AddShapeResult>(
            endPoint,
            [&](const AddPill& payload) { return _addPill(payload); });

        endPoint = PLUGIN_API_PREFIX + "add-cylinder";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        _api->getActionInterface()
            ->registerRequest<AddCylinder, AddShapeResult>(
                endPoint, [&](const AddCylinder& payload) {
                    return _addCylinder(payload);
                });

        endPoint = PLUGIN_API_PREFIX + "add-box";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        _api->getActionInterface()->registerRequest<AddBox, AddShapeResult>(
            endPoint, [&](const AddBox& payload) { return _addBox(payload); });

#ifdef USE_PQXX
        endPoint = PLUGIN_API_PREFIX + "import-volume";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        actionInterface->registerRequest<ImportVolume, Response>(
            endPoint, [&](const ImportVolume& param) -> Response {
                return _importVolume(param);
            });

        endPoint = PLUGIN_API_PREFIX + "import-compartment-simulation";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        _api->getActionInterface()
            ->registerRequest<ImportCompartmentSimulation, Response>(
                endPoint,
                [&](const ImportCompartmentSimulation& payload) -> Response {
                    return _importCompartmentSimulation(payload);
                });

        endPoint = PLUGIN_API_PREFIX + "import-morphology";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        actionInterface->registerRequest<ImportMorphology, Response>(
            endPoint, [&](const ImportMorphology& param) -> Response {
                return _importMorphology(param);
            });

        endPoint = PLUGIN_API_PREFIX + "import-morphology-as-sdf";
        PLUGIN_INFO("Registering '" + endPoint + "' endpoint");
        actionInterface->registerRequest<ImportMorphology, Response>(
            endPoint, [&](const ImportMorphology& param) -> Response {
                return _importMorphologyAsSDF(param);
            });
#endif
    }
}

void CircuitExplorerPlugin::preRender()
{
    if (_dirty)
        _api->getScene().markModified();
    _dirty = false;
}

Response CircuitExplorerPlugin::_getVersion() const
{
    Response response;
    response.contents = PACKAGE_VERSION;
    return response;
}

void CircuitExplorerPlugin::_setMaterialExtraAttributes(
    const MaterialExtraAttributes& mea)
{
    auto modelDescriptor = _api->getScene().getModel(mea.modelId);
    if (modelDescriptor)
        try
        {
            auto materials = modelDescriptor->getModel().getMaterials();
            for (auto& material : materials)
            {
                PropertyMap props;
                props.setProperty({MATERIAL_PROPERTY_CAST_USER_DATA, false});
                props.setProperty(
                    {MATERIAL_PROPERTY_SHADING_MODE,
                     static_cast<int>(MaterialShadingMode::diffuse)});
                props.setProperty(
                    {MATERIAL_PROPERTY_CLIPPING_MODE,
                     static_cast<int>(MaterialClippingMode::no_clipping)});
                props.setProperty({MATERIAL_PROPERTY_USER_PARAMETER, 1.0});
                material.second->updateProperties(props);
            }
        }
        catch (const std::runtime_error& e)
        {
            PLUGIN_INFO(e.what());
        }
    else
        PLUGIN_INFO("Model " << mea.modelId << " is not registered");
}

void CircuitExplorerPlugin::_setMaterial(const MaterialDescriptor& md)
{
    auto modelDescriptor = _api->getScene().getModel(md.modelId);
    if (modelDescriptor)
        try
        {
            auto material =
                modelDescriptor->getModel().getMaterial(md.materialId);
            if (material)
            {
                material->setDiffuseColor({md.diffuseColor[0],
                                           md.diffuseColor[1],
                                           md.diffuseColor[2]});
                material->setSpecularColor({md.specularColor[0],
                                            md.specularColor[1],
                                            md.specularColor[2]});

                material->setSpecularExponent(md.specularExponent);
                material->setReflectionIndex(md.reflectionIndex);
                material->setOpacity(md.opacity);
                material->setRefractionIndex(md.refractionIndex);
                material->setEmission(md.emission);
                material->setGlossiness(md.glossiness);
                material->updateProperty(MATERIAL_PROPERTY_CAST_USER_DATA,
                                         md.simulationDataCast);
                material->updateProperty(MATERIAL_PROPERTY_SHADING_MODE,
                                         md.shadingMode);
                material->updateProperty(MATERIAL_PROPERTY_CLIPPING_MODE,
                                         md.clippingMode);
                material->updateProperty(MATERIAL_PROPERTY_USER_PARAMETER,
                                         static_cast<double>(md.userParameter));
                material->markModified(); // This is needed to apply
                                          // propery modifications
                material->commit();

                _dirty = true;
            }
            else
                PLUGIN_INFO("Material " << md.materialId
                                        << " is not registered in model "
                                        << md.modelId);
        }
        catch (const std::runtime_error& e)
        {
            PLUGIN_INFO(e.what());
        }
    else
        PLUGIN_INFO("Model " << md.modelId << " is not registered");
}

void CircuitExplorerPlugin::_setMaterials(const MaterialsDescriptor& md)
{
    for (const auto modelId : md.modelIds)
    {
        auto modelDescriptor = _api->getScene().getModel(modelId);
        if (modelDescriptor)
        {
            size_t id = 0;
            for (const auto materialId : md.materialIds)
            {
                try
                {
                    auto material =
                        modelDescriptor->getModel().getMaterial(materialId);
                    if (material)
                    {
                        PLUGIN_INFO("Setting material " << materialId);
                        if (!md.diffuseColors.empty())
                        {
                            const size_t index = id * 3;
                            material->setDiffuseColor(
                                {md.diffuseColors[index],
                                 md.diffuseColors[index + 1],
                                 md.diffuseColors[index + 2]});
                            material->setSpecularColor(
                                {md.specularColors[index],
                                 md.specularColors[index + 1],
                                 md.specularColors[index + 2]});
                        }

                        if (!md.specularExponents.empty())
                            material->setSpecularExponent(
                                md.specularExponents[id]);
                        if (!md.reflectionIndices.empty())
                            material->setReflectionIndex(
                                md.reflectionIndices[id]);
                        if (!md.opacities.empty())
                            material->setOpacity(md.opacities[id]);
                        if (!md.refractionIndices.empty())
                            material->setRefractionIndex(
                                md.refractionIndices[id]);
                        if (!md.emissions.empty())
                            material->setEmission(md.emissions[id]);
                        if (!md.glossinesses.empty())
                            material->setGlossiness(md.glossinesses[id]);
                        if (!md.simulationDataCasts.empty())
                        {
                            const bool value = md.simulationDataCasts[id];
                            material->updateProperty(
                                MATERIAL_PROPERTY_CAST_USER_DATA, value);
                        }
                        if (!md.shadingModes.empty())
                            material->updateProperty(
                                MATERIAL_PROPERTY_SHADING_MODE,
                                md.shadingModes[id]);
                        if (!md.clippingModes.empty())
                            material->updateProperty(
                                MATERIAL_PROPERTY_CLIPPING_MODE,
                                md.clippingModes[id]);
                        if (!md.userParameters.empty())
                            material->updateProperty(
                                MATERIAL_PROPERTY_USER_PARAMETER,
                                static_cast<double>(md.userParameters[id]));
                        material->markModified(); // This is needed to apply
                                                  // propery modifications
                        material->commit();
                    }
                }
                catch (const std::runtime_error& e)
                {
                    PLUGIN_INFO(e.what());
                }
                ++id;
            }
            _dirty = true;
        }
        else
            PLUGIN_INFO("Model " << modelId << " is not registered");
    }
}

void CircuitExplorerPlugin::_setMaterialRange(
    const MaterialRangeDescriptor& mrd)
{
    auto modelDescriptor = _api->getScene().getModel(mrd.modelId);
    if (modelDescriptor)
    {
        std::vector<size_t> matIds;
        if (mrd.materialIds.empty())
        {
            matIds.reserve(modelDescriptor->getModel().getMaterials().size());
            for (const auto& mat : modelDescriptor->getModel().getMaterials())
                matIds.push_back(mat.first);
        }
        else
        {
            matIds.reserve(mrd.materialIds.size());
            for (const auto& id : mrd.materialIds)
                matIds.push_back(static_cast<size_t>(id));
        }

        if (mrd.diffuseColor.size() % 3 != 0)
        {
            PLUGIN_ERROR(
                "set-material-range: The diffuse colors component "
                "is not a multiple of 3");
            return;
        }

        const size_t numColors = mrd.diffuseColor.size() / 3;

        for (const auto materialId : matIds)
        {
            try
            {
                auto material =
                    modelDescriptor->getModel().getMaterial(materialId);
                if (material)
                {
                    const size_t randomIndex = (rand() % numColors) * 3;
                    material->setDiffuseColor(
                        {mrd.diffuseColor[randomIndex],
                         mrd.diffuseColor[randomIndex + 1],
                         mrd.diffuseColor[randomIndex + 2]});
                    material->setSpecularColor({mrd.specularColor[0],
                                                mrd.specularColor[1],
                                                mrd.specularColor[2]});

                    material->setSpecularExponent(mrd.specularExponent);
                    material->setReflectionIndex(mrd.reflectionIndex);
                    material->setOpacity(mrd.opacity);
                    material->setRefractionIndex(mrd.refractionIndex);
                    material->setEmission(mrd.emission);
                    material->setGlossiness(mrd.glossiness);
                    material->updateProperty(MATERIAL_PROPERTY_CAST_USER_DATA,
                                             mrd.simulationDataCast);
                    material->updateProperty(MATERIAL_PROPERTY_SHADING_MODE,
                                             mrd.shadingMode);
                    material->updateProperty(MATERIAL_PROPERTY_CLIPPING_MODE,
                                             mrd.clippingMode);
                    material->updateProperty(MATERIAL_PROPERTY_USER_PARAMETER,
                                             static_cast<double>(
                                                 mrd.userParameter));
                    material->markModified(); // This is needed to apply
                                              // propery modifications
                    material->commit();
                }
            }
            catch (const std::runtime_error& e)
            {
                PLUGIN_INFO(e.what());
            }
        }
        _dirty = true;
    }
    else
        PLUGIN_INFO("Model " << mrd.modelId << " is not registered");
}

MaterialIds CircuitExplorerPlugin::_getMaterialIds(const ModelId& modelId)
{
    MaterialIds materialIds;
    auto modelDescriptor = _api->getScene().getModel(modelId.modelId);
    if (modelDescriptor)
    {
        for (const auto& material : modelDescriptor->getModel().getMaterials())
            if (material.first != BOUNDINGBOX_MATERIAL_ID &&
                material.first != SECONDARY_MODEL_MATERIAL_ID)
                materialIds.ids.push_back(material.first);
    }
    else
        PLUGIN_THROW("Invalid model ID");
    return materialIds;
}

void CircuitExplorerPlugin::_setSynapseAttributes(
    const SynapseAttributes& param)
{
    try
    {
        _synapseAttributes = param;
        SynapseJSONLoader loader(_api->getScene(), _synapseAttributes);
        Vector3fs colors;
        for (const auto& htmlColor : _synapseAttributes.htmlColors)
        {
            auto hexCode = htmlColor;
            if (hexCode.at(0) == '#')
            {
                hexCode = hexCode.erase(0, 1);
            }
            int r, g, b;
            std::istringstream(hexCode.substr(0, 2)) >> std::hex >> r;
            std::istringstream(hexCode.substr(2, 2)) >> std::hex >> g;
            std::istringstream(hexCode.substr(4, 2)) >> std::hex >> b;

            Vector3f color{r / 255.f, g / 255.f, b / 255.f};
            colors.push_back(color);
        }
        const auto modelDescriptor =
            loader.importSynapsesFromGIDs(_synapseAttributes, colors);

        _api->getScene().addModel(modelDescriptor);

        PLUGIN_INFO("Synapses successfully added for GID "
                    << _synapseAttributes.gid);
        _dirty = true;
    }
    catch (const std::runtime_error& e)
    {
        PLUGIN_ERROR(e.what());
    }
    catch (...)
    {
        PLUGIN_ERROR("Unexpected exception occured in _updateMaterialFromJson");
    }
}

void CircuitExplorerPlugin::_exportModelToFile(
    const ExportModelToFile& saveModel)
{
    auto modelDescriptor = _api->getScene().getModel(saveModel.modelId);
    if (modelDescriptor)
    {
        BrickLoader brickLoader(_api->getScene());
        brickLoader.exportToFile(modelDescriptor, saveModel.path);
    }
    else
        PLUGIN_ERROR("Model " << saveModel.modelId << " is not registered");
}

void CircuitExplorerPlugin::_exportModelToMesh(const ExportModelToMesh& payload)
{
    auto modelDescriptor = _api->getScene().getModel(payload.modelId);
    if (modelDescriptor)
    {
        const auto& model = modelDescriptor->getModel();
        std::list<Weighted_point> l;
        for (const auto& spheres : model.getSpheres())
        {
            uint64_t count = 0;
            for (const auto& s : spheres.second)
            {
                if (count % payload.density == 0)
                    l.push_front(
                        Weighted_point(Point_3(s.center.x, s.center.y,
                                               s.center.z),
                                       payload.radiusMultiplier * s.radius));
                ++count;
            }
        }

        PLUGIN_INFO("Constructing skin surface from " << l.size()
                                                      << " spheres");

        Polyhedron polyhedron;
        if (payload.skin)
        {
            Skin_surface_3 skinSurface(l.begin(), l.end(),
                                       payload.shrinkFactor);

            PLUGIN_INFO("Meshing skin surface...");
            CGAL::mesh_skin_surface_3(skinSurface, polyhedron);
            CGAL::Polygon_mesh_processing::triangulate_faces(polyhedron);
        }
        else
        {
            Union_of_balls_3 union_of_balls(l.begin(), l.end());
            CGAL::mesh_union_of_balls_3(union_of_balls, polyhedron);
        }

        PLUGIN_INFO("Export mesh to " << payload.path);
        std::ofstream out(payload.path);
        out << polyhedron;
    }
    else
        PLUGIN_ERROR("Model " << payload.modelId << " is not registered");
}

void CircuitExplorerPlugin::_setConnectionsPerValue(
    const ConnectionsPerValue& cpv)
{
    meshing::PointCloud pointCloud;

    auto modelDescriptor = _api->getScene().getModel(cpv.modelId);
    if (modelDescriptor)
    {
        auto simulationHandler =
            modelDescriptor->getModel().getSimulationHandler();
        if (!simulationHandler)
        {
            PLUGIN_ERROR("Scene has not user data handler");
            return;
        }

        auto& model = modelDescriptor->getModel();
        for (const auto& spheres : model.getSpheres())
        {
            for (const auto& s : spheres.second)
            {
                const float* data = static_cast<float*>(
                    simulationHandler->getFrameData(cpv.frame));

                const float value = data[s.userData];
                if (abs(value - cpv.value) < cpv.epsilon)
                    pointCloud[spheres.first].push_back(
                        {s.center.x, s.center.y, s.center.z, s.radius});
            }
        }

        if (!pointCloud.empty())
        {
            auto meshModel = _api->getScene().createModel();
            meshing::PointCloudMesher mesher;
            if (mesher.toConvexHull(*meshModel, pointCloud))
            {
                auto modelDesc = std::make_shared<ModelDescriptor>(
                    std::move(meshModel),
                    "Connection for value " + std::to_string(cpv.value));

                _api->getScene().addModel(modelDesc);
                _dirty = true;
            }
        }
        else
            PLUGIN_INFO("No connections added for value "
                        << std::to_string(cpv.value));
    }
    else
        PLUGIN_INFO("Model " << cpv.modelId << " is not registered");
}

void CircuitExplorerPlugin::_setMetaballsPerSimulationValue(
    const MetaballsFromSimulationValue& mpsv)
{
    meshing::PointCloud pointCloud;

    auto modelDescriptor = _api->getScene().getModel(mpsv.modelId);
    if (modelDescriptor)
    {
        auto simulationHandler =
            modelDescriptor->getModel().getSimulationHandler();
        if (!simulationHandler)
        {
            PLUGIN_ERROR("Scene has not user data handler");
            return;
        }

        auto& model = modelDescriptor->getModel();
        for (const auto& spheres : model.getSpheres())
        {
            for (const auto& s : spheres.second)
            {
                const float* data = static_cast<float*>(
                    simulationHandler->getFrameData(mpsv.frame));

                const float value = data[s.userData];
                if (abs(value - mpsv.value) < mpsv.epsilon)
                    pointCloud[spheres.first].push_back(
                        {s.center.x, s.center.y, s.center.z, s.radius});
            }
        }

        if (!pointCloud.empty())
        {
            auto meshModel = _api->getScene().createModel();
            meshing::PointCloudMesher mesher;
            if (mesher.toMetaballs(*meshModel, pointCloud, mpsv.gridSize,
                                   mpsv.threshold))
            {
                auto modelDesc = std::make_shared<ModelDescriptor>(
                    std::move(meshModel),
                    "Connection for value " + std::to_string(mpsv.value));

                _api->getScene().addModel(modelDesc);
                PLUGIN_INFO("Metaballs successfully added to the scene");

                _dirty = true;
            }
            else
                PLUGIN_INFO("No mesh was created for value "
                            << std::to_string(mpsv.value));
        }
        else
            PLUGIN_INFO("No connections added for value "
                        << std::to_string(mpsv.value));
    }
    else
        PLUGIN_INFO("Model " << mpsv.modelId << " is not registered");
}

void CircuitExplorerPlugin::_setCamera(const CameraDefinition& payload)
{
    auto& camera = _api->getCamera();

    // Origin
    const auto& o = payload.origin;
    Vector3f origin{o[0], o[1], o[2]};
    camera.setPosition(origin);

    // Target
    const auto& d = payload.direction;
    Vector3f direction{d[0], d[1], d[2]};
    camera.setTarget(origin + direction);

    // Up
    const auto& u = payload.up;
    Vector3f up{u[0], u[1], u[2]};

    // Orientation
    const glm::quat q = glm::inverse(
        glm::lookAt(origin, origin + direction,
                    up)); // Not quite sure why this should be inverted?!?
    camera.setOrientation(q);

    // Aperture
    camera.updateProperty("apertureRadius", payload.apertureRadius);

    // Focus distance
    camera.updateProperty("focusDistance", payload.focusDistance);

    _api->getCamera().markModified();

    PLUGIN_DEBUG("SET: " << origin << ", " << direction << ", " << up << ", "
                         << glm::inverse(q) << "," << payload.apertureRadius
                         << "," << payload.focusDistance);
}

CameraDefinition CircuitExplorerPlugin::_getCamera()
{
    const auto& camera = _api->getCamera();

    CameraDefinition cd;
    const auto& p = camera.getPosition();
    cd.origin = {p.x, p.y, p.z};
    const auto d = glm::rotate(camera.getOrientation(), Vector3d(0., 0., -1.));
    cd.direction = {d.x, d.y, d.z};
    const auto u = glm::rotate(camera.getOrientation(), Vector3d(0., 1., 0.));
    cd.up = {u.x, u.y, u.z};
    PLUGIN_DEBUG("GET: " << p << ", " << d << ", " << u << ", "
                         << camera.getOrientation());
    return cd;
}

void CircuitExplorerPlugin::_attachCellGrowthHandler(
    const AttachCellGrowthHandler& payload)
{
    PLUGIN_INFO("Attaching Cell Growth Handler to model " << payload.modelId);
    auto modelDescriptor = _api->getScene().getModel(payload.modelId);
    if (modelDescriptor)
    {
        auto handler = std::make_shared<CellGrowthHandler>(payload.nbFrames);
        modelDescriptor->getModel().setSimulationHandler(handler);
    }
}

void CircuitExplorerPlugin::_attachCircuitSimulationHandler(
    const AttachCircuitSimulationHandler& payload)
{
    PLUGIN_INFO("Attaching Circuit Simulation Handler to model "
                << payload.modelId);
    auto modelDescriptor = _api->getScene().getModel(payload.modelId);
    if (modelDescriptor)
    {
        const brion::BlueConfig blueConfiguration(payload.circuitConfiguration);
        const brain::Circuit circuit(blueConfiguration);
        auto gids = circuit.getGIDs();
        auto handler = std::make_shared<VoltageSimulationHandler>(
            blueConfiguration.getReportSource(payload.reportName).getPath(),
            gids, payload.synchronousMode);
        auto& model = modelDescriptor->getModel();
        model.setSimulationHandler(handler);
        AdvancedCircuitLoader::setSimulationTransferFunction(
            model.getTransferFunction());
    }
    else
    {
        PLUGIN_ERROR("Model " << payload.modelId << " does not exist");
    }
}

AnterogradeTracingResult CircuitExplorerPlugin::_traceAnterogrades(
    const AnterogradeTracing& payload)
{
    AnterogradeTracingResult result;
    result.error = 0;

    if (payload.cellGIDs.empty())
    {
        result.error = 1;
        result.message = "No input cell GIDs specified";
        return result;
    }
    if (payload.sourceCellColor.size() < 4)
    {
        result.error = 2;
        result.message =
            "Source cell stain color must have "
            "4 components (RGBA)";
        return result;
    }
    if (payload.connectedCellsColor.size() < 4)
    {
        result.error = 3;
        result.message =
            "Connected cell stain color must have "
            "4 components (RGBA)";
        return result;
    }
    if (payload.nonConnectedCellsColor.size() < 4)
    {
        result.error = 4;
        result.message =
            "Non connected cell stain color must have "
            "4 components (RGBA)";
        return result;
    }

    auto modelDescriptor =
        _api->getScene().getModel(static_cast<size_t>(payload.modelId));
    if (modelDescriptor)
    {
        const brion::BlueConfig blueConfiguration(modelDescriptor->getPath());
        const brain::Circuit circuit(blueConfiguration);

        // Parse loaded targets
        const ModelMetadata& metaData = modelDescriptor->getMetadata();
        auto targetsIt = metaData.find("Targets");
        std::vector<std::string> targets;
        if (targetsIt != metaData.end())
        {
            const std::string& targetsString = targetsIt->second;
            if (!targetsString.empty())
            {
                if (targetsString.find(',') == std::string::npos)
                    targets.push_back(targetsString);
                else
                {
                    targets = _splitString(targetsString, ',');
                }
            }
        }

        auto densityIt = metaData.find("Density");
        const double density = std::stod(densityIt->second);
        auto randomIt = metaData.find("RandomSeed");
        const double randomSeed = std::stod(randomIt->second);

        auto gidsMetaIt = metaData.find("GIDs");
        const std::string& gidsStr = gidsMetaIt->second;

        brion::GIDSet allGIDs;

        if (!gidsStr.empty())
        {
            std::vector<std::string> tempStrGids = _splitString(gidsStr, ',');
            for (const auto& gidStrTok : tempStrGids)
            {
                allGIDs.insert(static_cast<uint32_t>(std::stoul(gidStrTok)));
            }
        }
        else
        {
            // Gather all GIDs from loaded targets
            if (!targets.empty())
            {
                for (const auto& targetName : targets)
                {
                    if (density < 1.0)
                    {
                        brion::GIDSet targetGids =
                            circuit.getRandomGIDs(density, targetName,
                                                  randomSeed);
                        allGIDs.insert(targetGids.begin(), targetGids.end());
                    }
                    else
                    {
                        brion::GIDSet targetGids = circuit.getGIDs(targetName);
                        allGIDs.insert(targetGids.begin(), targetGids.end());
                    }
                }
            }
            else if (density < 1.0)
                allGIDs = circuit.getRandomGIDs(density, "", randomSeed);
            else
                allGIDs = circuit.getGIDs();
        }

        // Map GIDs to material IDs
        std::unordered_map<uint32_t, size_t> gidMaterialMap;
        auto materials = modelDescriptor->getModel().getMaterials();
        auto itGids = allGIDs.begin();
        auto itMats = materials.begin();
        for (; itMats != materials.end(); ++itMats, ++itGids)
        {
            gidMaterialMap[*itGids] = itMats->first;
        }

        const size_t totalSynapses = payload.targetCellGIDs.size();
        result.message = "Tracing returned " + std::to_string(totalSynapses) +
                         " connected cells";

        // If there is any GID, modify the scene materials
        if (totalSynapses > 0)
        {
            // Enable CircuitExplorer extra parameters on materials
            MaterialExtraAttributes mea;
            mea.modelId = payload.modelId;
            _setMaterialExtraAttributes(mea);

            // Reset all cells to default color
            MaterialRangeDescriptor mrd;
            mrd.modelId = payload.modelId;
            mrd.opacity = static_cast<float>(payload.nonConnectedCellsColor[3]);
            mrd.diffuseColor = {
                static_cast<float>(payload.nonConnectedCellsColor[0]),
                static_cast<float>(payload.nonConnectedCellsColor[1]),
                static_cast<float>(payload.nonConnectedCellsColor[2])};

            mrd.specularColor = {0.f, 0.f, 0.f};
            mrd.specularExponent = 0.f;
            mrd.glossiness = 0.f;
            mrd.shadingMode =
                static_cast<int>(MaterialShadingMode::diffuse_transparency);
            _setMaterialRange(mrd);

            // Mark connections
            MaterialRangeDescriptor connectedMrd = mrd;
            mrd.diffuseColor = {
                static_cast<float>(payload.connectedCellsColor[0]),
                static_cast<float>(payload.connectedCellsColor[1]),
                static_cast<float>(payload.connectedCellsColor[2])};
            mrd.opacity = 1.0f;
            mrd.modelId = payload.modelId;
            mrd.materialIds.reserve(payload.targetCellGIDs.size());
            for (auto gid : payload.targetCellGIDs)
            {
                auto it = gidMaterialMap.find(gid);
                if (it != gidMaterialMap.end())
                    mrd.materialIds.push_back(it->second);
            }
            _setMaterialRange(connectedMrd);

            // Mark sources
            const std::set<uint32_t> gidsSet(payload.cellGIDs.begin(),
                                             payload.cellGIDs.end());
            MaterialRangeDescriptor sourcesMrd = mrd;
            mrd.diffuseColor = {static_cast<float>(payload.sourceCellColor[0]),
                                static_cast<float>(payload.sourceCellColor[1]),
                                static_cast<float>(payload.sourceCellColor[2])};
            mrd.opacity = 1.f;
            mrd.modelId = payload.modelId;
            mrd.materialIds.reserve(gidsSet.size());
            for (auto source : gidsSet)
            {
                auto it = gidMaterialMap.find(source);
                if (it != gidMaterialMap.end())
                    mrd.materialIds.push_back(it->second);
            }
            _setMaterialRange(sourcesMrd);

            _api->getScene().markModified();
            _api->getEngine().triggerRender();
            //_dirty = true;
        }
    }
    else
    {
        result.error = 5;
        result.message =
            "The given model ID does not correspond "
            "to any existing scene model";
    }

    return result;
}

void CircuitExplorerPlugin::_createShapeMaterial(ModelPtr& model,
                                                 const size_t id,
                                                 const Vector3d& color,
                                                 const double& opacity)
{
    MaterialPtr mptr = model->createMaterial(id, std::to_string(id));
    mptr->setDiffuseColor(color);
    mptr->setOpacity(opacity);
    mptr->setSpecularExponent(0.0);

    PropertyMap props;
    props.setProperty({MATERIAL_PROPERTY_CAST_USER_DATA, false});
    props.setProperty(
        {MATERIAL_PROPERTY_SHADING_MODE,
         static_cast<int>(MaterialShadingMode::diffuse_transparency)});
    props.setProperty({MATERIAL_PROPERTY_CLIPPING_MODE,
                       static_cast<int>(MaterialClippingMode::no_clipping)});

    mptr->updateProperties(props);

    mptr->markModified();
    mptr->commit();
}

AddShapeResult CircuitExplorerPlugin::_addSphere(const AddSphere& payload)
{
    AddShapeResult result;
    result.error = 0;
    result.message = "";

    if (payload.center.size() < 3)
    {
        result.error = 1;
        result.message =
            "Sphere center has the wrong number of parameters (3 "
            "necessary)";
        return result;
    }

    if (payload.color.size() < 4)
    {
        result.error = 2;
        result.message =
            "Sphere color has the wrong number of parameters (RGBA, 4 "
            "necessary)";
        return result;
    }

    if (payload.radius < 0.0f)
    {
        result.error = 3;
        result.message = "Negative radius passed for sphere creation";
        return result;
    }

    ModelPtr modelptr = _api->getScene().createModel();

    const size_t matId = 1;
    const Vector3d color(payload.color[0], payload.color[1], payload.color[2]);
    const double opacity = payload.color[3];
    _createShapeMaterial(modelptr, matId, color, opacity);

    const Vector3f center(payload.center[0], payload.center[1],
                          payload.center[2]);
    modelptr->addSphere(matId, {center, payload.radius});

    size_t numModels = _api->getScene().getNumModels();
    const std::string name = payload.name.empty()
                                 ? "sphere_" + std::to_string(numModels)
                                 : payload.name;
    result.id = _api->getScene().addModel(
        std::make_shared<ModelDescriptor>(std::move(modelptr), name));
    _api->getScene().markModified();

    _api->getEngine().triggerRender();

    _dirty = true;
    return result;
}

AddShapeResult CircuitExplorerPlugin::_addPill(const AddPill& payload)
{
    AddShapeResult result;
    result.error = 0;
    result.message = "";

    if (payload.p1.size() < 3)
    {
        result.error = 1;
        result.message =
            "Pill point 1 has the wrong number of parameters (3 necessary)";
        return result;
    }
    if (payload.p2.size() < 3)
    {
        result.error = 2;
        result.message =
            "Pill point 2 has the wrong number of parameters (3 necessary)";
        return result;
    }
    if (payload.color.size() < 4)
    {
        result.error = 3;
        result.message =
            "Pill color has the wrong number of parameters (RGBA, 4 "
            "necessary)";
        return result;
    }
    if (payload.type != "pill" && payload.type != "conepill" &&
        payload.type != "sigmoidpill")
    {
        result.error = 4;
        result.message =
            "Unknown pill type parameter. Must be either \"pill\", "
            "\"conepill\", or \"sigmoidpill\"";
        return result;
    }
    if (payload.radius1 < 0.0f || payload.radius2 < 0.0f)
    {
        result.error = 5;
        result.message = "Negative radius passed for the pill creation";
        return result;
    }

    ModelPtr modelptr = _api->getScene().createModel();

    size_t matId = 1;
    const Vector3d color(payload.color[0], payload.color[1], payload.color[2]);
    const double opacity = payload.color[3];
    _createShapeMaterial(modelptr, matId, color, opacity);

    const Vector3f p0(payload.p1[0], payload.p1[1], payload.p1[2]);
    const Vector3f p1(payload.p2[0], payload.p2[1], payload.p2[2]);
    SDFGeometry sdf;
    if (payload.type == "pill")
    {
        sdf = createSDFPill(p0, p1, payload.radius1);
    }
    else if (payload.type == "conepill")
    {
        sdf = createSDFConePill(p0, p1, payload.radius1, payload.radius2);
    }
    else if (payload.type == "sigmoidpill")
    {
        sdf =
            createSDFConePillSigmoid(p0, p1, payload.radius1, payload.radius2);
    }

    modelptr->addSDFGeometry(matId, sdf, {});

    size_t numModels = _api->getScene().getNumModels();
    const std::string name =
        payload.name.empty() ? payload.type + "_" + std::to_string(numModels)
                             : payload.name;
    result.id = _api->getScene().addModel(
        std::make_shared<ModelDescriptor>(std::move(modelptr), name));
    _api->getScene().markModified();
    _api->getEngine().triggerRender();

    _dirty = true;

    return result;
}

AddShapeResult CircuitExplorerPlugin::_addCylinder(const AddCylinder& payload)
{
    AddShapeResult result;
    result.error = 0;
    result.message = "";

    if (payload.center.size() < 3)
    {
        result.error = 1;
        result.message =
            "Cylinder center has the wrong number of parameters (3 "
            "necessary)";
        return result;
        ;
    }
    if (payload.up.size() < 3)
    {
        result.error = 2;
        result.message =
            "Cylinder up has the wrong number of parameters (3 necessary)";
        return result;
    }
    if (payload.color.size() < 4)
    {
        result.error = 3;
        result.message =
            "Cylinder color has the wrong number of parameters (RGBA, 4 "
            "necessary)";
        return result;
    }
    if (payload.radius < 0.0f)
    {
        result.error = 4;
        result.message = "Negative radius passed for cylinder creation";
        return result;
    }

    ModelPtr modelptr = _api->getScene().createModel();

    const size_t matId = 1;
    const Vector3d color(payload.color[0], payload.color[1], payload.color[2]);
    const double opacity = payload.color[3];
    _createShapeMaterial(modelptr, matId, color, opacity);

    const Vector3f center(payload.center[0], payload.center[1],
                          payload.center[2]);
    const Vector3f up(payload.up[0], payload.up[1], payload.up[2]);
    modelptr->addCylinder(matId, {center, up, payload.radius});

    size_t numModels = _api->getScene().getNumModels();
    const std::string name = payload.name.empty()
                                 ? "cylinder_" + std::to_string(numModels)
                                 : payload.name;
    result.id = _api->getScene().addModel(
        std::make_shared<ModelDescriptor>(std::move(modelptr), name));
    _api->getScene().markModified();
    _api->getEngine().triggerRender();

    _dirty = true;

    return result;
}

AddShapeResult CircuitExplorerPlugin::_addBox(const AddBox& payload)
{
    AddShapeResult result;
    result.error = 0;
    result.message = "";

    if (payload.minCorner.size() < 3)
    {
        result.error = 1;
        result.message =
            "Box minCorner has the wrong number of parameters (3 "
            "necessary)";
        return result;
    }
    if (payload.maxCorner.size() < 3)
    {
        result.error = 2;
        result.message =
            "Box maxCorner has the wrong number of parameters (3 necesary)";
        return result;
    }
    if (payload.color.size() < 4)
    {
        result.error = 3;
        result.message =
            "Box color has the wrong number of parameters (RGBA, 4 "
            "necesary)";
        return result;
    }

    ModelPtr modelptr = _api->getScene().createModel();

    const size_t matId = 1;
    const Vector3d color(payload.color[0], payload.color[1], payload.color[2]);
    const double opacity = payload.color[3];
    _createShapeMaterial(modelptr, matId, color, opacity);

    const Vector3f minCorner(payload.minCorner[0], payload.minCorner[1],
                             payload.minCorner[2]);
    const Vector3f maxCorner(payload.maxCorner[0], payload.maxCorner[1],
                             payload.maxCorner[2]);

    TriangleMesh mesh = createBox(minCorner, maxCorner);

    modelptr->getTriangleMeshes()[matId] = mesh;
    modelptr->markInstancesDirty();

    size_t numModels = _api->getScene().getNumModels();
    const std::string name = payload.name.empty()
                                 ? "box_" + std::to_string(numModels)
                                 : payload.name;
    result.id = _api->getScene().addModel(
        std::make_shared<ModelDescriptor>(std::move(modelptr), name));
    _api->getScene().markModified();
    _api->getEngine().triggerRender();

    _dirty = true;

    return result;
}

void CircuitExplorerPlugin::_addGrid(const AddGrid& payload)
{
    PLUGIN_INFO("Building Grid scene");

    auto& scene = _api->getScene();
    auto model = scene.createModel();

    const Vector3f red = {1, 0, 0};
    const Vector3f green = {0, 1, 0};
    const Vector3f blue = {0, 0, 1};
    const Vector3f grey = {0.5, 0.5, 0.5};

    PropertyMap props;
    props.setProperty({MATERIAL_PROPERTY_CAST_USER_DATA, false});
    props.setProperty({MATERIAL_PROPERTY_SHADING_MODE,
                       static_cast<int>(MaterialShadingMode::none)});
    props.setProperty({MATERIAL_PROPERTY_CLIPPING_MODE,
                       static_cast<int>(MaterialClippingMode::no_clipping)});

    auto material = model->createMaterial(0, "x");
    material->setDiffuseColor(grey);
    material->setProperties(props);

    const float m = payload.minValue;
    const float M = payload.maxValue;
    const float s = payload.steps;
    const float r = payload.radius;
    for (float x = m; x <= M; x += s)
        for (float y = m; y <= M; y += s)
            if (fabs(x) < 0.001f || fabs(y) < 0.001f)
            {
                model->addCylinder(0, {{x, y, m}, {x, y, M}, r});
                model->addCylinder(0, {{m, x, y}, {M, x, y}, r});
                model->addCylinder(0, {{x, m, y}, {x, M, y}, r});
            }

    material = model->createMaterial(1, "plane_x");
    material->setDiffuseColor(payload.useColors ? red : grey);
    material->setOpacity(payload.planeOpacity);
    material->setProperties(props);
    auto& tmx = model->getTriangleMeshes()[1];
    tmx.vertices.push_back({m, 0, m});
    tmx.vertices.push_back({M, 0, m});
    tmx.vertices.push_back({M, 0, M});
    tmx.vertices.push_back({m, 0, M});
    tmx.indices.push_back(Vector3ui(0, 1, 2));
    tmx.indices.push_back(Vector3ui(2, 3, 0));

    material = model->createMaterial(2, "plane_y");
    material->setDiffuseColor(payload.useColors ? green : grey);
    material->setOpacity(payload.planeOpacity);
    material->setProperties(props);
    auto& tmy = model->getTriangleMeshes()[2];
    tmy.vertices.push_back({m, m, 0});
    tmy.vertices.push_back({M, m, 0});
    tmy.vertices.push_back({M, M, 0});
    tmy.vertices.push_back({m, M, 0});
    tmy.indices.push_back(Vector3ui(0, 1, 2));
    tmy.indices.push_back(Vector3ui(2, 3, 0));

    material = model->createMaterial(3, "plane_z");
    material->setDiffuseColor(payload.useColors ? blue : grey);
    material->setOpacity(payload.planeOpacity);
    material->setProperties(props);
    auto& tmz = model->getTriangleMeshes()[3];
    tmz.vertices.push_back({0, m, m});
    tmz.vertices.push_back({0, m, M});
    tmz.vertices.push_back({0, M, M});
    tmz.vertices.push_back({0, M, m});
    tmz.indices.push_back(Vector3ui(0, 1, 2));
    tmz.indices.push_back(Vector3ui(2, 3, 0));

    if (payload.showAxis)
    {
        const float l = M;
        const float smallRadius = payload.radius * 25.0;
        const float largeRadius = payload.radius * 50.0;
        const float l1 = l * 0.89;
        const float l2 = l * 0.90;

        PropertyMap diffuseProps;
        diffuseProps.setProperty({MATERIAL_PROPERTY_CAST_USER_DATA, false});
        diffuseProps.setProperty(
            {MATERIAL_PROPERTY_SHADING_MODE,
             static_cast<int>(MaterialShadingMode::diffuse)});

        // X
        material = model->createMaterial(4, "x_axis");
        material->setDiffuseColor({1, 0, 0});
        material->setProperties(diffuseProps);

        model->addCylinder(4, {{0, 0, 0}, {l1, 0, 0}, smallRadius});
        model->addCone(4, {{l1, 0, 0}, {l2, 0, 0}, smallRadius, largeRadius});
        model->addCone(4, {{l2, 0, 0}, {M, 0, 0}, largeRadius, 0});

        // Y
        material = model->createMaterial(5, "y_axis");
        material->setDiffuseColor({0, 1, 0});
        material->setProperties(diffuseProps);

        model->addCylinder(5, {{0, 0, 0}, {0, l1, 0}, smallRadius});
        model->addCone(5, {{0, l1, 0}, {0, l2, 0}, smallRadius, largeRadius});
        model->addCone(5, {{0, l2, 0}, {0, M, 0}, largeRadius, 0});

        // Z
        material = model->createMaterial(6, "z_axis");
        material->setDiffuseColor({0, 0, 1});
        material->setProperties(diffuseProps);

        model->addCylinder(6, {{0, 0, 0}, {0, 0, l1}, smallRadius});
        model->addCone(6, {{0, 0, l1}, {0, 0, l2}, smallRadius, largeRadius});
        model->addCone(6, {{0, 0, l2}, {0, 0, M}, largeRadius, 0});

        // Origin
        model->addSphere(0, {{0, 0, 0}, smallRadius});
    }

    scene.addModel(std::make_shared<ModelDescriptor>(std::move(model), "Grid"));
}

void CircuitExplorerPlugin::_addColumn(const AddColumn& payload)
{
    PLUGIN_INFO("Building Column model");

    auto& scene = _api->getScene();
    auto model = scene.createModel();

    const Vector3f white = {1.f, 1.f, 1.F};

    PropertyMap props;
    props.setProperty({MATERIAL_PROPERTY_CAST_USER_DATA, false});
    props.setProperty({MATERIAL_PROPERTY_SHADING_MODE,
                       static_cast<int>(MaterialShadingMode::diffuse)});
    props.setProperty({MATERIAL_PROPERTY_CLIPPING_MODE,
                       static_cast<int>(MaterialClippingMode::no_clipping)});

    auto material = model->createMaterial(0, "column");
    material->setDiffuseColor(white);
    material->setProperties(props);

    const Vector3fs verticesBottom = {
        {-0.25f, -1.0f, -0.5f}, {0.25f, -1.0f, -0.5f}, {0.5f, -1.0f, -0.25f},
        {0.5f, -1.0f, 0.25f},   {0.5f, -1.0f, -0.25f}, {0.5f, -1.0f, 0.25f},
        {0.25f, -1.0f, 0.5f},   {-0.25f, -1.0f, 0.5f}, {-0.5f, -1.0f, 0.25f},
        {-0.5f, -1.0f, -0.25f}};
    const Vector3fs verticesTop = {{-0.25f, 1.f, -0.5f}, {0.25f, 1.f, -0.5f},
                                   {0.5f, 1.f, -0.25f},  {0.5f, 1.f, 0.25f},
                                   {0.5f, 1.f, -0.25f},  {0.5f, 1.f, 0.25f},
                                   {0.25f, 1.f, 0.5f},   {-0.25f, 1.f, 0.5f},
                                   {-0.5f, 1.f, 0.25f},  {-0.5f, 1.f, -0.25f}};

    const auto r = payload.radius;
    for (size_t i = 0; i < verticesBottom.size(); ++i)
    {
        model->addCylinder(0, {verticesBottom[i],
                               verticesBottom[(i + 1) % verticesBottom.size()],
                               r / 2.f});
        model->addSphere(0, {verticesBottom[i], r});
    }

    for (size_t i = 0; i < verticesTop.size(); ++i)
    {
        model->addCylinder(0, {verticesTop[i],
                               verticesTop[(i + 1) % verticesTop.size()],
                               r / 2.f});
        model->addSphere(0, {verticesTop[i], r});
    }

    for (size_t i = 0; i < verticesTop.size(); ++i)
        model->addCylinder(0, {verticesBottom[i], verticesTop[i], r / 2.f});

    scene.addModel(
        std::make_shared<ModelDescriptor>(std::move(model), "Column"));
}

#ifdef USE_PQXX
Response CircuitExplorerPlugin::_importMorphology(
    const ImportMorphology& payload)
{
    Response response;
    try
    {
        DBConnector connector(payload.connectionString, payload.schema);
        connector.importMorphology(_api->getScene(), payload.guid,
                                   payload.filename);
        response.status = true;
    }
    catch (const std::runtime_error& e)
    {
        response.contents = e.what();
        PLUGIN_ERROR(response.contents);
    }
    return response;
}

Response CircuitExplorerPlugin::_importMorphologyAsSDF(
    const ImportMorphology& payload)
{
    Response response;
    try
    {
        DBConnector connector(payload.connectionString, payload.schema);
        connector.importMorphologyAsSDF(_api->getScene(), payload.guid,
                                        payload.filename);
        response.status = true;
    }
    catch (const std::runtime_error& e)
    {
        response.contents = e.what();
        PLUGIN_ERROR(response.contents);
    }
    return response;
}

Response CircuitExplorerPlugin::_importVolume(const ImportVolume& payload)
{
    Response response;
    try
    {
        DBConnector connector(payload.connectionString, payload.schema);
        const auto& d{payload.dimensions};
        const auto& s{payload.spacing};
        connector.importVolume(payload.guid, {d[0], d[1], d[2]},
                               {s[0], s[1], s[2]}, payload.rawFilename);
    }
    catch (const std::runtime_error& e)
    {
        response.contents = e.what();
        PLUGIN_ERROR(response.contents);
    }
    return response;
}

Response CircuitExplorerPlugin::_importCompartmentSimulation(
    const ImportCompartmentSimulation& payload)
{
    Response response;
    DBConnector connector(payload.connectionString, payload.schema);
    try
    {
        connector.importCompartmentSimulation(payload.blueConfig,
                                              payload.reportName);
        response.status = true;
    }
    catch (const std::runtime_error& e)
    {
        response.contents = e.what();
    }
    return response;
}
#endif

extern "C" ExtensionPlugin* brayns_plugin_create(int /*argc*/, char** /*argv*/)
{
    PLUGIN_INFO("");
    PLUGIN_INFO(
        "   _|_|_|  _|                                _|    _|      _|_|_|_|   "
        "                   _|                                          ");
    PLUGIN_INFO(
        " _|            _|  _|_|    _|_|_|  _|    _|      _|_|_|_|  _|        "
        "_|    _|  _|_|_|    _|    _|_|    _|  _|_|    _|_|    _|  _|_| ");
    PLUGIN_INFO(
        " _|        _|  _|_|      _|        _|    _|  _|    _|      _|_|_|     "
        " _|_|    _|    _|  _|  _|    _|  _|_|      _|_|_|_|  _|_|     ");
    PLUGIN_INFO(
        " _|        _|  _|        _|        _|    _|  _|    _|      _|        "
        "_|    _|  _|    _|  _|  _|    _|  _|        _|        _|       ");
    PLUGIN_INFO(
        "   _|_|_|  _|  _|          _|_|_|    _|_|_|  _|      _|_|  _|_|_|_|  "
        "_|    _|  _|_|_|    _|    _|_|    _|          _|_|_|  _|        ");
    PLUGIN_INFO(
        "                                                                      "
        "         _|                                                   ");
    PLUGIN_INFO(
        "                                                                      "
        "         _|                                                   ");
    PLUGIN_INFO("");
    PLUGIN_INFO("Initializing CircuitExplorer plug-in");
    return new CircuitExplorerPlugin();
}
} // namespace circuitexplorer
