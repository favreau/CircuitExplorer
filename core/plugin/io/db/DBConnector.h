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

#pragma once

#include <pqxx/pqxx>

#include <common/Types.h>

namespace circuitexplorer
{
namespace db
{
class DBConnector
{
public:
    DBConnector(const std::string& connectionString, const std::string& schema);
    ~DBConnector();

    void importMorphology(brayns::Scene& scene, const uint64_t guid,
                          const std::string& filename);
    void importMorphologyAsSDF(brayns::Scene& scene, const uint64_t guid,
                               const std::string& filename);

    void importVolume(const uint64_t guid, const brayns::Vector3f& dimensions,
                      const brayns::Vector3f& spacing,
                      const std::string& filename);

    void importCompartmentSimulation(const std::string& blueConfig,
                                     const std::string& reportName,
                                     const uint64_t reportId);

    void importSynapses(const std::string& blueConfig);

private:
    pqxx::connection _connection;
    std::string _connectionString;
    std::string _schema;
};

typedef std::shared_ptr<DBConnector> DBConnectorPtr;
} // namespace db
} // namespace circuitexplorer
