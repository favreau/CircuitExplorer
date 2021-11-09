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

#include <brayns/common/geometry/SDFGeometry.h>
#include <brayns/common/loader/Loader.h>
#include <brayns/common/types.h>
#include <brayns/parameters/GeometryParameters.h>

#include <unordered_map>
#include <vector>

namespace circuitexplorer
{
namespace io
{
namespace loader
{
using namespace brayns;

class AdvancedCircuitLoader;
struct ParallelModelContainer;
using GIDOffsets = std::vector<uint64_t>;
using CompartmentReportPtr = std::shared_ptr<brion::CompartmentReport>;

// SDF structures
struct SDFMorphologyData
{
    std::vector<SDFGeometry> geometries;
    std::vector<std::set<size_t>> neighbours;
    std::vector<size_t> materials;
    std::vector<size_t> localToGlobalIdx;
    std::vector<size_t> bifurcationIndices;
    std::unordered_map<size_t, int> geometrySection;
    std::unordered_map<int, std::vector<size_t>> sectionGeometries;
};

struct MorphologyTreeStructure
{
    std::vector<int> sectionParent;
    std::vector<std::vector<size_t>> sectionChildren;
    std::vector<size_t> sectionTraverseOrder;
};

/** Loads morphologies from SWC and H5, and Circuit Config files */
class MorphologyLoader : public Loader
{
public:
    MorphologyLoader(Scene& scene, PropertyMap&& loaderParams);

    /** @copydoc Loader::getName */
    std::string getName() const final;

    /** @copydoc Loader::getSupportedExtensions */
    std::vector<std::string> getSupportedExtensions() const final;

    /** @copydoc Loader::isSupported */
    bool isSupported(const std::string& filename,
                     const std::string& extension) const final;

    /** @copydoc Loader::getCLIProperties */
    static PropertyMap getCLIProperties();

    /** @copydoc Loader::getProperties */
    PropertyMap getProperties() const final;

    /** @copydoc Loader::importFromBlob */
    ModelDescriptorPtr importFromBlob(
        Blob&& blob, const LoaderProgress& callback,
        const PropertyMap& properties) const final;

    /** @copydoc Loader::importFromFile */
    ModelDescriptorPtr importFromFile(
        const std::string& filename, const LoaderProgress& callback,
        const PropertyMap& properties) const final;

    /**
     * @brief importMorphology imports a single morphology from a specified URI
     * @param uri URI of the morphology
     * @param index Index of the morphology
     * @param defaultMaterialId Material to use
     * @param compartmentReport Compartment report to map to the morphology
     * @return Model container
     */
    ParallelModelContainer importMorphology(
        const Gid& gid, const PropertyMap& properties,
        const std::string& source, const uint64_t index,
        const SynapsesInfo& synapsesInfo,
        const Matrix4f& transformation = Matrix4f(),
        CompartmentReportPtr compartmentReport = nullptr,
        const float mitochondriaDensity = 0.f) const;

    /**
     * @brief setBaseMaterialId Set the base material ID for the morphology
     * @param materialId Id of the base material ID for the morphology
     */
    void setBaseMaterialId(const size_t materialId)
    {
        _baseMaterialId = materialId;
    }

    /**
     * @brief createMissingMaterials Checks that all materials exist for
     * existing geometry in the model. Missing materials are created with the
     * default parameters
     */
    static void createMissingMaterials(Model& model,
                                       const PropertyMap& properties = {});

    static const brain::neuron::SectionTypes getSectionTypesFromProperties(
        const PropertyMap& properties);

private:
    /**
     * @brief _getCorrectedRadius Modifies the radius of the geometry according
     * to --radius-multiplier and --radius-correction geometry parameters
     * @param radius Radius to be corrected
     * @return Corrected value of a radius according to geometry parameters
     */
    double _getCorrectedRadius(const PropertyMap& properties,
                               const double radius) const;

    void _importMorphology(const Gid& gid, const PropertyMap& properties,
                           const std::string& source, const uint64_t index,
                           const Matrix4f& transformation,
                           ParallelModelContainer& model,
                           CompartmentReportPtr compartmentReport,
                           const SynapsesInfo& synapsesInfo,
                           const float mitochondriaDensity = 0.f) const;

    /**
     * @brief _importMorphologyAsPoint places sphere at the specified morphology
     * position
     * @param index Index of the current morphology
     * @param material Material that is forced in case geometry parameters do
     * not apply
     * @param compartmentReport Compartment report to map to the morphology
     * @param scene Scene to which the morphology should be loaded into
     */
    void _importMorphologyAsPoint(const PropertyMap& properties,
                                  const uint64_t index,
                                  CompartmentReportPtr compartmentReport,
                                  ParallelModelContainer& model) const;

