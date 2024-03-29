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

#include "utils.ih"

#include "math/sampling.ih"
#include "ospray/SDK/camera/Camera.ih"

inline float getRandomValue()
{
    float r;
    int nbMaxTries = 10;
    while (nbMaxTries >= 0 && rdrand(&r) == false)
        --nbMaxTries;
    return r;
}

struct DOFPerspectiveCamera
{
    Camera super;

    vec3f org;    //!< position of camera, already contains shift when
                  //! STEREO_{LEFT|RIGHT}
    vec3f dir_00; //!< direction of ray with screenSample=(0,0); scaled to
                  //! focusDistance
    vec3f dir_du; //!< delta of ray direction between two pixels in X; scaled to
                  //! focusDistance
    vec3f dir_dv; //!< delta of ray direction between two pixels in Y; scaled to
                  //! focusDistance
    float scaledAperture; //!< radius of aperture, divided by horizontal image
                          //! plane size
    float aspect;         //!< image plane size x / y
    vec3f ipd_offset;     //!< shift of camera position for left/right eye (only
                          //! when stereo)

    // Clip planes
    const uniform vec4f* clipPlanes;
    unsigned int numClipPlanes;
};

void DOFPerspectiveCamera_initRay(uniform Camera* uniform _self,
                                  varying Ray& ray,
                                  const varying CameraSample& sample)
{
    uniform DOFPerspectiveCamera* uniform self =
        (uniform DOFPerspectiveCamera * uniform) _self;

    vec2f screen = sample.screen;
    vec3f org = self->org;

    screen = Camera_subRegion(_self, screen);

    vec3f dir =
        self->dir_00 + screen.x * self->dir_du + screen.y * self->dir_dv;

    if (self->super.doesDOF)
    {
        float random = 1.f;
        random = getRandomValue();
        random = (random == 0.f ? 0.001f : random);
        const vec3f llp =
            uniformSampleDisk(random * self->scaledAperture, sample.lens);
        // transform local lens point to focal plane (dir_XX are prescaled in
        // this case)
        const vec3f lp =
            (llp.x * self->dir_du) + ((llp.y * self->aspect) * self->dir_dv);
        org = org + lp;
        dir = dir - lp;
    }

    float nearClip = self->super.nearClip;
    float farClip = inf;
    const varying vec3f direction = normalize(dir);

    clipRay(self->clipPlanes, self->numClipPlanes, org, direction, nearClip,
            farClip);

    const float time = Camera_shutterTime(_self, sample.time);
    setRay(ray, org, direction, nearClip, farClip, time);
}

/*! create a new ispc-side version of a DOFPerspectiveCamera - with given
    pointer to the c-class version - and return both class pointer and
    pointer to internal data back via the supplied reference
    parameters */
export void* uniform DOFPerspectiveCamera_create(void* uniform cppE)
{
    uniform DOFPerspectiveCamera* uniform self =
        uniform new uniform DOFPerspectiveCamera;
    self->super.cppEquivalent = cppE;
    self->super.initRay = DOFPerspectiveCamera_initRay;
    self->super.doesDOF = false;
    self->numClipPlanes = 0;
    return self;
}

export void DOFPerspectiveCamera_set(
    void* uniform _self, const uniform vec3f& org, const uniform vec3f& dir_00,
    const uniform vec3f& dir_du, const uniform vec3f& dir_dv,
    const uniform float scaledAperture, const uniform float aspect,
    const uniform vec3f& ipd_offset, const uniform vec4f clipPlanes[],
    const uniform unsigned int numClipPlanes)
{
    uniform DOFPerspectiveCamera* uniform self =
        (uniform DOFPerspectiveCamera * uniform) _self;
    self->org = org;
    self->dir_00 = dir_00;
    self->dir_du = dir_du;
    self->dir_dv = dir_dv;
    self->scaledAperture = scaledAperture;
    self->super.doesDOF = scaledAperture > 0.f;
    self->aspect = aspect;
    self->ipd_offset = ipd_offset;
    self->clipPlanes = clipPlanes;
    self->numClipPlanes = numClipPlanes;
}
