/* Copyright (c) 2020, EPFL/Blue Brain Project
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

#include "DBConnector.h"

#include <common/Logs.h>
#include <plugin/neuroscience/common/MorphologyLoader.h>

#include <brayns/engineapi/Model.h>
#include <brayns/engineapi/Scene.h>

#include <fstream>
#include <omp.h>

namespace circuitexplorer
{
namespace db
{
using namespace brayns;
using namespace neuroscience;
using namespace common;

const uint16_t NB_CONNECTIONS = 20;

DBConnector::DBConnector(const std::string& connectionString,
                         const std::string& schema)
    : _connection(connectionString)
    , _connectionString(connectionString)
    , _schema(schema)
{
}

DBConnector::~DBConnector()
{
    _connection.disconnect();
}

void DBConnector::importCompartmentSimulation(const std::string& blueConfig,
                                              const std::string& reportName,
                                              const uint64_t reportId)
{
    std::vector<pqxx::connection*> connections;
    for (size_t i = 0; i < NB_CONNECTIONS; ++i)
    {
        PLUGIN_INFO("Initializing connection " << i << ": "
                                               << _connectionString);
        connections.push_back(new pqxx::connection(_connectionString));
        PLUGIN_INFO((connections[i] ? "OK" : "KO"));
    }

    PLUGIN_INFO("Import compartment simulation");
    const brion::BlueConfig bc(blueConfig);
    brion::CompartmentReport compartmentReport(bc.getReportSource(reportName),
                                               brion::MODE_READ);
    const auto guids = compartmentReport.getGIDs();
    const auto dt = compartmentReport.getTimestep();
    const auto nbFrames = compartmentReport.getEndTime() / dt;

    PLUGIN_INFO("Importing " << guids.size() << " guids");

    // Open report
    const brion::CompartmentReport report(bc.getReportSource(reportName),
                                          brion::MODE_READ, guids);

    uint64_ts morphologyIds;
    {
        pqxx::nontransaction transaction(*connections[0]);
        std::string guidsAsStr;
        for (const auto guid : guids)
        {
            if (!guidsAsStr.empty())
                guidsAsStr += ",";
            guidsAsStr += std::to_string(guid);
        }
        const std::string sql = "SELECT morphology_guid FROM " + _schema +
                                ".node WHERE guid IN (" + guidsAsStr +
                                ") ORDER BY guid";
        auto res = transaction.exec(sql);
        for (auto c = res.begin(); c != res.end(); ++c)
            morphologyIds.push_back(c[0].as<uint64_t>());
    }

    std::map<uint64_t, std::vector<uint64_t>> morphologySectionNbPoints;
    {
        pqxx::nontransaction transaction(*connections[0]);
        const std::string sql =
            "SELECT morphology_guid, nb_points FROM " + _schema +
            ".section ORDER BY morphology_guid, section_guid";
        auto res = transaction.exec(sql);
        for (auto c = res.begin(); c != res.end(); ++c)
            morphologySectionNbPoints[c[0].as<uint64_t>()].push_back(
                c[1].as<uint64_t>());
    }

    uint64_ts morphologyNbSections;
    {
        pqxx::nontransaction transaction(*connections[0]);
        const std::string sql = "SELECT COUNT(section_guid) FROM " + _schema +
                                ".section GROUP BY morphology_guid ORDER BY "
                                "morphology_guid";
        auto res = transaction.exec(sql);
        for (auto c = res.begin(); c != res.end(); ++c)
            morphologyNbSections.push_back(c[0].as<uint64_t>());
    }

    const auto& allOffsets = report.getOffsets();

    uint64_t i;
    // #pragma omp parallel for num_threads(NB_CONNECTIONS)
    for (i = 0; i < guids.size(); ++i)
    {
        try
        {
            const auto morphologyId = morphologyIds[i];
            auto it = guids.begin();
            std::advance(it, i);
            const auto neuronId = *(it);

            pqxx::work transaction(*connections[omp_get_thread_num()]);
            std::map<uint64_t, floats> values;

            for (uint64_t frame = 0; frame < nbFrames; ++frame)
            {
                const auto voltages = *report.loadFrame(frame * dt).get().data;
                const auto& offsets = allOffsets[i];
                const auto nbSections = morphologyNbSections[morphologyId];
                uint64_t lastOffset = 0;
                for (uint64_t o = 0; o < offsets.size(); ++o)
                {
                    const auto offset = offsets[o];
                    if (offset > voltages.size())
                        continue;
                    for (uint64_t e = lastOffset; e < offset; ++e)
                        values[o].push_back(voltages[e]); // TODO: o is one too
                                                          // late
                    lastOffset = offset;
                }
            }

            for (const auto& value : values)
            {
                const pqxx::binarystring data((void*)value.second.data(),
                                              value.second.size() *
                                                  sizeof(float));
                transaction.exec_params("INSERT INTO " + _schema +
                                            ".compartment_report VALUES "
                                            "($1, $2, $3, $4)",
                                        reportId, neuronId, value.first, data);
            }
            transaction.commit();
        }
        catch (pqxx::sql_error& e)
        {
            PLUGIN_ERROR(e.what());
        }
    }

    for (i = 0; i < NB_CONNECTIONS; ++i)
        connections[i]->disconnect();
}

void DBConnector::importMorphology(Scene& scene, const uint64_t guid,
                                   const std::string& filename)
{
    PLUGIN_INFO("Importing morphology " << guid << " from " << filename);
    PropertyMap props;
    props.setProperty({"070RealisticSoma", false});
    props.setProperty(
        {"090AssetQuality", enumToString<AssetQuality>(AssetQuality::high)});
    props.setProperty(
        {"022UserDataType", enumToString(UserDataType::undefined)});
    props.setProperty({"060UseSdfgeometry", false});
    props.setProperty({"061DampenBranchThicknessChangerate", false});
    props.setProperty(
        {"080AssetColorScheme", enumToString(CircuitColorScheme::none)});
    props.setProperty({"051RadiusCorrection", 0.});
    props.setProperty({"050RadiusMultiplier", 1.});
    props.setProperty({"052SectionTypeSoma", false});
    props.setProperty(
        {"091MaxDistanceToSoma", std::numeric_limits<double>::max()});

    pqxx::work transaction(_connection);

    const std::vector<std::string> columns = {"axon", "dendrite",
                                              "apicaldendrite"};
    for (size_t section = 0; section < columns.size(); ++section)
    {
        try
        {
            props.setProperty({"053SectionTypeAxon", section == 0});
            props.setProperty({"054SectionTypeDendrite", section == 1});
            props.setProperty({"055SectionTypeApicalDendrite", section == 2});

            MorphologyLoader loader(scene, std::move(props));
            auto modelDescriptor =
                loader.importFromFile(filename, LoaderProgress(), props);

            auto& model = modelDescriptor->getModel();

            auto& spheres = model.getSpheres()[0];
            uint64_t nbSpheres = spheres.size();
            auto& cylinders = model.getCylinders()[0];
            uint64_t nbCylinders = cylinders.size();
            auto& cones = model.getCones()[0];
            uint64_t nbCones = cones.size();

            const auto bufferSize =
                sizeof(uint64_t) + sizeof(Sphere) * nbSpheres +
                sizeof(uint64_t) + sizeof(Cylinder) * nbCylinders +
                sizeof(uint64_t) + sizeof(Cone) * nbCones;

            std::vector<uint8_t> buffer(bufferSize);
            auto index = buffer.data();
            memcpy(index, &nbSpheres, sizeof(uint64_t));
            index += sizeof(uint64_t);
            if (nbSpheres > 0)
            {
                memcpy(index, spheres.data(), sizeof(Sphere) * nbSpheres);
                index += sizeof(Sphere) * nbSpheres;
            }

            memcpy(index, &nbCylinders, sizeof(uint64_t));
            index += sizeof(uint64_t);
            if (nbCylinders > 0)
            {
                memcpy(index, cylinders.data(), sizeof(Cylinder) * nbCylinders);
                index += sizeof(Cylinder) * nbCylinders;
            }

            memcpy(index, &nbCones, sizeof(uint64_t));
            index += sizeof(uint64_t);
            if (nbCones > 0)
                memcpy(index, cones.data(), sizeof(Cone) * nbCones);

            pqxx::binarystring data((void*)buffer.data(), bufferSize);
            transaction.exec_params("UPDATE " + _schema + ".morphology SET " +
                                        columns[section] + "=$1 WHERE guid=$2",
                                    data, guid);
            transaction.commit();

            PLUGIN_INFO("Successfully imported "
                        << nbSpheres << " spheres, " << nbCylinders
                        << " cylinders and " << nbCones << " cones");
        }
        catch (const std::runtime_error& e)
        {
            transaction.abort();
            PLUGIN_THROW(e.what());
        }
    }
}

void DBConnector::importMorphologyAsSDF(Scene& scene, const uint64_t guid,
                                        const std::string& filename)
{
    PLUGIN_INFO("Importing morphology " << guid << " as SDF from " << filename);
    PropertyMap props;
    props.setProperty({"070RealisticSoma", false});
    props.setProperty(
        {"090AssetQuality", enumToString<AssetQuality>(AssetQuality::high)});
    props.setProperty(
        {"022UserDataType", enumToString(UserDataType::distance_to_soma)});
    props.setProperty({"060UseSdfgeometry", true});
    props.setProperty({"061DampenBranchThicknessChangerate", true});
    props.setProperty(
        {"080AssetColorScheme", enumToString(CircuitColorScheme::none)});
    props.setProperty({"051RadiusCorrection", 0.});
    props.setProperty({"050RadiusMultiplier", 1.});
    props.setProperty({"052SectionTypeSoma", true});
    props.setProperty(
        {"091MaxDistanceToSoma", std::numeric_limits<double>::max()});

    pqxx::work transaction(_connection);
    try
    {
        props.setProperty({"053SectionTypeAxon", true});
        props.setProperty({"054SectionTypeDendrite", true});
        props.setProperty({"055SectionTypeApicalDendrite", true});

        MorphologyLoader loader(scene, std::move(props));
        auto modelDescriptor =
            loader.importFromFile(filename, LoaderProgress(), props);

        if (!modelDescriptor)
            PLUGIN_THROW("Failed to load " + filename);

        auto& model = modelDescriptor->getModel();

        auto& sdfGeometry = model.getSDFGeometryData();

        // Geometries
        const uint64_t nbGeometries = sdfGeometry.geometries.size();
        const uint64_t geometriesBufferSize =
            sizeof(SDFGeometry) * nbGeometries;

        // Indices
        const uint64_t nbGeometryIndices = sdfGeometry.geometryIndices.size();
        uint64_t geometryIndicesSize{0};
        for (const auto& geometryIndex : sdfGeometry.geometryIndices)
        {
            // Map key
            geometryIndicesSize += sizeof(size_t);
            // Map value (Length + buffer)
            geometryIndicesSize += sizeof(uint64_t);
            geometryIndicesSize +=
                geometryIndex.second.size() * sizeof(uint64_t);
        }

        // Neighbours
        uint64_t neighboursSize{0};
        const uint64_t nbNeighbours = sdfGeometry.neighbours.size();
        for (const auto& neighbour : sdfGeometry.neighbours)
        {
            geometryIndicesSize += sizeof(uint64_t);
            neighboursSize += neighbour.size() * sizeof(size_t);
        }

        // Flat neighbours
        const uint64_t nbNeighboursFlat = sdfGeometry.neighboursFlat.size();
        const uint64_t neighboursFlatSize =
            sizeof(uint64_t) * sdfGeometry.neighboursFlat.size();

        const auto bufferSize = sizeof(uint64_t) + geometriesBufferSize +
                                sizeof(uint64_t) + geometryIndicesSize +
                                sizeof(uint64_t) + neighboursSize +
                                sizeof(uint64_t) + neighboursFlatSize;

        // Create DB buffer
        std::vector<uint8_t> buffer(bufferSize);

        // Geometries
        auto index = buffer.data();
        memcpy(index, &nbGeometries, sizeof(uint64_t));
        index += sizeof(uint64_t);
        if (geometriesBufferSize > 0)
        {
            memcpy(index, sdfGeometry.geometries.data(), geometriesBufferSize);
            index += geometriesBufferSize;
        }

        // Indices
        memcpy(index, &nbGeometryIndices, sizeof(uint64_t));
        index += sizeof(uint64_t);
        if (nbGeometryIndices > 0)
            for (const auto& geometryIndex : sdfGeometry.geometryIndices)
            {
                const size_t key = geometryIndex.first;
                memcpy(index, &key, sizeof(size_t));
                index += sizeof(size_t);

                const uint64_t nbElements = geometryIndex.second.size();
                memcpy(index, &nbElements, sizeof(nbElements));
                index += sizeof(nbElements);

                const uint64_t geometryIndexBufferSize =
                    geometryIndex.second.size() * sizeof(uint64_t);
                memcpy(index, geometryIndex.second.data(),
                       geometryIndexBufferSize);
                index += geometryIndexBufferSize;
            }

        // Neighbours
        memcpy(index, &nbNeighbours, sizeof(uint64_t));
        index += sizeof(uint64_t);
        for (const auto& neighbour : sdfGeometry.neighbours)
        {
            const uint64_t neighbourSize = neighbour.size();
            memcpy(index, &neighbourSize, sizeof(uint64_t));
            index += sizeof(uint64_t);
            if (neighbourSize > 0)
            {
                const uint64_t neighbourBufferSize =
                    neighbourSize * sizeof(size_t);
                memcpy(index, neighbour.data(), neighbourBufferSize);
                index += neighbourBufferSize;
            }
        }

        // Flat neighbours
        memcpy(index, &neighboursFlatSize, sizeof(uint64_t));
        index += sizeof(uint64_t);
        if (neighboursFlatSize > 0)
        {
            const uint64_t neighboursFlatBufferSize =
                neighboursFlatSize * sizeof(uint64_t);
            memcpy(index, sdfGeometry.neighboursFlat.data(),
                   neighboursFlatBufferSize);
        }

        PLUGIN_INFO(bufferSize << " vs " << index - buffer.data());
        const pqxx::binarystring data((void*)buffer.data(), bufferSize);
        transaction.exec_params("UPDATE " + _schema +
                                    ".morphology SET sdf=$1 WHERE guid=$2",
                                data, guid);
        transaction.commit();

        PLUGIN_INFO("Successfully imported SDF: "
                    << nbGeometries << " geometries, " << nbGeometryIndices
                    << " indices, " << nbNeighbours << " neighbours, and "
                    << nbNeighboursFlat << " flat neighbours");
    }
    catch (const std::runtime_error& e)
    {
        transaction.abort();
        PLUGIN_THROW(e.what());
    }
}

void DBConnector::importVolume(const uint64_t guid, const Vector3f& dimensions,
                               const Vector3f& spacing,
                               const std::string& filename)
{
    pqxx::work transaction(_connection);
    try
    {
        std::ifstream file(filename, std::fstream::binary);
        if (!file.good())
            PLUGIN_THROW("Could not open volume file");

        const std::ifstream::pos_type pos = file.tellg();
        std::vector<char> buffer(pos);
        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), pos);
        file.close();

        const pqxx::binarystring voxels((void*)buffer.data(), buffer.size());
        transaction.exec_params(
            "INSERT INTO " + _schema +
                ".volume VALUES "
                "($1, ARRAY[$2,$3,$4], ARRAY[$5,$6,$7], $8)",
            guid, dimensions.x, dimensions.y, dimensions.z, spacing.x,
            spacing.y, spacing.z, voxels);
        transaction.commit();
    }
    catch (const std::runtime_error& e)
    {
        transaction.abort();
        PLUGIN_THROW(e.what());
    }
}

void DBConnector::importSynapses(const std::string& blueConfig)
{
    std::vector<pqxx::connection*> connections;
    for (size_t i = 0; i < NB_CONNECTIONS; ++i)
    {
        PLUGIN_INFO("Initializing connection " << i << ": "
                                               << _connectionString);
        connections.push_back(new pqxx::connection(_connectionString));
        PLUGIN_INFO((connections[i] ? "OK" : "KO"));
    }

    PLUGIN_INFO("Import synapses");
    const brion::BlueConfig blueConfiguration(blueConfig);
    const brain::Circuit circuit(blueConfiguration);
    const auto allGuids = circuit.getGIDs();
    uint64_ts guids;
    for (const auto guid : allGuids)
        guids.push_back(guid);
    const auto synapses = std::unique_ptr<brain::Synapses>(
        new brain::Synapses(circuit.getAfferentSynapses(allGuids)));

    uint64_t progress = 0;
    uint64_t i;
#pragma omp parallel for num_threads(NB_CONNECTIONS)
    for (i = 0; i < synapses->size(); ++i)
    {
        pqxx::work transaction(*connections[omp_get_thread_num()]);
        try
        {
            const auto synapse = (*synapses)[i];
            const auto synapseClass = 0;
            const auto preNeuronId = synapse.getPresynapticGID() - 1;
            const auto preSectionId = synapse.getPresynapticSectionID();
            const auto preSegmentId = synapse.getPresynapticSegmentID();
            const auto postNeuronId = synapse.getPostsynapticGID() - 1;
            const auto postSectionId = synapse.getPostsynapticSectionID();
            const auto postSegmentId = synapse.getPostsynapticSegmentID();
            const auto postSurface = synapse.getPostsynapticSurfacePosition();
            const auto preSurface = synapse.getPresynapticSurfacePosition();
            const auto center = (postSurface + preSurface) * 0.5;

            transaction.exec_params(
                "INSERT INTO " + _schema +
                    ".synapse VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, "
                    "$10, $11, $12, $13)",
                synapseClass, preNeuronId, preSectionId, preSegmentId,
                postNeuronId, postSectionId, postSegmentId, preSurface.x,
                preSurface.y, preSurface.z, center.x, center.y, center.z);

            transaction.exec_params("INSERT INTO " + _schema +
                                        ".synapse VALUES ($1, $2, $3, $4, $5, "
                                        "$6, $7, $8, $9, $10, $11, $12, $13)",
                                    synapseClass, postNeuronId, postSectionId,
                                    postSegmentId, preNeuronId, preSectionId,
                                    preSegmentId, postSurface.x, postSurface.y,
                                    postSurface.z, center.x, center.y,
                                    center.z);

#if 0
            const auto afferentSynapses = std::unique_ptr<brain::Synapses>(
                new brain::Synapses(circuit.getAfferentSynapses({guid})));
            for (const auto& synapse : *afferentSynapses)
            {
                const auto synapseClass = 0;
                const auto preNeuronId = guid - 1;
                const auto preSectionId = synapse.getPresynapticSectionID();
                const auto preSegmentId = synapse.getPresynapticSegmentID();
                const auto postNeuronId = synapse.getPostsynapticGID() - 1;
                const auto postSectionId = synapse.getPostsynapticSectionID();
                const auto postSegmentId = synapse.getPostsynapticSegmentID();
                const auto postSurface =
                    synapse.getPostsynapticSurfacePosition();
                const auto surface = synapse.getPresynapticSurfacePosition();
                const auto center = surface + (postSurface - surface) * 0.5;

                transaction.exec_params(
                    "INSERT INTO " + _schema +
                        ".synapse VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, "
                        "$10, $11, $12, $13)",
                    synapseClass, preNeuronId, preSectionId, preSegmentId,
                    postNeuronId, postSectionId, postSegmentId, surface.x,
                    surface.y, surface.z, center.x, center.y, center.z);
            }
#endif
            transaction.commit();
        }
        catch (pqxx::sql_error& e)
        {
            PLUGIN_ERROR("PostgreSQL: " << e.what());
        }
        catch (const std::runtime_error& e)
        {
            PLUGIN_ERROR(e.what());
        }
        catch (...)
        {
            PLUGIN_ERROR("Unknown exception");
        }
#pragma omp critical
        ++progress;

#pragma omp critical
        PLUGIN_PROGRESS("Importing synapses", progress, synapses->size());
    }
}
} // namespace db
} // namespace circuitexplorer
