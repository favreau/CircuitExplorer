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

#include "Types.h"

#include <brayns/common/mathTypes.h>
#include <brayns/common/types.h>

#include <brain/brain.h>

#include <glm/gtx/matrix_decompose.hpp>

namespace circuitexplorer
{
// Convertors
brayns::Vector3f get_translation(const brayns::Matrix4f& matrix);
brain::Matrix4f glm_to_vmmlib(const brayns::Matrix4f& matrix);
brayns::Matrix4f vmmlib_to_glm(const brain::Matrix4f& matrix);
brayns::Vector3f transformVector3f(const brayns::Vector3f& v,
                                   const brayns::Matrix4f& transformation);

// Containers
bool inBox(const brayns::Vector3f& point, const brayns::Boxf& box);
brayns::Vector3f getPointInSphere(const float innerRadius);

// Volumes
float sphereVolume(const float radius);
float cylinderVolume(const float height, const float radius);
float coneVolume(const float height, const float r1, const float r2);
float capsuleVolume(const float height, const float radius);

} // namespace circuitexplorer
