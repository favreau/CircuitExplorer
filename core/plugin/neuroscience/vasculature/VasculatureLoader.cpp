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

#include "VasculatureLoader.h"

#include <common/CommonTypes.h>
#include <common/Logs.h>
#include <common/Utils.h>
#include <plugin/neuroscience/common/Types.h>

#include <brayns/engineapi/Material.h>
#include <brayns/engineapi/Model.h>
#include <brayns/engineapi/Scene.h>
#include <brayns/parameters/ParametersManager.h>

#include <highfive/H5DataSet.hpp>
#include <highfive/H5File.hpp>

namespace circuitexplorer
{
namespace neuroscience
{
namespace vasculature
{
const std::string LOADER_NAME = "Vasculature";
const std::string SUPPORTED_EXTENTION_H5 = "h5";
const size_t DEFAULT_MATERIAL = 0;

VasculatureLoader::VasculatureLoader(Scene& scene, PropertyMap&& loaderParams)
    : Loader(scene)
    , _defaults(loaderParams)
{
    PLUGIN_INFO("Registering " << LOADER_NAME);
}

std::string VasculatureLoader::getName() const
{
    return LOADER_NAME;
}

std::vector<std::string> VasculatureLoader::getSupportedExtensions() const
{
    return {SUPPORTED_EXTENTION_H5};
}

PropertyMap VasculatureLoader::getProperties() const
{
    return _defaults;
}

PropertyMap VasculatureLoader::getCLIProperties()
{
    PropertyMap pm(LOADER_NAME);
    pm.setProperty(PROP_USE_SDF_BRANCHES);
    pm.setProperty(PROP_RADIUS_MULTIPLIER);
    pm.setProperty(PROP_ASSET_QUALITY);
    pm.setProperty(PROP_ASSET_COLOR_SCHEME);
    pm.setProperty(PROP_GIDS);
    return pm;
}

bool VasculatureLoader::isSupported(const std::string& /*filename*/,
                                    const std::string& extension) const
{
    const std::set<std::string> types = {SUPPORTED_EXTENTION_H5};
    return types.find(extension) != types.end();
}

ModelDescriptorPtr VasculatureLoader::importFromBlob(
    Blob&& /*blob*/, const LoaderProgress& /*callback*/,
    const PropertyMap& /*properties*/) const
{
    throw std::runtime_error("Loading vasculature from blob is not supported");
}

// From http://en.cppreference.com/w/cpp/types/numeric_limits/epsilon
template <class T>
typename std::enable_if<!std::numeric_limits<T>::is_integer, bool>::type
    almost_equal(T x, T y, int ulp)
{
    // the machine epsilon has to be scaled to the magnitude of the values used
    // and multiplied by the desired precision in ULPs (units in the last place)
    return std::abs(x - y) <=
               std::numeric_limits<T>::epsilon() * std::abs(x + y) * ulp
           // unless the result is subnormal
           || std::abs(x - y) < std::numeric_limits<T>::min();
}

// TODO: Generalise SDF for any type of asset
size_t VasculatureLoader::_addSDFGeometry(SDFMorphologyData& sdfMorphologyData,
                                          const SDFGeometry& geometry,
                                          const std::set<size_t>& neighbours,
                                          const size_t materialId,
                                          const int section) const
{
    const size_t idx = sdfMorphologyData.geometries.size();
    sdfMorphologyData.geometries.push_back(geometry);
    sdfMorphologyData.neighbours.push_back(neighbours);
    sdfMorphologyData.materials.push_back(materialId);
    sdfMorphologyData.geometrySection[idx] = section;
    sdfMorphologyData.sectionGeometries[section].push_back(idx);
    return idx;
}

void VasculatureLoader::_addStepConeGeometry(
    const bool useSDF, const Vector3f& position, const double radius,
    const Vector3f& target, const double previousRadius,
    const size_t materialId, const uint64_t& userDataOffset, Model& model,
    SDFMorphologyData& sdfMorphologyData, const uint32_t sdfGroupId,
    const Vector3f& displacementParams) const
{
    if (useSDF)
    {
        const auto geom =
            (almost_equal(radius, previousRadius, 100000))
                ? createSDFPill(position, target, radius, userDataOffset,
                                displacementParams)
                : createSDFConePill(position, target, radius, previousRadius,
                                    userDataOffset, displacementParams);
        _addSDFGeometry(sdfMorphologyData, geom, {}, materialId, sdfGroupId);
    }
    else if (almost_equal(radius, previousRadius, 100000))
        model.addCylinder(materialId,
                          {position, target, static_cast<float>(radius),
                           userDataOffset});
    else
        model.addCone(materialId,
                      {position, target, static_cast<float>(radius),
                       static_cast<float>(previousRadius), userDataOffset});
}

void VasculatureLoader::_finalizeSDFGeometries(
    Model& model, SDFMorphologyData& sdfMorphologyData) const
{
    const size_t numGeoms = sdfMorphologyData.geometries.size();
    sdfMorphologyData.localToGlobalIdx.resize(numGeoms, 0);

    // Extend neighbours to make sure smoothing is applied on all
    // closely connected geometries
    for (size_t rep = 0; rep < 4; rep++)
    {
        const size_t numNeighs = sdfMorphologyData.neighbours.size();
        auto neighsCopy = sdfMorphologyData.neighbours;
        for (size_t i = 0; i < numNeighs; i++)
        {
            for (size_t j : sdfMorphologyData.neighbours[i])
            {
                for (size_t newNei : sdfMorphologyData.neighbours[j])
                {
                    neighsCopy[i].insert(newNei);
                    neighsCopy[newNei].insert(i);
                }
            }
        }
        sdfMorphologyData.neighbours = neighsCopy;
    }

    for (size_t i = 0; i < numGeoms; i++)
    {
        // Convert neighbours from set to vector and erase itself from its
        // neighbours
        std::vector<size_t> neighbours;
        const auto& neighSet = sdfMorphologyData.neighbours[i];
        std::copy(neighSet.begin(), neighSet.end(),
                  std::back_inserter(neighbours));
        neighbours.erase(std::remove_if(neighbours.begin(), neighbours.end(),
                                        [i](size_t elem) { return elem == i; }),
                         neighbours.end());

        model.addSDFGeometry(sdfMorphologyData.materials[i],
                             sdfMorphologyData.geometries[i], neighbours);
    }
}

void VasculatureLoader::_populateGraphIds(const std::string& filename,
                                          Edges& edges) const
{
    std::unique_ptr<HighFive::File> file = std::unique_ptr<HighFive::File>(
        new HighFive::File(filename, HighFive::File::ReadOnly));
    const auto& report = file->getGroup("report");
    const auto& vasculature = report.getGroup("vasculature");
    const auto& edgeGraphIds = vasculature.getDataSet("subgraph_id");
    const auto& edgeTypes = vasculature.getDataSet("type");

    std::vector<uint64_t> graphIds;
    edgeGraphIds.read(graphIds);
    std::vector<uint64_t> types;
    edgeTypes.read(types);

    for (uint64_t i = 0; i < graphIds.size(); ++i)
    {
        edges[i].graphId = graphIds[i];
        edges[i].type = types[i];
    }
}

Edges VasculatureLoader::_loadEdges(const std::string& filename) const
{
    std::unique_ptr<HighFive::File> file = std::unique_ptr<HighFive::File>(
        new HighFive::File(filename, HighFive::File::ReadOnly));

    const auto& nodes = file->getGroup("nodes");
    const auto& vasculature = nodes.getGroup("vasculature");
    const auto& zero = vasculature.getGroup("0");

    std::vector<double> sx, sy, sz, sd;
    std::vector<uint32_t> ss;
    const auto& start_x = zero.getDataSet("start_x");
    start_x.read(sx);
    const auto& start_y = zero.getDataSet("start_y");
    start_y.read(sy);
    const auto& start_z = zero.getDataSet("start_z");
    start_z.read(sz);
    const auto& start_d = zero.getDataSet("start_diameter");
    start_d.read(sd);
    const auto& start_s = zero.getDataSet("section_id");
    start_s.read(ss);

    std::vector<double> ex, ey, ez, ed;
    const auto& end_x = zero.getDataSet("end_x");
    end_x.read(ex);
    const auto& end_y = zero.getDataSet("end_y");
    end_y.read(ey);
    const auto& end_z = zero.getDataSet("end_z");
    end_z.read(ez);
    const auto& end_d = zero.getDataSet("end_diameter");
    end_d.read(ed);

    const uint64_t nbSegments = sx.size();
    PLUGIN_INFO("Added vasculature made of " << nbSegments << " segments");

    Edges edges;
    for (uint64_t i = 0; i < nbSegments; ++i)
    {
        Edge edge;
        edge.start = Vector3d(sx[i], sy[i], sz[i]);
        edge.startRadius = sd[i] * 0.5;
        edge.end = Vector3d(ex[i], ey[i], ez[i]);
        edge.endRadius = ed[i] * 0.5;
        edge.sectionId = ss[i];
        edges[i] = edge;
    }
    return edges;
}

ModelDescriptorPtr VasculatureLoader::importFromFile(
    const std::string& filename, const LoaderProgress& callback,
    const PropertyMap& properties) const
{
    std::set<size_t> materialIds;
    ModelDescriptorPtr modelDescriptor{nullptr};
    try
    {
        PropertyMap props = _defaults;
        props.merge(properties);

        SDFMorphologyData sdfMorphologyData;
        const bool useSDF = props.getProperty<bool>(PROP_USE_SDF_BRANCHES.name);
        const double radiusMultiplier =
            props.getProperty<double>(PROP_RADIUS_MULTIPLIER.name);
        const auto morphologyQuality = stringToEnum<AssetQuality>(
            properties.getProperty<std::string>(PROP_ASSET_QUALITY.name));
        const auto colorScheme = stringToEnum<AssetColorScheme>(
            properties.getProperty<std::string>(PROP_ASSET_COLOR_SCHEME.name));
        const auto gidsAsString =
            properties.getProperty<std::string>(PROP_GIDS.name);

        PLUGIN_ERROR("Section GIDs " << gidsAsString);
        const auto gids = GIDsAsInts(gidsAsString);

        auto edges = _loadEdges(filename);

        _populateGraphIds("/home/favreau/Downloads/report_entry_nodes.h5",
                          edges); // TODO
        auto model = _scene.createModel();

        PLUGIN_INFO("Added vasculature made of " << edges.size() << " edges");
        uint64_t nbLoadedSegments = 0;
        std::set<uint32_t> nbLoadedSections;

        uint64_t i = 0;
        for (const auto& edge : edges)
        {
            const auto& e = edge.second;

            if (e.type == 1)
                continue;

            if (!gids.empty() &&
                std::find(gids.begin(), gids.end(), e.sectionId) == gids.end())
                continue;

            nbLoadedSections.insert(e.sectionId);
            ++nbLoadedSegments;

            callback.updateProgress("Loading vasculature...", i / edges.size());
            const uint64_t userData = i;
            const float startRadius = e.startRadius * radiusMultiplier;

            size_t materialId = DEFAULT_MATERIAL;
            switch (colorScheme)
            {
            case AssetColorScheme::by_segment:
                materialId = i;
                break;
            case AssetColorScheme::by_section:
                materialId = e.sectionId;
                break;
            case AssetColorScheme::by_graph:
                materialId = e.graphId;
                break;
            default:
                materialId = DEFAULT_MATERIAL;
            }
            materialIds.insert(materialId);

            const Vector3f displacementParams = {std::min(startRadius, 0.2f),
                                                 0.75f, 1.0f};
            if (morphologyQuality != AssetQuality::medium)
                if (useSDF)
                {
                    const size_t idx =
                        _addSDFGeometry(sdfMorphologyData,
                                        createSDFSphere(e.start, startRadius,
                                                        userData,
                                                        displacementParams),
                                        {}, materialId, e.sectionId);
                }
                else
                    model->addSphere(materialId,
                                     {e.start, startRadius, userData});

            if (morphologyQuality != AssetQuality::low)
            {
                const float endRadius = e.endRadius * radiusMultiplier;

                if (useSDF)
                {
                    _addStepConeGeometry(useSDF, e.start, startRadius, e.end,
                                         endRadius, materialId, userData,
                                         *model, sdfMorphologyData, e.sectionId,
                                         displacementParams);
                }
                else
                {
                    if (almost_equal(startRadius, endRadius, 100000))
                        model->addCylinder(materialId, {e.start, e.end,
                                                        startRadius, userData});
                    else
                        model->addCone(materialId, {e.start, e.end, startRadius,
                                                    endRadius, userData});
                }
            }
        }

        if (useSDF)
            _finalizeSDFGeometries(*model, sdfMorphologyData);

        // Materials
        for (const auto materialId : materialIds)
        {
            auto nodeMaterial =
                model->createMaterial(materialId, std::to_string(materialId));
            nodeMaterial->setDiffuseColor({1.f, 1.f, 1.f});
            nodeMaterial->setSpecularColor({1.f, 1.f, 1.f});
            nodeMaterial->setSpecularExponent(100.f);
        }

        ModelMetadata metadata = {
            {"GIDs", properties.getProperty<std::string>(PROP_GIDS.name)},
            {"Color scheme", enumToString<AssetColorScheme>(colorScheme)},
            {"Morphology quality",
             enumToString<AssetQuality>(morphologyQuality)},
            {"Number of sections", std::to_string(nbLoadedSections.size())},
            {"Number of segments", std::to_string(nbLoadedSegments)}};
        modelDescriptor =
            std::make_shared<brayns::ModelDescriptor>(std::move(model),
                                                      "Vasculature", metadata);
    }
    catch (const HighFive::FileException& exc)
    {
        PLUGIN_THROW("Could not open vasculature file " + filename + ": " +
                     exc.what());
    }
    return modelDescriptor;
}

void VasculatureLoader::applyGeometryReport(
    Model& model, const ApplyVasculatureGeometryReport& details)
{
    try
    {
        std::unique_ptr<HighFive::File> file = std::unique_ptr<HighFive::File>(
            new HighFive::File(details.path, HighFive::File::ReadOnly));

        std::vector<std::vector<double>> simulationData;
        const auto& report = file->getGroup("report");
        const auto& vasculature = report.getGroup("vasculature");
        const auto& dataset = vasculature.getDataSet("data");
        dataset.read(simulationData);
        const size_t nbFrames = simulationData.size();
        if (nbFrames == 0)
            PLUGIN_THROW("Report does not contain any simulation data: " +
                         details.path);
        std::vector<double> series;
        if (details.debug)
        {
            for (uint64_t i = 0; i < simulationData[0].size(); ++i)
            {
                const float value =
                    static_cast<float>(simulationData[0][i]) *
                    (1.f + details.amplitude *
                               (sin(float(details.frame + i) * M_PI / 360.f) +
                                0.5f * cos(float(details.frame + i) * 3.f *
                                           M_PI / 360.f)));
                series.push_back(value);
            }
        }
        else
        {
            if (details.frame >= nbFrames)
                PLUGIN_THROW("Invalid frame specified for report: " +
                             details.path);
            series = simulationData[details.frame];
        }

        auto& spheresMap = model.getSpheres();
        for (auto& spheres : spheresMap)
            for (auto& sphere : spheres.second)
                sphere.radius = details.amplitude * series[sphere.userData];

        auto& conesMap = model.getCones();
        for (auto& cones : conesMap)
            for (auto& cone : cones.second)
            {
                cone.centerRadius = details.amplitude * series[cone.userData];
                cone.upRadius = details.amplitude * series[cone.userData + 1];
            }

        auto& cylindersMap = model.getCylinders();
        for (auto& cylinders : cylindersMap)
            for (auto& cylinder : cylinders.second)
                cylinder.radius = details.amplitude * series[cylinder.userData];

        model.updateBounds();
        PLUGIN_DEBUG("Vasculature geometry successfully modified using report "
                     << details.path);
    }
    catch (const HighFive::FileException& exc)
    {
        PLUGIN_THROW("Could not open vasculature report file " + details.path +
                     ": " + exc.what());
    }
}

} // namespace vasculature
} // namespace neuroscience
} // namespace circuitexplorer
