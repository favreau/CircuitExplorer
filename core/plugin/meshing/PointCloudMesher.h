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

#include <brayns/common/types.h>
#include <map>

namespace circuitexplorer
{
namespace meshing
{
using namespace brayns;

typedef std::map<size_t, std::vector<Vector4f>> PointCloud;

class PointCloudMesher
{
public:
    PointCloudMesher();

    bool toConvexHull(Model& model, const PointCloud& pointCloud);

    bool toMetaballs(Model& model, const PointCloud& pointCloud,
                     const size_t gridSize, const float threshold);
};
} // namespace meshing
} // namespace circuitexplorer
