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

#include <brayns/common/types.h>

namespace circuitexplorer
{
using namespace brayns;

Vector3f get_translation(const Matrix4f& matrix);
brain::Matrix4f glm_to_vmmlib(const Matrix4f& matrix);
Matrix4f vmmlib_to_glm(const brain::Matrix4f& matrix);
bool inBox(const Vector3f& point, const Boxf& box);
Vector3f getPointInSphere();
} // namespace circuitexplorer
