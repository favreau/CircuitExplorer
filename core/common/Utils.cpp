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

#include "Utils.h"

namespace circuitexplorer
{
brayns::Vector3f get_translation(const brayns::Matrix4f& matrix)
{
    return brayns::Vector3f(glm::value_ptr(matrix)[12],
                            glm::value_ptr(matrix)[13],
                            glm::value_ptr(matrix)[14]);
}

brain::Matrix4f glm_to_vmmlib(const brayns::Matrix4f& matrix)
{
    brain::Matrix4f result;
    memcpy(&result.array, glm::value_ptr(matrix), sizeof(result.array));
    return result;
}

brayns::Matrix4f vmmlib_to_glm(const brain::Matrix4f& matrix)
{
    brayns::Matrix4f result;
    memcpy(glm::value_ptr(result), &matrix.array, sizeof(matrix.array));
    return result;
}

bool inBox(const brayns::Vector3f& point, const brayns::Boxf& box)
{
    const auto min = box.getMin();
    const auto max = box.getMax();
    return (point.x >= min.x && point.y >= min.y && point.z >= min.z &&
            point.x <= max.x && point.y <= max.y && point.z <= max.z);
}

brayns::Vector3f getPointInSphere(const float innerRadius)
{
    const float radius =
        innerRadius + (rand() % 1000 / 1000.f) * (1.f - innerRadius);
    const float phi = M_PI * ((rand() % 2000 - 1000) / 1000.f);
    const float theta = M_PI * ((rand() % 2000 - 1000) / 1000.f);
    brayns::Vector3f v;
    v.x = radius * sin(phi) * cos(theta);
    v.y = radius * sin(phi) * sin(theta);
    v.z = radius * cos(phi);
    return v;
}

brayns::Vector3fs getPointsInSphere(const size_t nbPoints,
                                    const float innerRadius)
{
    const float radius =
        innerRadius + (rand() % 1000 / 1000.f) * (1.f - innerRadius);
    float phi = M_PI * ((rand() % 2000 - 1000) / 1000.f);
    float theta = M_PI * ((rand() % 2000 - 1000) / 1000.f);
    brayns::Vector3fs points;
    for (size_t i = 0; i < nbPoints; ++i)
    {
        brayns::Vector3f point = {radius * sin(phi) * cos(theta),
                                  radius * sin(phi) * sin(theta),
                                  radius * cos(phi)};
        points.push_back(point);
        phi += ((rand() % 1000) / 5000.f);
        theta += ((rand() % 1000) / 5000.f);
    }
    return points;
}

brayns::Vector3f transformVector3f(const brayns::Vector3f& v,
                                   const brayns::Matrix4f& transformation)
{
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(transformation, scale, rotation, translation, skew,
                   perspective);
    return translation + rotation * v;
}

float sphereVolume(const float radius)
{
    return 4.f * M_PI * pow(radius, 3) / 3.f;
}

float cylinderVolume(const float height, const float radius)
{
    return height * M_PI * radius * radius;
}

float coneVolume(const float height, const float r1, const float r2)
{
    return M_PI * (r1 * r1 + r1 * r2 + r2 * r2) * height / 3.f;
}

float capsuleVolume(const float height, const float radius)
{
    return sphereVolume(radius) + cylinderVolume(height, radius);
}
} // namespace circuitexplorer