    /**
     * @brief _importMorphologyFromURI imports a morphology from the specified
     * URI
     * @param uri URI of the morphology
     * @param index Index of the current morphology
     * @param materialFunc A function mapping brain::neuron::SectionType to a
     * material id
     * @param compartmentReport Compartment report to map to the morphology
     * @param model Model container to whichh the morphology should be loaded
     * into
     */
    void _importMorphologyFromURI(const Gid& gid, const PropertyMap& properties,
                                  const std::string& uri, const uint64_t index,
                                  const Matrix4f& transformation,
                                  CompartmentReportPtr compartmentReport,
                                  ParallelModelContainer& model,
                                  const SynapsesInfo& synapsesInfo,
                                  const float mitochondriaDensity) const;

    size_t _addSDFGeometry(SDFMorphologyData& sdfMorphologyData,
                           const SDFGeometry& geometry,
                           const std::set<size_t>& neighbours,
                           const size_t materialId, const int section) const;

    /**
     * Creates an SDF soma by adding and connecting the soma children using cone
     * pills
     */
    void _connectSDFSomaChildren(const PropertyMap& properties,
                                 const Vector3f& somaPosition,
                                 const double somaRadius,
                                 const size_t materialId,
                                 const uint64_t& userDataOffset,
                                 const brain::neuron::Sections& somaChildren,
                                 SDFMorphologyData& sdfMorphologyData) const;

    /**
     * Goes through all bifurcations and connects to all connected SDF
     * geometries it is overlapping. Every section that has a bifurcation will
     * traverse its children and blend the geometries inside the bifurcation.
     */
    void _connectSDFBifurcations(SDFMorphologyData& sdfMorphologyData,
                                 const MorphologyTreeStructure& mts) const;

    /**
     * Calculates all neighbours and adds the geometries to the model container.
     */
    void _finalizeSDFGeometries(ParallelModelContainer& modelContainer,
                                SDFMorphologyData& sdfMorphologyData) const;

    /**
     * Calculates the structure of the morphology tree by finding overlapping
     * beginnings and endings of the sections.
     */
    MorphologyTreeStructure _calculateMorphologyTreeStructure(
        const PropertyMap& properties,
        const brain::neuron::Sections& sections) const;

    /**
     * Adds a Soma geometry to the model
     */
    void _addSomaGeometry(const uint64_t index, const PropertyMap& properties,
                          const brain::neuron::Soma& soma, uint64_t offset,
                          ParallelModelContainer& model,
                          SDFMorphologyData& sdfMorphologyData,
                          const bool useSimulationModel,
                          const bool generateInternals,
                          const float mitochondriaDensity,
                          uint32_t& sdfGroupId) const;

    /**
     * Adds the sphere between the steps in the sections
     */
    void _addStepSphereGeometry(const bool useSDFGeometry, const bool isDone,
                                const Vector3f& position, const double radius,
                                const size_t materialId,
                                const uint64_t& userDataOffset,
                                ParallelModelContainer& model,
                                SDFMorphologyData& sdfMorphologyData,
                                const uint32_t sdfGroupId,
                                const float displacementRatio = 1.f) const;

    /**
     * Adds the cone between the steps in the sections
     */
    void _addStepConeGeometry(
        const bool useSDFGeometry, const Vector3f& position,
        const double radius, const Vector3f& target,
        const double previousRadius, const size_t materialId,
        const uint64_t& userDataOffset, ParallelModelContainer& model,
        SDFMorphologyData& sdfMorphologyData, const uint32_t sdfGroupId,
        const float displacementRatio = 1.f) const;

    /**
     * @brief _getMaterialIdFromColorScheme returns the material id
     * corresponding to the morphology color scheme and the section type
     * @param sectionType Section type of the morphology
     * @return Material Id
     */
    size_t _getMaterialIdFromColorScheme(
        const PropertyMap& properties,
        const brain::neuron::SectionType& sectionType) const;

    /**
     * @brief Computes the distance of a segment to the soma
     * @param section Section containing the segment
     * @param sampleId segment index in the section
     * @return Distance to the soma
     */
    float _distanceToSoma(const brain::neuron::Section& section,
                          const size_t sampleId) const;

    void _addSynapse(const bool useSDF, const brain::Synapse& synapse,
                     const SynapseType synapseType,
                     const brain::neuron::Sections& sections,
                     const Vector3f& somaPosition, const float somaRadius,
                     const Matrix4f& transformation, const size_t materialId,
                     ParallelModelContainer& model,
                     SDFMorphologyData& sdfMorphologyData,
                     uint32_t& sdfGroupId) const;

    void _addSomaInternals(const uint64_t index, ParallelModelContainer& model,
                           const size_t materialId, const float somaRadius,
                           const float mitochondriaDensity,
                           const bool useSDFGeometry,
                           SDFMorphologyData& sdfMorphologyData,
                           uint32_t& sdfGroupId) const;

