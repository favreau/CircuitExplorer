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

#include "VoltageSimulationHandler.h"

#include <common/Logs.h>

#include <brayns/parameters/AnimationParameters.h>

namespace circuitexplorer
{
namespace neuroscience
{
namespace neuron
{
VoltageSimulationHandler::VoltageSimulationHandler(
    const std::string& reportPath, const brion::GIDSet& gids,
    const bool synchronousMode)
    : AbstractSimulationHandler()
    , _synchronousMode(synchronousMode)
    , _reportPath(reportPath)
    , _compartmentReport(new brion::CompartmentReport(brion::URI(reportPath),
                                                      brion::MODE_READ, gids))
{
    // Load simulation information from compartment reports
    _dt = _compartmentReport->getTimestep();
    const auto startTime = _compartmentReport->getStartTime();
    const auto endTime = _compartmentReport->getEndTime();
    _startFrame = startTime / _dt;
    _nbFrames = (endTime - startTime) / _dt;
    _unit = _compartmentReport->getTimeUnit();
    _frameSize = _compartmentReport->getFrameSize();

    PLUGIN_INFO("-----------------------------------------------------------");
    PLUGIN_INFO("Voltage simulation information");
    PLUGIN_INFO("----------------------");
    PLUGIN_INFO("Start time           : " << startTime);
    PLUGIN_INFO("End time             : " << endTime);
    PLUGIN_INFO("Steps between frames : " << _dt);
    PLUGIN_INFO("Number of frames     : " << _nbFrames);
    PLUGIN_INFO("Frame size           : " << _frameSize);
    PLUGIN_INFO("-----------------------------------------------------------");
}

VoltageSimulationHandler::VoltageSimulationHandler(
    const VoltageSimulationHandler& rhs)
    : AbstractSimulationHandler(rhs)
    , _synchronousMode(rhs._synchronousMode)
    , _compartmentReport(rhs._compartmentReport)
    , _ready(false)
{
}

VoltageSimulationHandler::~VoltageSimulationHandler() {}

AbstractSimulationHandlerPtr VoltageSimulationHandler::clone() const
{
    return std::make_shared<VoltageSimulationHandler>(*this);
}

bool VoltageSimulationHandler::isReady() const
{
    return _ready;
}

void* VoltageSimulationHandler::getFrameData(const uint32_t frame)
{
    const auto boundedFrame = _startFrame + _getBoundedFrame(frame);

    if (!_currentFrameFuture.valid() && _currentFrame != boundedFrame)
        _triggerLoading(boundedFrame);

    if (!_makeFrameReady(boundedFrame))
        return nullptr;

    return _frameData.data();
}

void VoltageSimulationHandler::_triggerLoading(const uint32_t frame)
{
    float timestamp = frame * _dt;
    timestamp = std::min(static_cast<float>(_nbFrames), timestamp);

    if (_currentFrameFuture.valid())
        _currentFrameFuture.wait();

    _ready = false;
    _currentFrameFuture = _compartmentReport->loadFrame(timestamp);
}

bool VoltageSimulationHandler::_isFrameLoaded() const
{
    if (!_currentFrameFuture.valid())
        return false;

    if (_synchronousMode)
    {
        _currentFrameFuture.wait();
        return true;
    }

    return _currentFrameFuture.wait_for(std::chrono::milliseconds(0)) ==
           std::future_status::ready;
}

bool VoltageSimulationHandler::_makeFrameReady(const uint32_t frame)
{
    if (_isFrameLoaded())
    {
        try
        {
            _frameData = std::move(*_currentFrameFuture.get().data);
        }
        catch (const std::exception& e)
        {
            PLUGIN_ERROR("Error loading simulation frame " << frame << ": "
                                                           << e.what());
            return false;
        }
        _currentFrame = frame;
        _ready = true;
    }
    return true;
}
} // namespace neuron
} // namespace neuroscience
} // namespace circuitexplorer
