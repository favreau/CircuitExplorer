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

#include "camera/Camera.h"

namespace ospray
{
//! Implements a clipped perspective camera
struct OSPRAY_SDK_INTERFACE DOFPerspectiveCamera : public Camera
{
    /*! \brief constructor \internal also creates the ispc-side data structure
     */
    DOFPerspectiveCamera();

    //! \brief common function to help printf-debugging
    /*! Every derived class should override this! */
    virtual std::string toString() const
    {
        return "ospray::DOFPerspectiveCamera";
    }
    virtual void commit();

public:
    // ------------------------------------------------------------------
    // the parameters we 'parsed' from our parameters
    // ------------------------------------------------------------------
    float fovy;
    float aspect;
    float apertureRadius;
    float focusDistance;
    bool architectural; // orient image plane to be parallel to 'up' and shift
                        // the lens
    bool stereo;
    float interpupillaryDistance; // distance between the two cameras (stereo)

    // Clip planes
    bool enableClippingPlanes{false};
    Ref<Data> clipPlanes;
};

} // namespace ospray