    void _addSectionInternals(
        const PropertyMap& properties, const bool useSDFGeometry,
        const float sectionLength, const float sectionVolume,
        const brion::Vector4fs& samples, const float mitochondriaDensity,
        const size_t materialId, SDFMorphologyData& sdfMorphologyData,
        uint32_t& sdfGroupId, ParallelModelContainer& model) const;

    size_t _getNbMitochondrionSegments() const;

    size_t _baseMaterialId{NO_MATERIAL};
    PropertyMap _defaults;
};
typedef std::shared_ptr<MorphologyLoader> MorphologyLoaderPtr;

struct ParallelModelContainer
{
    void addSphere(const size_t materialId, const brayns::Sphere& sphere)
    {
        spheres[materialId].push_back(sphere);
    }

    void addCylinder(const size_t materialId, const brayns::Cylinder& cylinder)
    {
        cylinders[materialId].push_back(cylinder);
    }

    void addCone(const size_t materialId, const brayns::Cone& cone)
    {
        cones[materialId].push_back(cone);
    }

    void addSDFGeometry(const size_t materialId,
                        const brayns::SDFGeometry& geom,
                        const std::vector<size_t> neighbours)
    {
        sdfMaterials.push_back(materialId);
        sdfGeometries.push_back(geom);
        sdfNeighbours.push_back(neighbours);
    }

    void moveGeometryToModel(brayns::Model& model)
    {
        moveSpheresToModel(model);
        moveCylindersToModel(model);
        moveConesToModel(model);
        moveSDFGeometriesToModel(model);
        sdfMaterials.clear();
    }

    void moveSpheresToModel(brayns::Model& model)
    {
        for (const auto& sphere : spheres)
        {
            const auto index = sphere.first;
            model.getSpheres()[index].insert(model.getSpheres()[index].end(),
                                             sphere.second.begin(),
                                             sphere.second.end());
        }
        spheres.clear();
    }

    void moveCylindersToModel(brayns::Model& model)
    {
        for (const auto& cylinder : cylinders)
        {
            const auto index = cylinder.first;
            model.getCylinders()[index].insert(
                model.getCylinders()[index].end(), cylinder.second.begin(),
                cylinder.second.end());
        }
        cylinders.clear();
    }

    void moveConesToModel(brayns::Model& model)
    {
        for (const auto& cone : cones)
        {
            const auto index = cone.first;
            model.getCones()[index].insert(model.getCones()[index].end(),
                                           cone.second.begin(),
                                           cone.second.end());
        }
        cones.clear();
    }

    void moveSDFGeometriesToModel(brayns::Model& model)
    {
        const size_t numGeoms = sdfGeometries.size();
        std::vector<size_t> localToGlobalIndex(numGeoms, 0);

        // Add geometries to Model. We do not know the indices of the neighbours
        // yet so we leave them empty.
        for (size_t i = 0; i < numGeoms; i++)
        {
            localToGlobalIndex[i] =
                model.addSDFGeometry(sdfMaterials[i], sdfGeometries[i], {});
        }

        // Write the neighbours using global indices
        uint64_ts neighboursTmp;
        for (uint64_t i = 0; i < numGeoms; i++)
        {
            const uint64_t globalIndex = localToGlobalIndex[i];
            neighboursTmp.clear();

            for (auto localNeighbourIndex : sdfNeighbours[i])
                neighboursTmp.push_back(
                    localToGlobalIndex[localNeighbourIndex]);

            model.updateSDFGeometryNeighbours(globalIndex, neighboursTmp);
        }
        sdfGeometries.clear();
        sdfNeighbours.clear();
    }

    void applyTransformation(const brayns::Matrix4f& transformation)
    {
        for (auto& s : spheres)
            for (auto& sphere : s.second)
                sphere.center =
                    transformVector3f(sphere.center, transformation);
        for (auto& c : cylinders)
            for (auto& cylinder : c.second)
            {
                cylinder.center =
                    transformVector3f(cylinder.center, transformation);
                cylinder.up = transformVector3f(cylinder.up, transformation);
            }
        for (auto& c : cones)
            for (auto& cone : c.second)
            {
                cone.center = transformVector3f(cone.center, transformation);
                cone.up = transformVector3f(cone.up, transformation);
            }
        for (auto& s : sdfGeometries)
        {
            s.p0 = transformVector3f(s.p0, transformation);
            s.p1 = transformVector3f(s.p1, transformation);
        }
    }

    brayns::SpheresMap spheres;
    brayns::CylindersMap cylinders;
    brayns::ConesMap cones;
    brayns::TriangleMeshMap trianglesMeshes;
    MorphologyInfo morphologyInfo;
    std::vector<brayns::SDFGeometry> sdfGeometries;
    std::vector<std::vector<size_t>> sdfNeighbours;
    std::vector<size_t> sdfMaterials;
};

} // namespace loader
} // namespace io
} // namespace circuitexplorer
