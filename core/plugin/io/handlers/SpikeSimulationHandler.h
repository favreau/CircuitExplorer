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

#include <brain/brain.h>
#include <brayns/api.h>
#include <brayns/common/simulation/AbstractSimulationHandler.h>
#include <brayns/common/types.h>
#include <brayns/engineapi/Scene.h>

namespace circuitexplorer
{
namespace io
{
namespace handler
{
using namespace brayns;

typedef std::shared_ptr<brain::SpikeReportReader> SpikeReportReaderPtr;

class SpikeSimulationHandler : public AbstractSimulationHandler
{
public:
    SpikeSimulationHandler(const std::string& reportPath,
                           const brain::GIDSet& gids);
    SpikeSimulationHandler(const SpikeSimulationHandler& rhs);

    void* getFrameData(const uint32_t frame) final;

    const std::string& getReportPath() const { return _reportPath; }
    SpikeReportReaderPtr getReport() const { return _spikeReport; }
    const brain::GIDSet& getGIDs() const { return _gids; }
    AbstractSimulationHandlerPtr clone() const final;

private:
    std::string _reportPath;
    brain::GIDSet _gids;
    SpikeReportReaderPtr _spikeReport;

    std::map<uint64_t, uint64_t> _gidMap;
};
using SpikeSimulationHandlerPtr = std::shared_ptr<SpikeSimulationHandler>;
} // namespace handler
} // namespace io
} // namespace circuitexplorer
