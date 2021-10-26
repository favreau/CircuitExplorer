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

#include <iostream>
#include <thread>

namespace circuitexplorer
{
#define PLUGIN_PREFIX "CE"

#define PLUGIN_ERROR(message)                                             \
    std::cerr << std::dec << "[" << std::this_thread::get_id() << "] E [" \
              << PLUGIN_PREFIX << "] " << message << std::endl;
#define PLUGIN_WARN(message)                                              \
    std::cerr << std::dec << "[" << std::this_thread::get_id() << "] W [" \
              << PLUGIN_PREFIX << "] " << message << std::endl;
#define PLUGIN_INFO(message)                                              \
    std::cout << std::dec << "[" << std::this_thread::get_id() << "] I [" \
              << PLUGIN_PREFIX << "] " << message << std::endl;
#ifdef NDEBUG
#define PLUGIN_DEBUG(message) ;
#else
#define PLUGIN_DEBUG(message)                                             \
    std::cout << std::dec << "[" << std::this_thread::get_id() << "] D [" \
              << PLUGIN_PREFIX << "] " << message << std::endl;
#endif
#define PLUGIN_TIMER(__time, __msg)                                       \
    std::cout << std::dec << "[" << std::this_thread::get_id() << "] T [" \
              << PLUGIN_PREFIX << "] " << __msg << " in " << __time       \
              << " seconds" << std::endl;

#define PLUGIN_THROW(message)              \
    {                                      \
        PLUGIN_ERROR(message);             \
        throw std::runtime_error(message); \
    }

#define PLUGIN_PROGRESS(message, progress, barWidth)                          \
    {                                                                         \
        std::cout << std::dec << "[" << std::this_thread::get_id() << "] I [" \
                  << PLUGIN_PREFIX << "] [";                                  \
        const float __maxValue = float(barWidth);                             \
        const float __step = 100.f / __maxValue;                              \
        const uint32_t __pos = progress / __maxValue * 50.f;                  \
        for (uint32_t __i = 0; __i < 50; ++__i)                               \
        {                                                                     \
            if (__i < __pos)                                                  \
                std::cout << "=";                                             \
            else if (__i == __pos)                                            \
                std::cout << ">";                                             \
            else                                                              \
                std::cout << " ";                                             \
        }                                                                     \
        std::cout << "] " << std::min(__pos * 2, uint32_t(100)) << "% "       \
                  << message << "\r";                                         \
        std::cout.flush();                                                    \
    }

} // namespace circuitexplorer
