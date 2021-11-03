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

namespace circuitexplorer
{
#define PLUGIN_PREFIX "CE"

#define PLUGIN_ERROR(__msg) \
    std::cerr << "E [" << PLUGIN_PREFIX << "] " << __msg << std::endl;
#define PLUGIN_WARN(__msg) \
    std::cerr << "W [" << PLUGIN_PREFIX << "] " << __msg << std::endl;
#define PLUGIN_INFO(__msg) \
    std::cout << "I [" << PLUGIN_PREFIX << "] " << __msg << std::endl;
#ifdef NDEBUG
#define PLUGIN_DEBUG(__msg) ;
#else
#define PLUGIN_DEBUG(__msg) \
    std::cout << "D [" << PLUGIN_PREFIX << "] " << __msg << std::endl;
#endif
#define PLUGIN_TIMER(__time, __msg)                                          \
    std::cout << "T [" << PLUGIN_PREFIX << "] " << __msg << " in " << __time \
              << " seconds" << std::endl;

#define PLUGIN_THROW(__msg)              \
    {                                    \
        PLUGIN_ERROR(__msg);             \
        throw std::runtime_error(__msg); \
    }

#define PLUGIN_PROGRESS(__msg, __progress, __maxValue)                  \
    {                                                                   \
        std::cout << "I [" << PLUGIN_PREFIX << "] [";                   \
        const float __mv = float(__maxValue);                           \
        const float __step = 100.f / __mv;                              \
        const uint32_t __pos = __progress / __mv * 50.f;                \
        for (uint32_t __i = 0; __i < 50; ++__i)                         \
        {                                                               \
            if (__i < __pos)                                            \
                std::cout << "=";                                       \
            else if (__i == __pos)                                      \
                std::cout << ">";                                       \
            else                                                        \
                std::cout << " ";                                       \
        }                                                               \
        std::cout << "] " << std::min(__pos * 2, uint32_t(100)) << "% " \
                  << __msg << "\r";                                     \
        std::cout.flush();                                              \
    }

} // namespace circuitexplorer
