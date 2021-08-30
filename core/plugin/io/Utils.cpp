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
Vector3f get_translation(const Matrix4f& matrix)
{
    return Vector3f(glm::value_ptr(matrix)[12], glm::value_ptr(matrix)[13],
                    glm::value_ptr(matrix)[14]);
}

brain::Matrix4f glm_to_vmmlib(const Matrix4f& matrix)
{
    brain::Matrix4f tf;
    memcpy(&tf.array, glm::value_ptr(matrix), sizeof(tf.array));
    return tf;
}

Matrix4f vmmlib_to_glm(const brain::Matrix4f& matrix)
{
    Matrix4f tf;
    memcpy(glm::value_ptr(tf), &matrix.array, sizeof(matrix.array));
    return tf;
}

bool inBox(const Vector3f& point, const Boxf& box)
{
    const auto min = box.getMin();
    const auto max = box.getMax();
    return (point.x >= min.x && point.y >= min.y && point.z >= min.z &&
            point.x <= max.x && point.y <= max.y && point.z <= max.z);
}

Vector3f getPointInSphere()
{
    float d, x, y, z;
    do
    {
        x = (rand() % 1000 - 500) / 1000.f;
        y = (rand() % 1000 - 500) / 1000.f;
        z = (rand() % 1000 - 500) / 1000.f;
        d = sqrt(x * x + y * y + z * z);
    } while (d > 1.f);
    return Vector3f(x, y, z);
}

} // namespace circuitexplorer
