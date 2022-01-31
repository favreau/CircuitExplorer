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

#include <common/Logs.h>

#include "SpikeSimulationHandler.h"
#include <brayns/parameters/AnimationParameters.h>

namespace circuitexplorer
{
namespace neuroscience
{
namespace neuron
{
const float DEFAULT_REST_VALUE = -80.f;
const float DEFAULT_SPIKING_VALUE = -1.f;
const float DEFAULT_TIME_INTERVAL = 0.01f;
const float DEFAULT_DECAY_SPEED = 1.f;

SpikeSimulationHandler::SpikeSimulationHandler(const std::string& reportPath,
                                               const brion::GIDSet& gids)
    : AbstractSimulationHandler()
    , _reportPath(reportPath)
    , _gids(gids)
    , _spikeReport(new brain::SpikeReportReader(brain::URI(reportPath), gids))
{
    uint64_t c{0};
    for (const auto gid : _gids)
    {
        _gidMap[gid] = c;
        ++c;
    }

    // Load simulation information from compartment reports
    _nbFrames = _spikeReport->getEndTime() / DEFAULT_TIME_INTERVAL;
    _dt = DEFAULT_TIME_INTERVAL;
    _frameSize = _gids.size();
    _frameData.resize(_frameSize, DEFAULT_REST_VALUE);

    PLUGIN_INFO("-----------------------------------------------------------");
    PLUGIN_INFO("Spike simulation information");
    PLUGIN_INFO("----------------------");
    PLUGIN_INFO("Report path           : " << _reportPath);
    PLUGIN_INFO("Frame size (# of GIDs): " << _frameSize);
    PLUGIN_INFO("End time              : " << _spikeReport->getEndTime());
    PLUGIN_INFO("Time interval         : " << DEFAULT_TIME_INTERVAL);
    PLUGIN_INFO("Decay speed           : " << DEFAULT_DECAY_SPEED);
    PLUGIN_INFO("Number of frames      : " << _nbFrames);
    PLUGIN_INFO("-----------------------------------------------------------");
}

SpikeSimulationHandler::SpikeSimulationHandler(
    const SpikeSimulationHandler& rhs)
    : AbstractSimulationHandler(rhs)
    , _reportPath(rhs._reportPath)
    , _gids(rhs._gids)
    , _spikeReport(rhs._spikeReport)
    , _gidMap(rhs._gidMap)
{
}

void* SpikeSimulationHandler::getFrameData(const uint32_t frame)
{
    const auto boundedFrame = _getBoundedFrame(frame);
    if (_currentFrame != boundedFrame)
    {
        for (size_t i = 0; i < _frameSize; ++i)
        {
            if (_frameData[i] > DEFAULT_REST_VALUE)
                _frameData[i] -= DEFAULT_DECAY_SPEED;
            else
                _frameData[i] = DEFAULT_REST_VALUE;
        }

        const float ts = boundedFrame * _dt;
        const float endTime = _spikeReport->getEndTime() - _dt;
        const auto& spikes =
            _spikeReport->getSpikes(std::min(ts, endTime),
                                    std::min(ts + 1.f, endTime));

        for (const auto spike : spikes)
            _frameData[_gidMap[spike.second]] = DEFAULT_SPIKING_VALUE;

        _currentFrame = boundedFrame;
    }

    return _frameData.data();
}

AbstractSimulationHandlerPtr SpikeSimulationHandler::clone() const
{
    return std::make_shared<SpikeSimulationHandler>(*this);
}
} // namespace neuron
} // namespace neuroscience
} // namespace circuitexplorer
