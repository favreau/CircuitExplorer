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

#include "MorphologyLoader.h"
#include <common/Logs.h>
#include <common/Utils.h>

#include <plugin/neuroscience/common/ParallelModelContainer.h>

#include <brayns/common/simulation/AbstractSimulationHandler.h>
#include <brayns/common/types.h>
#include <brayns/engineapi/Material.h>
#include <brayns/engineapi/Model.h>
#include <brayns/engineapi/Scene.h>

#include <brion/uri.h>

#include <boost/filesystem.hpp>

namespace circuitexplorer
{
namespace neuroscience
{
namespace common
{
const std::string LOADER_NAME = "Morphology";
const std::string SUPPORTED_EXTENTION_H5 = "h5";
const std::string SUPPORTED_EXTENTION_SWC = "swc";
const float DEFAULT_SYNAPSE_RADIUS = 0.1f;

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

inline double _getLastSampleRadius(const brain::neuron::Section& section)
{
    return section[-1][3] * 0.5f;
}

MorphologyLoader::MorphologyLoader(Scene& scene, PropertyMap&& loaderParams)
    : Loader(scene)
    , _defaults(loaderParams)
{
}

std::string MorphologyLoader::getName() const
{
    return LOADER_NAME;
}

std::vector<std::string> MorphologyLoader::getSupportedExtensions() const
{
    return {SUPPORTED_EXTENTION_H5, SUPPORTED_EXTENTION_SWC};
}

bool MorphologyLoader::isSupported(const std::string& /*filename*/,
                                   const std::string& extension) const
{
    const std::set<std::string> types = {SUPPORTED_EXTENTION_H5,
                                         SUPPORTED_EXTENTION_SWC};
    return types.find(extension) != types.end();
}

ParallelModelContainer MorphologyLoader::importMorphology(
    const Gid& gid, const PropertyMap& properties, const std::string& source,
    const uint64_t index, const SynapsesInfo& synapsesInfo,
    const Matrix4f& transformation, CompartmentReportPtr compartmentReport,
    const float mitochondriaDensity) const
{
    ParallelModelContainer modelContainer;
    _importMorphology(gid, properties, source, index, transformation,
                      modelContainer, compartmentReport, synapsesInfo,
                      mitochondriaDensity);

    // Apply transformation to everything except synapes
    modelContainer.applyTransformation(transformation);
    return modelContainer;
}

void MorphologyLoader::_importMorphology(
    const Gid& gid, const PropertyMap& properties, const std::string& source,
    const uint64_t index, const Matrix4f& transformation,
    ParallelModelContainer& model, CompartmentReportPtr compartmentReport,
    const SynapsesInfo& synapsesInfo, const float mitochondriaDensity) const
{
    const auto sectionTypes = getSectionTypesFromProperties(properties);

    if (sectionTypes.size() == 1 &&
        sectionTypes[0] == brain::neuron::SectionType::soma)
        _importMorphologyAsPoint(properties, index, compartmentReport, model);
    else
        _importMorphologyFromURI(gid, properties, source, index, transformation,
                                 compartmentReport, model, synapsesInfo,
                                 mitochondriaDensity);
}

float MorphologyLoader::_getCorrectedRadius(const PropertyMap& properties,
                                            const float radius) const
{
    const float radiusCorrection =
        properties.getProperty<double>(PROP_RADIUS_CORRECTION.name);
    const float radiusMultiplier =
        properties.getProperty<double>(PROP_RADIUS_MULTIPLIER.name);
    return (radiusCorrection != 0.0 ? radiusCorrection
                                    : radius * radiusMultiplier * 0.5);
}

void MorphologyLoader::_importMorphologyAsPoint(
    const PropertyMap& properties, const uint64_t index,
    CompartmentReportPtr compartmentReport, ParallelModelContainer& model) const
{
    // If there is no compartment report, the offset in the simulation buffer is
    // the index of the morphology in the circuit
    uint64_t userDataOffset = index;
    if (compartmentReport)
        userDataOffset = compartmentReport->getOffsets()[index][0];

    const double radiusMultiplier =
        properties.getProperty<double>(PROP_RADIUS_MULTIPLIER.name);
    const size_t materialId =
        _getMaterialIdFromColorScheme(properties,
                                      brain::neuron::SectionType::soma);
    model.addSphere(materialId,
                    {model.getMorphologyInfo().somaPosition,
                     static_cast<float>(radiusMultiplier), userDataOffset});
}

size_t MorphologyLoader::_addSDFGeometry(SDFMorphologyData& sdfMorphologyData,
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

void MorphologyLoader::_connectSDFSomaChildren(
    const PropertyMap& properties, const Vector3f& somaPosition,
    const double somaRadius, const size_t materialId,
    const uint64_t& userDataOffset, const brain::neuron::Sections& somaChildren,
    SDFMorphologyData& sdfMorphologyData) const
{
    std::set<size_t> child_indices;

    for (const auto& child : somaChildren)
    {
        const auto& samples = child.getSamples();
        const auto index = std::min(size_t(1), samples.size());
        const auto sample = samples[index];
        const Vector3f s{sample.x, sample.y, sample.z};

        // Create a sigmoid cone with soma radius to center of soma to give it
        // an organic look.
        const auto radiusEnd = _getCorrectedRadius(properties, sample.w * 0.5f);

        const size_t geomIdx = _addSDFGeometry(
            sdfMorphologyData,
            createSDFConePillSigmoid(somaPosition, s, somaRadius, radiusEnd,
                                     userDataOffset, {0.2f, 1.75f, 2.0f}),
            {}, materialId, -1);
        child_indices.insert(geomIdx);
    }

    for (size_t c : child_indices)
        sdfMorphologyData.neighbours[c] = child_indices;
}

void MorphologyLoader::_connectSDFBifurcations(
    SDFMorphologyData& sdfMorphologyData,
    const MorphologyTreeStructure& mts) const
{
    const size_t nbSections = mts.sectionChildren.size();

    for (uint section = 0; section < nbSections; ++section)
    {
        // Find the bifurction geometry id for this section
        size_t bifurcationId = 0;
        bool bifurcationIdFound = false;
        for (size_t bifId : sdfMorphologyData.bifurcationIndices)
        {
            const int bifSection = sdfMorphologyData.geometrySection.at(bifId);

            if (bifSection == static_cast<int>(section))
            {
                bifurcationId = bifId;
                bifurcationIdFound = true;
                break;
            }
        }

        if (!bifurcationIdFound)
            continue;

        // Function for connecting overlapping geometries with current
        // bifurcation
        const auto connectGeometriesToBifurcation =
            [&](const std::vector<size_t>& geometries) {
                const auto& bifGeom =
                    sdfMorphologyData.geometries[bifurcationId];

                for (size_t geomIdx : geometries)
                {
                    // Do not blend yourself
                    if (geomIdx == bifurcationId)
                        continue;

                    const auto& geom = sdfMorphologyData.geometries[geomIdx];
                    const double dist0 = glm::distance2(geom.p0, bifGeom.p0);
                    const double dist1 = glm::distance2(geom.p1, bifGeom.p0);
                    const double radiusSum = geom.r0 + bifGeom.r0;
                    const double radiusSumSq = radiusSum * radiusSum;

                    if (dist0 < radiusSumSq || dist1 < radiusSumSq)
                    {
                        sdfMorphologyData.neighbours[bifurcationId].insert(
                            geomIdx);
                        sdfMorphologyData.neighbours[geomIdx].insert(
                            bifurcationId);
                    }
                }
            };

        // Connect all child sections
        for (const size_t sectionChild : mts.sectionChildren[section])
        {
            if (sdfMorphologyData.sectionGeometries.find(sectionChild) ==
                sdfMorphologyData.sectionGeometries.end())
                PLUGIN_THROW("Invalid SDF connected children section");
            connectGeometriesToBifurcation(
                sdfMorphologyData.sectionGeometries.at(sectionChild));
        }

        // Connect with own section
        if (sdfMorphologyData.sectionGeometries.find(section) ==
            sdfMorphologyData.sectionGeometries.end())
            PLUGIN_THROW("Invalid SDF own section");
        connectGeometriesToBifurcation(
            sdfMorphologyData.sectionGeometries.at(section));
    }
}

void MorphologyLoader::_finalizeSDFGeometries(
    ParallelModelContainer& modelContainer,
    SDFMorphologyData& sdfMorphologyData) const
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

        modelContainer.addSDFGeometry(sdfMorphologyData.materials[i],
                                      sdfMorphologyData.geometries[i],
                                      neighbours);
    }
}

MorphologyTreeStructure MorphologyLoader::_calculateMorphologyTreeStructure(
    const PropertyMap& properties,
    const brain::neuron::Sections& sections) const
{
    const size_t nbSections = sections.size();

    const auto dampenBranchThicknessChangerate = properties.getProperty<bool>(
        PROP_DAMPEN_BRANCH_THICKNESS_CHANGERATE.name);
    if (!dampenBranchThicknessChangerate)
    {
        MorphologyTreeStructure mts;
        mts.sectionTraverseOrder.resize(nbSections);
        mts.sectionParent.resize(nbSections, -1);
        std::iota(mts.sectionTraverseOrder.begin(),
                  mts.sectionTraverseOrder.end(), 0);
        return mts;
    }

    std::vector<std::pair<double, Vector3f>> bifurcationPosition(
        nbSections, std::make_pair<double, Vector3f>(0.0f, {0.f, 0.f, 0.f}));

    std::vector<std::pair<double, Vector3f>> sectionEndPosition(
        nbSections, std::make_pair<double, Vector3f>(0.0f, {0.f, 0.f, 0.f}));

    std::vector<std::vector<size_t>> sectionChildren(nbSections,
                                                     std::vector<size_t>());

    std::vector<int> sectionParent(nbSections, -1);
    std::vector<bool> skipSection(nbSections, true);
    std::vector<bool> addedSection(nbSections, false);

    // Find section bifurcations and end positions
    for (size_t sectionI = 0; sectionI < nbSections; sectionI++)
    {
        const auto& section = sections[sectionI];

        if (section.getType() == brain::neuron::SectionType::soma)
            continue;

        const auto& samples = section.getSamples();
        if (samples.empty())
            continue;

        skipSection[sectionI] = false;

        { // Branch beginning
            const auto& sample = samples[0];

            const auto radius =
                _getCorrectedRadius(properties, sample.w * 0.5f);

            const Vector3f position(sample.x, sample.y, sample.z);

            bifurcationPosition[sectionI].first = radius;
            bifurcationPosition[sectionI].second = position;
        }

        { // Branch end
            const auto& sample = samples.back();
            const auto radius =
                _getCorrectedRadius(properties, sample.w * 0.5f);
            const Vector3f position(sample.x, sample.y, sample.z);
            sectionEndPosition[sectionI].first = radius;
            sectionEndPosition[sectionI].second = position;
        }
    }

    const auto overlaps = [](const std::pair<double, Vector3f>& p0,
                             const std::pair<double, Vector3f>& p1) {
        const double d = (p0.second - p1.second).length();
        const double r = p0.first + p1.first;

        return (d < r);
    };

    // Find overlapping section bifurcations and end positions
    for (size_t sectionI = 0; sectionI < nbSections; sectionI++)
    {
        if (skipSection[sectionI])
            continue;

        for (size_t sectionJ = sectionI + 1; sectionJ < nbSections; sectionJ++)
        {
            if (skipSection[sectionJ])
                continue;

            if (overlaps(bifurcationPosition[sectionJ],
                         sectionEndPosition[sectionI]))
            {
                if (sectionParent[sectionJ] == -1)
                {
                    sectionChildren[sectionI].push_back(sectionJ);
                    sectionParent[sectionJ] = static_cast<size_t>(sectionI);
                }
            }
            else if (overlaps(bifurcationPosition[sectionI],
                              sectionEndPosition[sectionJ]))
            {
                if (sectionParent[sectionI] == -1)
                {
                    sectionChildren[sectionJ].push_back(sectionI);
                    sectionParent[sectionI] = static_cast<size_t>(sectionJ);
                }
            }
        }
    }

    // Fill stack with root sections
    std::vector<size_t> sectionStack;
    for (size_t sectionI = 0; sectionI < nbSections; sectionI++)
    {
        if (skipSection[sectionI])
            continue;
        else if (sectionParent[sectionI] == -1)
            sectionStack.push_back(sectionI);
    }

    // Starting from the roots fill the tree traversal order
    std::vector<size_t> sectionOrder;
    while (!sectionStack.empty())
    {
        const size_t sectionI = sectionStack.back();
        sectionStack.pop_back();
        assert(!addedSection[sectionI]);
        addedSection[sectionI] = true;

        sectionOrder.push_back(sectionI);
        for (const size_t childI : sectionChildren[sectionI])
            sectionStack.push_back(childI);
    }

    MorphologyTreeStructure mts;
    mts.sectionTraverseOrder = std::move(sectionOrder);
    mts.sectionParent = std::move(sectionParent);
    mts.sectionChildren = std::move(sectionChildren);
    return mts;
}

void MorphologyLoader::_addSomaGeometry(
    const uint64_t index, const PropertyMap& properties,
    const brain::neuron::Soma& soma, uint64_t offset,
    ParallelModelContainer& model, SDFMorphologyData& sdfMorphologyData,
    const bool /*useSimulationModel*/, const bool generateInternals,
    const float mitochondriaDensity, uint32_t& sdfGroupId) const
{
    size_t materialId =
        _getMaterialIdFromColorScheme(properties,
                                      brain::neuron::SectionType::soma);
    model.getMorphologyInfo().somaPosition = soma.getCentroid();
    const double somaRadius =
        _getCorrectedRadius(properties, soma.getMeanRadius());
    const bool useSDFSoma =
        properties.getProperty<bool>(PROP_USE_SDF_SOMA.name);

    const auto& children = soma.getChildren();

    if (useSDFSoma)
        _connectSDFSomaChildren(properties,
                                model.getMorphologyInfo().somaPosition,
                                somaRadius, materialId, offset, children,
                                sdfMorphologyData);
    else
    {
        model.addSphere(materialId, {model.getMorphologyInfo().somaPosition,
                                     static_cast<float>(somaRadius), offset});
        // When using a simulation model, parametric geometries
        // must occupy as much space as possible in the mesh.
        // This code inserts a Cone between the soma and the
        // beginning of each branch.
        for (const auto& child : children)
        {
            const auto& samples = child.getSamples();
            const Vector3f sample{samples[0].x, samples[0].y, samples[0].z};

            model.getMorphologyInfo().bounds.merge(sample);
            const double sampleRadius =
                _getCorrectedRadius(properties, samples[0].w * 0.5f);

            model.addCone(materialId,
                          {model.getMorphologyInfo().somaPosition, sample,
                           static_cast<float>(somaRadius),
                           static_cast<float>(sampleRadius), offset});
        }
    }

    if (generateInternals && mitochondriaDensity > 0.f)
    {
        materialId = _getMaterialIdFromColorScheme(
            properties, brain::neuron::SectionType::undefined);
        _addSomaInternals(properties, index, model, materialId, somaRadius,
                          mitochondriaDensity, sdfMorphologyData, sdfGroupId);
    }
}

void MorphologyLoader::_addStepSphereGeometry(
    const bool useSDFGeometry, const bool isDone, const Vector3f& position,
    const double radius, const size_t materialId,
    const uint64_t& userDataOffset, ParallelModelContainer& model,
    SDFMorphologyData& sdfMorphologyData, const uint32_t sdfGroupId,
    const float displacementRatio) const
{
    model.getMorphologyInfo().bounds.merge(position);
    if (useSDFGeometry)
    {
        if (isDone)
        {
            // Since our cone pills already give us a sphere at the end
            // points we don't need to add any sphere between segments
            // except at the bifurcation
            const Vector3f displacementParams = {std::min(radius, 0.2), 1.75f,
                                                 2.0f};
            // DISPLACEMENT_PARAMS * displacementRatio;
            const size_t idx =
                _addSDFGeometry(sdfMorphologyData,
                                createSDFSphere(position, radius,
                                                userDataOffset,
                                                displacementParams),
                                {}, materialId, sdfGroupId);

            sdfMorphologyData.bifurcationIndices.push_back(idx);
        }
    }
    else
        model.addSphere(materialId,
                        {position, static_cast<float>(radius), userDataOffset});
}

void MorphologyLoader::_addStepConeGeometry(
    const bool useSDFGeometry, const Vector3f& position, const double radius,
    const Vector3f& target, const double previousRadius,
    const size_t materialId, const uint64_t& userDataOffset,
    ParallelModelContainer& model, SDFMorphologyData& sdfMorphologyData,
    const uint32_t sdfGroupId, const float displacementRatio) const
{
    model.getMorphologyInfo().bounds.merge(position);
    model.getMorphologyInfo().bounds.merge(target);
    if (useSDFGeometry)
    {
        // const auto displacementParams = DISPLACEMENT_PARAMS *
        // displacementRatio;
        const Vector3f displacementParams = {std::min(radius, 0.2), 1.75f,
                                             2.0f};
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

float MorphologyLoader::_distanceToSoma(const brain::neuron::Section& section,
                                        const size_t sampleId) const
{
    float distance = 0.0;
    if (sampleId > 0)
    {
        const auto& samples = section.getSamples();
        for (size_t i = 0; i < std::min(samples.size(), sampleId) - 1; ++i)
        {
            const auto& a = samples[i];
            const auto& b = samples[i + 1];
            distance = distance + (b - a).length();
        }
    }

    auto s = section;
    while (s.hasParent())
    {
        s = s.getParent();

        const auto& samples = s.getSamples();
        for (size_t i = 0; i < samples.size() - 1; ++i)
        {
            const auto& a = samples[i];
            const auto& b = samples[i + 1];
            distance = distance + (b - a).length();
        }
    }

    return distance * 10.f; // Since user data is uint64_t, we multiply by
                            // 10 to increase the precision of the growth
                            // (first decimal value will then be considered)
}

void MorphologyLoader::_importMorphologyFromURI(
    const Gid& gid, const PropertyMap& properties, const std::string& uri,
    const uint64_t index, const Matrix4f& transformation,
    CompartmentReportPtr compartmentReport, ParallelModelContainer& model,
    const SynapsesInfo& synapsesInfo, const float mitochondriaDensity) const
{
    SDFMorphologyData sdfMorphologyData;

    // Soma
    const auto sectionTypes = getSectionTypesFromProperties(properties);
    const auto morphologyQuality = stringToEnum<AssetQuality>(
        properties.getProperty<std::string>(PROP_ASSET_QUALITY.name));
    const auto userDataType = stringToEnum<UserDataType>(
        properties.getProperty<std::string>(PROP_USER_DATA_TYPE.name));
    const auto useSdfSoma =
        properties.getProperty<bool>(PROP_USE_SDF_SOMA.name);
    const auto useSdfBranches =
        properties.getProperty<bool>(PROP_USE_SDF_BRANCHES.name);
    const auto useSdfNucleus =
        properties.getProperty<bool>(PROP_USE_SDF_NUCLEUS.name);
    const auto useSdfMitochondria =
        properties.getProperty<bool>(PROP_USE_SDF_MITOCHONDRIA.name);
    const auto useSdfSynapses =
        properties.getProperty<bool>(PROP_USE_SDF_SYNAPSES.name);

    const auto dampenBranchThicknessChangerate = properties.getProperty<bool>(
        PROP_DAMPEN_BRANCH_THICKNESS_CHANGERATE.name);
    const auto maxDistanceToSoma = properties.getProperty<double>(
        PROP_MORPHOLOGY_MAX_DISTANCE_TO_SOMA.name);
    const auto generateInternals =
        properties.getProperty<bool>(PROP_INTERNALS.name);

    // If there is no compartment report, the offset in the simulation
    // buffer is the index of the morphology in the circuit
    uint64_t userDataOffset = 0;
    if (compartmentReport)
        userDataOffset = compartmentReport->getOffsets()[index][0];

    const brion::URI source(uri);
    const brain::neuron::Morphology morphology(source);
    const auto sections = morphology.getSections(sectionTypes);

    uint32_t sdfGroupId = 0;
    if (std::find(sectionTypes.begin(), sectionTypes.end(),
                  brain::neuron::SectionType::soma) != sectionTypes.end())
    {
        _addSomaGeometry(index, properties, morphology.getSoma(),
                         userDataOffset, model, sdfMorphologyData,
                         compartmentReport != nullptr, generateInternals,
                         mitochondriaDensity, sdfGroupId);
    }

    // Only the first one or two axon sections are reported, so find the
    // last one and use its offset for all the other axon sections
    uint16_t lastAxon = 0;
    if (compartmentReport &&
        std::find(sectionTypes.begin(), sectionTypes.end(),
                  brain::neuron::SectionType::axon) != sectionTypes.end())
    {
        const auto& counts = compartmentReport->getCompartmentCounts()[index];
        const auto& axon =
            morphology.getSections(brain::neuron::SectionType::axon);
        for (const auto& section : axon)
        {
            if (counts[section.getID()] > 0)
            {
                lastAxon = section.getID();
                continue;
            }
            break;
        }
    }

    const auto morphologyTree =
        _calculateMorphologyTreeStructure(properties, sections);

    // Dendrites and axon
    for (const size_t sectionId : morphologyTree.sectionTraverseOrder)
    {
        const auto& section = sections[sectionId];

        if (section.getType() == brain::neuron::SectionType::soma)
            continue;

        const auto materialId =
            _getMaterialIdFromColorScheme(properties, section.getType());
        const auto& samples = section.getSamples();
        if (samples.empty())
            continue;

        const size_t nbSamples = samples.size();

        auto previousSample = samples[0];
        size_t step = 1;
        switch (morphologyQuality)
        {
        case AssetQuality::low:
            step = nbSamples - 1;
            break;
        case AssetQuality::medium:
            step = nbSamples / 2;
            step = (step == 0) ? 1 : step;
            break;
        default:
            step = 1;
        }

        double segmentStep = 0;
        if (compartmentReport)
        {
            const auto& counts =
                compartmentReport->getCompartmentCounts()[index];
            // Number of compartments usually differs from number of samples
            segmentStep = counts[section.getID()] / double(nbSamples);
        }

        auto previousRadius =
            _getCorrectedRadius(properties,
                                section.hasParent()
                                    ? _getLastSampleRadius(section.getParent())
                                    : samples[0].w * 0.5f);

        bool done = false;
        float sectionVolume = 0.f;
        float sectionLength = 0.f;

        // Axon and dendrites
        for (size_t s = step; !done && s < nbSamples + step; s += step)
        {
            if (s >= (nbSamples - 1))
            {
                s = nbSamples - 1;
                done = true;
            }

            const auto distanceToSoma = _distanceToSoma(section, s);
            if (distanceToSoma > maxDistanceToSoma)
                continue;

            model.getMorphologyInfo().maxDistanceToSoma =
                std::max(model.getMorphologyInfo().maxDistanceToSoma,
                         distanceToSoma);

            switch (userDataType)
            {
            case UserDataType::distance_to_soma:
                userDataOffset = distanceToSoma;
                break;

            case UserDataType::simulation_offset:
                if (compartmentReport)
                {
                    const auto& offsets =
                        compartmentReport->getOffsets()[index];
                    const auto& counts =
                        compartmentReport->getCompartmentCounts()[index];

                    // Update the offset if we have enough compartments aka
                    // a full compartment report. Otherwise we keep the soma
                    // offset which happens for soma reports and use this
                    // for all the sections
                    if (section.getID() < counts.size())
                    {
                        if (counts[section.getID()] > 0)
                            userDataOffset = offsets[section.getID()] +
                                             double(s - step) * segmentStep;
                        else
                        {
                            if (section.getType() ==
                                brain::neuron::SectionType::axon)
                                userDataOffset = offsets[lastAxon];
                            else
                                // This should never happen, but just in
                                // case use an invalid value to show an
                                // error color
                                userDataOffset =
                                    std::numeric_limits<uint64_t>::max();
                        }
                    }
                }
                break;
            case UserDataType::undefined:
                userDataOffset = 0;
                break;
            }

            const auto& sample = samples[s];
            const Vector3f position{sample.x, sample.y, sample.z};
            const Vector3f target(previousSample.x, previousSample.y,
                                  previousSample.z);
            sectionLength += length(position - target);

            model.getMorphologyInfo().bounds.merge(position);
            model.getMorphologyInfo().bounds.merge(target);

            float radius = _getCorrectedRadius(properties, sample.w * 0.5f);
            const float maxRadiusChange = 0.1f;

            const float dist = glm::length(target - position);
            if (dist > 0.0001f && s != samples.size() - 1 &&
                dampenBranchThicknessChangerate)
            {
                const float radiusChange =
                    std::min(std::abs(previousRadius - radius),
                             dist * maxRadiusChange);
                if (radius < previousRadius)
                    radius = previousRadius - radiusChange;
                else
                    radius = previousRadius + radiusChange;
            }

            // Add Geometry
            if (useSdfBranches)
            {
                _addStepSphereGeometry(useSdfBranches, done, position, radius,
                                       materialId, userDataOffset, model,
                                       sdfMorphologyData,
                                       sectionId + sdfGroupId);

                if (position != target && previousRadius > 0.f)
                    _addStepConeGeometry(useSdfBranches, position, radius,
                                         target, previousRadius, materialId,
                                         userDataOffset, model,
                                         sdfMorphologyData,
                                         sectionId + sdfGroupId);
            }
            else
            {
                model.addSphere(materialId, {position, radius});
                if (radius == previousRadius)
                    model.addCylinder(materialId, {position, target, radius});
                else
                    model.addCone(materialId,
                                  {position, target, radius, previousRadius});
            }
            sectionVolume +=
                coneVolume(length(position - target), previousRadius, radius);

            previousSample = sample;
            previousRadius = radius;
        }

        // Generate axon internals (Mitochondria)
        if (generateInternals &&
            section.getType() == brain::neuron::SectionType::axon)
        {
            uint32_t groupId = sectionId + sdfGroupId;
            _addSectionInternals(properties, sectionLength, sectionVolume,
                                 samples, mitochondriaDensity, materialId,
                                 sdfMorphologyData, groupId, model);
            sdfGroupId = groupId;
        }
    }

    // Synapses
    const size_t materialId =
        _getMaterialIdFromColorScheme(properties,
                                      brain::neuron::SectionType::undefined);
    const auto& somaPosition = morphology.getSoma().getCentroid();
    const double somaRadius =
        _getCorrectedRadius(properties, morphology.getSoma().getMeanRadius());

    const auto inverseTransformation = inverse(transformation);
    if (synapsesInfo.afferentSynapses)
        for (const auto& synapse : *synapsesInfo.afferentSynapses)
        {
            if (synapsesInfo.prePostSynapticUsecase &&
                gid != synapsesInfo.postGid)
                if (synapse.getPresynapticGID() != synapsesInfo.preGid)
                    continue;

            _addSynapse(properties, synapse, SynapseType::afferent, sections,
                        somaPosition, somaRadius, inverseTransformation,
                        materialId, model, sdfMorphologyData, sdfGroupId);
        }
    if (synapsesInfo.efferentSynapses && !synapsesInfo.prePostSynapticUsecase)
        for (const auto& synapse : *synapsesInfo.efferentSynapses)
            _addSynapse(properties, synapse, SynapseType::efferent, sections,
                        somaPosition, somaRadius, inverseTransformation,
                        materialId, model, sdfMorphologyData, sdfGroupId);

    // Finalization
    if (useSdfBranches || useSdfSoma || useSdfNucleus || useSdfMitochondria ||
        useSdfSynapses)
    {
        _connectSDFBifurcations(sdfMorphologyData, morphologyTree);
        _finalizeSDFGeometries(model, sdfMorphologyData);
    }
}

void MorphologyLoader::_addSynapse(
    const PropertyMap& properties, const brain::Synapse& synapse,
    const SynapseType synapseType, const brain::neuron::Sections& sections,
    const Vector3f& somaPosition, const float somaRadius,
    const Matrix4f& transformation, const size_t materialId,
    ParallelModelContainer& model, SDFMorphologyData& sdfMorphologyData,
    uint32_t& sdfGroupId) const
{
    uint32_t sectionId;
    uint32_t segmentId;
    Vector3f origin;
    Vector3f target;
    brion::Vector4fs segments;
    float radius = DEFAULT_SYNAPSE_RADIUS;
    bool processRadius = false;
    size_t synapseMaterialId;

    switch (synapseType)
    {
    case SynapseType::afferent:
    {
        auto pos = synapse.getPostsynapticSurfacePosition();
        origin = Vector3f(pos.x, pos.y, pos.z);
        pos = synapse.getPostsynapticCenterPosition();
        target = Vector3f(pos.x, pos.y, pos.z);
        sectionId = synapse.getPostsynapticSectionID();
        if (sectionId < sections.size())
        {
            segments = sections[sectionId].getSamples();
            segmentId = synapse.getPostsynapticSegmentID();
            synapseMaterialId = materialId + MATERIAL_OFFSET_AFFERENT_SYNPASE;
            processRadius = true;
        }
        break;
    }
    case SynapseType::efferent:
    {
        auto pos = synapse.getPresynapticSurfacePosition();
        origin = Vector3f(pos.x, pos.y, pos.z);
        pos = synapse.getPresynapticCenterPosition();
        target = Vector3f(pos.x, pos.y, pos.z);
        sectionId = synapse.getPresynapticSectionID();
        if (sectionId < sections.size())
        {
            segments = sections[sectionId].getSamples();
            segmentId = synapse.getPresynapticSegmentID();
            synapseMaterialId = materialId + MATERIAL_OFFSET_EFFERENT_SYNPASE;
            processRadius = true;
        }
        break;
    }
    }

    // Transformation
    target = transformVector3f(target, transformation);
    if (length(target - somaPosition) <= somaRadius)
        return; // Do not process synapses on the soma
    origin = transformVector3f(origin, transformation);

    if (processRadius && segmentId < segments.size())
    {
        const auto& segment = segments[segmentId];
        radius = segment.w * 0.1f;
    }

    // Spine geometry
    const auto useSDFSynapses =
        properties.getProperty<bool>(PROP_USE_SDF_SYNAPSES.name);

    const float spineRadiusRatio = 0.75f;
    const float spineSmallRadius = radius * 0.15f;
    const float spineBaseRadius = radius * 0.25f;
    const Vector3f direction = target - origin;
    const Vector3f surfaceTarget = origin + direction * 0.5f;
    const float spineLargeRadius = radius * spineRadiusRatio;

    // Create random shape between origin and target
    Vector3f middle = (surfaceTarget + origin) / 2.f;
    const float d = length(surfaceTarget - origin) / 5.f;
    middle +=
        Vector3f(d * (rand() % 1000 / 1000.f), d * (rand() % 1000 / 1000.f),
                 d * (rand() % 1000 / 1000.f));
    const float spineMiddleRadius =
        spineSmallRadius + d * 0.1f * (rand() % 1000 / 1000.f);
    if (useSDFSynapses)
    {
        const float spineDisplacementRatio = 5.f;
        _addStepSphereGeometry(useSDFSynapses, true, origin, spineLargeRadius,
                               synapseMaterialId, -1, model, sdfMorphologyData,
                               sdfGroupId, spineDisplacementRatio);
        _addStepSphereGeometry(useSDFSynapses, true, middle, spineMiddleRadius,
                               synapseMaterialId, -1, model, sdfMorphologyData,
                               sdfGroupId, spineDisplacementRatio);
        if (origin != middle)
            _addStepConeGeometry(useSDFSynapses, origin, spineSmallRadius,
                                 middle, spineMiddleRadius, synapseMaterialId,
                                 -1, model, sdfMorphologyData, sdfGroupId,
                                 spineDisplacementRatio);
        if (middle != surfaceTarget)
            _addStepConeGeometry(useSDFSynapses, middle, spineMiddleRadius,
                                 surfaceTarget, spineBaseRadius,
                                 synapseMaterialId, -1, model,
                                 sdfMorphologyData, sdfGroupId,
                                 spineDisplacementRatio);
        ++sdfGroupId;
    }
    else
    {
        model.addSphere(synapseMaterialId, {origin, radius});
        model.addSphere(synapseMaterialId, {middle, spineMiddleRadius});
        if (origin != middle)
            model.addCone(synapseMaterialId, {origin, middle, spineSmallRadius,
                                              spineMiddleRadius});
        if (middle != surfaceTarget)
            model.addCone(synapseMaterialId,
                          {middle, surfaceTarget, spineMiddleRadius,
                           spineBaseRadius});
    }
}

void MorphologyLoader::_addSomaInternals(
    const PropertyMap& properties, const uint64_t index,
    ParallelModelContainer& model, const size_t materialId,
    const float somaRadius, const float mitochondriaDensity,
    SDFMorphologyData& sdfMorphologyData, uint32_t& sdfGroupId) const
{
    const bool useSDFNucleus =
        properties.getProperty<bool>(PROP_USE_SDF_NUCLEUS.name);
    const bool useSDFMitochondria =
        properties.getProperty<bool>(PROP_USE_SDF_MITOCHONDRIA.name);

    const float mitochondrionRadiusRatio = 0.025f;
    const float mitochondrionDisplacementRatio = 5.f;
    const float nucleusRadius =
        somaRadius * 0.8f; // 80% of the volume of the soma;
    const float mitochondrionRadius =
        somaRadius * mitochondrionRadiusRatio; // 5% of the volume of the soma

    const float somaInnerRadius = nucleusRadius + mitochondrionRadius;
    const float availableVolumeForMitochondria =
        sphereVolume(somaRadius) * mitochondriaDensity;

    // Soma nucleus
    const auto somaPosition = Vector3f(model.getMorphologyInfo().somaPosition);
    const size_t nucleusMaterialId = materialId + MATERIAL_OFFSET_NUCLEUS;
    if (useSDFNucleus)
    {
        _addStepSphereGeometry(useSDFNucleus, true, somaPosition, nucleusRadius,
                               nucleusMaterialId, -1, model, sdfMorphologyData,
                               sdfGroupId);
        ++sdfGroupId;
    }
    else
        model.addSphere(nucleusMaterialId, {somaPosition, nucleusRadius});

    // Mitochondria
    const size_t mitochondrionMaterialId =
        materialId + MATERIAL_OFFSET_MITOCHONDRION;
    float mitochondriaVolume = 0.f;
    while (mitochondriaVolume < availableVolumeForMitochondria)
    {
        const size_t nbSegments = _getNbMitochondrionSegments();
        const auto pointsInSphere =
            getPointsInSphere(nbSegments, somaInnerRadius / somaRadius);
        float previousRadius = mitochondrionRadius;
        for (size_t i = 0; i < nbSegments; ++i)
        {
            // Mitochondrion geometry
            const float radius =
                (1.f + (rand() % 500 / 1000.f)) * mitochondrionRadius;
            const auto p2 = somaPosition + somaRadius * pointsInSphere[i];
            if (useSDFMitochondria)
                _addStepSphereGeometry(useSDFMitochondria, true, p2, radius,
                                       mitochondrionMaterialId, -1, model,
                                       sdfMorphologyData, sdfGroupId,
                                       mitochondrionDisplacementRatio);

            else
                model.addSphere(mitochondrionMaterialId, {p2, radius});
            mitochondriaVolume += sphereVolume(radius);

            if (i > 0)
            {
                const auto p1 =
                    somaPosition + somaRadius * pointsInSphere[i - 1];
                if (useSDFMitochondria)
                    _addStepConeGeometry(useSDFMitochondria, p1, previousRadius,
                                         p2, radius, mitochondrionMaterialId,
                                         -1, model, sdfMorphologyData,
                                         sdfGroupId,
                                         mitochondrionDisplacementRatio);

                else
                    model.addCone(mitochondrionMaterialId,
                                  {p1, p2, previousRadius, radius});
                mitochondriaVolume +=
                    coneVolume(length(p2 - p1), previousRadius, radius);
            }
            previousRadius = radius;
        }
        if (useSDFMitochondria)
            ++sdfGroupId;
    }
}

size_t MorphologyLoader::_getNbMitochondrionSegments() const
{
    return 2 + rand() % 18;
}

void MorphologyLoader::_addSectionInternals(
    const PropertyMap& properties, const float sectionLength,
    const float sectionVolume, const brion::Vector4fs& samples,
    const float mitochondriaDensity, const size_t materialId,
    SDFMorphologyData& sdfMorphologyData, uint32_t& sdfGroupId,
    ParallelModelContainer& model) const
{
    const bool useSDFMitochondria =
        properties.getProperty<bool>(PROP_USE_SDF_MITOCHONDRIA.name);

    // Add mitochondria (density is per section, not for the full axon)
    const float mitochondrionSegmentSize = 0.25f;
    const float mitochondrionRadiusRatio = 0.25f;

    const size_t nbMaxMitochondrionSegments =
        sectionLength / mitochondrionSegmentSize;
    const float indexRatio =
        float(samples.size()) / float(nbMaxMitochondrionSegments);

    float mitochondriaVolume = 0.f;

    size_t nbSegments = _getNbMitochondrionSegments();
    int mitochondrionSegment =
        -(rand() % (1 + nbMaxMitochondrionSegments / 10));
    float previousRadius;
    Vector3f previousPosition;

    for (size_t step = 0; step < nbMaxMitochondrionSegments; ++step)
    {
        if (mitochondriaVolume < sectionVolume * mitochondriaDensity &&
            mitochondrionSegment >= 0 && mitochondrionSegment < nbSegments)
        {
            const size_t srcIndex = size_t(step * indexRatio);
            const size_t dstIndex = size_t(step * indexRatio) + 1;
            if (dstIndex < samples.size())
            {
                const auto& srcSample = samples[srcIndex];
                const auto& dstSample = samples[dstIndex];
                const float srcRadius =
                    _getCorrectedRadius(properties, srcSample.w * 0.5f);
                const Vector3f srcPosition{
                    srcSample.x + srcRadius * (rand() % 100 - 50) / 500.f,
                    srcSample.y + srcRadius * (rand() % 100 - 50) / 500.f,
                    srcSample.z + srcRadius * (rand() % 100 - 50) / 500.f};
                const float dstRadius =
                    _getCorrectedRadius(properties, dstSample.w * 0.5f);
                const Vector3f dstPosition{
                    dstSample.x + dstRadius * (rand() % 100 - 50) / 500.f,
                    dstSample.y + dstRadius * (rand() % 100 - 50) / 500.f,
                    dstSample.z + dstRadius * (rand() % 100 - 50) / 500.f};

                const Vector3f direction = dstPosition - srcPosition;
                const Vector3f position =
                    srcPosition + direction * (step * indexRatio - srcIndex);
                const float mitocondrionRadius =
                    srcRadius * mitochondrionRadiusRatio;
                const float radius =
                    (1.f + ((rand() % 500) / 1000.f)) * mitocondrionRadius;

                const size_t mitochondrionMaterialId =
                    materialId + MATERIAL_OFFSET_MITOCHONDRION;
                if (useSDFMitochondria)
                    _addStepSphereGeometry(useSDFMitochondria, true, position,
                                           radius, mitochondrionMaterialId, -1,
                                           model, sdfMorphologyData, sdfGroupId,
                                           mitochondrionRadiusRatio);
                else
                    model.addSphere(mitochondrionMaterialId,
                                    {position, radius});
                mitochondriaVolume += sphereVolume(radius);

                if (mitochondrionSegment > 0)
                {
                    if (useSDFMitochondria)
                        _addStepConeGeometry(useSDFMitochondria, position,
                                             radius, previousPosition,
                                             previousRadius,
                                             mitochondrionMaterialId, -1, model,
                                             sdfMorphologyData, sdfGroupId,
                                             mitochondrionRadiusRatio);
                    else
                        model.addCone(mitochondrionMaterialId,
                                      {position, previousPosition, radius,
                                       previousRadius});
                    mitochondriaVolume +=
                        coneVolume(length(position - previousPosition), radius,
                                   previousRadius);
                }

                previousPosition = position;
                previousRadius = radius;
            }
        }
        ++mitochondrionSegment;

        if (mitochondrionSegment == nbSegments)
        {
            mitochondrionSegment =
                -(rand() % (1 + nbMaxMitochondrionSegments / 10));
            nbSegments = _getNbMitochondrionSegments();
        }
    }
}

size_t MorphologyLoader::_getMaterialIdFromColorScheme(
    const PropertyMap& properties,
    const brain::neuron::SectionType& sectionType) const
{
    size_t materialId;
    const auto colorScheme = stringToEnum<AssetColorScheme>(
        properties.getProperty<std::string>(PROP_ASSET_COLOR_SCHEME.name));
    switch (colorScheme)
    {
    case AssetColorScheme::by_section:
        switch (sectionType)
        {
        case brain::neuron::SectionType::soma:
            materialId = MATERIAL_OFFSET_SOMA;
            break;
        case brain::neuron::SectionType::axon:
            materialId = MATERIAL_OFFSET_AXON;
            break;
        case brain::neuron::SectionType::dendrite:
            materialId = MATERIAL_OFFSET_DENDRITE;
            break;
        case brain::neuron::SectionType::apicalDendrite:
            materialId = MATERIAL_OFFSET_APICAL_DENDRITE;
            break;
        default:
            materialId = 0;
            break;
        }
        break;
    default:
        materialId = 0;
    }
    return _baseMaterialId + materialId;
}

ModelDescriptorPtr MorphologyLoader::importFromBlob(
    Blob&& /*blob*/, const LoaderProgress& /*callback*/,
    const PropertyMap& /*properties*/) const
{
    PLUGIN_THROW("Loading a morphology from memory is currently not supported");
}

ModelDescriptorPtr MorphologyLoader::importFromFile(
    const std::string& fileName, const LoaderProgress& /*callback*/,
    const PropertyMap& properties) const
{
    // TODO: This needs to be done to work around wrong types coming from
    // the UI
    PropertyMap props = _defaults;
    props.merge(properties);
    // TODO: This needs to be done to work around wrong types coming from
    // the UI

    auto model = _scene.createModel();
    auto modelContainer =
        importMorphology(0, props, fileName, 0, SynapsesInfo());
    modelContainer.moveGeometryToModel(*model);
    createMissingMaterials(*model);

    auto modelDescriptor =
        std::make_shared<ModelDescriptor>(std::move(model), fileName);
    return modelDescriptor;
}

PropertyMap MorphologyLoader::getProperties() const
{
    return _defaults;
}

PropertyMap MorphologyLoader::getCLIProperties()
{
    PropertyMap pm(LOADER_NAME);
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
    pm.setProperty(PROP_DAMPEN_BRANCH_THICKNESS_CHANGERATE);
    pm.setProperty(PROP_USER_DATA_TYPE);
    pm.setProperty(PROP_ASSET_COLOR_SCHEME);
    pm.setProperty(PROP_ASSET_QUALITY);
    pm.setProperty(PROP_MORPHOLOGY_MAX_DISTANCE_TO_SOMA);
    pm.setProperty(PROP_INTERNALS);
    return pm;
}

const brain::neuron::SectionTypes
    MorphologyLoader::getSectionTypesFromProperties(
        const PropertyMap& properties)
{
    brain::neuron::SectionTypes sectionTypes;
    if (properties.getProperty<bool>(PROP_SECTION_TYPE_SOMA.name))
        sectionTypes.push_back(brain::neuron::SectionType::soma);
    if (properties.getProperty<bool>(PROP_SECTION_TYPE_AXON.name))
        sectionTypes.push_back(brain::neuron::SectionType::axon);
    if (properties.getProperty<bool>(PROP_SECTION_TYPE_DENDRITE.name))
        sectionTypes.push_back(brain::neuron::SectionType::dendrite);
    if (properties.getProperty<bool>(PROP_SECTION_TYPE_APICAL_DENDRITE.name))
        sectionTypes.push_back(brain::neuron::SectionType::apicalDendrite);
    return sectionTypes;
}

void MorphologyLoader::createMissingMaterials(Model& model,
                                              const PropertyMap& properties)
{
    std::set<size_t> materialIds;
    for (auto& spheres : model.getSpheres())
        materialIds.insert(spheres.first);
    for (auto& cylinders : model.getCylinders())
        materialIds.insert(cylinders.first);
    for (auto& cones : model.getCones())
        materialIds.insert(cones.first);
    for (auto& meshes : model.getTriangleMeshes())
        materialIds.insert(meshes.first);
    for (auto& sdfGeometries : model.getSDFGeometryData().geometryIndices)
        materialIds.insert(sdfGeometries.first);

    auto materials = model.getMaterials();
    for (const auto materialId : materialIds)
    {
        const auto it = materials.find(materialId);
        if (it == materials.end())
            model.createMaterial(materialId, std::to_string(materialId),
                                 properties);
    }

    auto simulationHandler = model.getSimulationHandler();
    if (simulationHandler)
        for (const auto& material : materials)
            simulationHandler->bind(material.second);
}
} // namespace common
} // namespace neuroscience
} // namespace circuitexplorer
