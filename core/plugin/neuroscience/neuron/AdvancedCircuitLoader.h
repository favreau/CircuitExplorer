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

#pragma once

#include "AbstractCircuitLoader.h"

namespace circuitexplorer
{
namespace neuroscience
{
namespace neuron
{
using namespace common;

class AdvancedCircuitLoader : public AbstractCircuitLoader
{
public:
    AdvancedCircuitLoader(Scene &scene,
                          const ApplicationParameters &applicationParameters,
                          PropertyMap &&loaderParams);

    std::string getName() const final;

    static PropertyMap getCLIProperties();

    ModelDescriptorPtr importFromFile(
        const std::string &filename, const LoaderProgress &callback,
        const PropertyMap &properties) const final;
};
} // namespace neuron
} // namespace neuroscience
} // namespace circuitexplorer
