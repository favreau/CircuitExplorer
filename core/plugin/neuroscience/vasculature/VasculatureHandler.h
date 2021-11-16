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

#include <brayns/api.h>
#include <brayns/common/simulation/AbstractSimulationHandler.h>
#include <brayns/common/types.h>

namespace circuitexplorer
{
namespace neuroscience
{
namespace vasculature
{
using namespace api;

/**
 * @brief The VasculatureHandler class handles the mapping of the vasculature
 * simulation to the geometry
 */
class VasculatureHandler : public brayns::AbstractSimulationHandler
{
public:
    /**
     * @brief Default constructor
     */
    VasculatureHandler(const AttachVasculatureHandler& details);
    VasculatureHandler(const VasculatureHandler& rhs);
    ~VasculatureHandler();

    void* getFrameData(const uint32_t) final;

    bool isReady() const final { return true; }

    brayns::AbstractSimulationHandlerPtr clone() const final;

private:
    std::vector<std::vector<double>> _simulationData;
};
using VasculatureHandlerPtr = std::shared_ptr<VasculatureHandler>;
} // namespace vasculature
} // namespace neuroscience
} // namespace circuitexplorer