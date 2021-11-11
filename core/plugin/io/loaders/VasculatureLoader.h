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

#pragma once

#include <plugin/api/CircuitExplorerParams.h>

#include <brayns/common/loader/Loader.h>
#include <brayns/common/types.h>
#include <brayns/parameters/GeometryParameters.h>

namespace circuitexplorer
{
namespace io
{
namespace loader
{
using namespace brayns;
using namespace api;

/**
 * Load vasculature from H5 file
 */
class VasculatureLoader : public Loader
{
public:
    VasculatureLoader(Scene& scene, PropertyMap&& loaderParams);

    std::string getName() const final;

    PropertyMap getProperties() const;

    static PropertyMap getCLIProperties();

    std::vector<std::string> getSupportedExtensions() const final;

    bool isSupported(const std::string& filename,
                     const std::string& extension) const final;

    ModelDescriptorPtr importFromBlob(
        Blob&& blob, const LoaderProgress& callback,
        const PropertyMap& properties) const final;

    ModelDescriptorPtr importFromFile(
        const std::string& filename, const LoaderProgress& callback,
        const PropertyMap& properties) const final;

private:
    size_t _addSDFGeometry(SDFMorphologyData& sdfMorphologyData,
                           const SDFGeometry& geometry,
                           const std::set<size_t>& neighbours,
                           const size_t materialId, const int section) const;

    void _addStepConeGeometry(const bool useSDFGeometry,
                              const Vector3f& position, const double radius,
                              const Vector3f& target,
                              const double previousRadius,
                              const size_t materialId,
                              const uint64_t& userDataOffset, Model& model,
                              SDFMorphologyData& sdfMorphologyData,
                              const uint32_t sdfGroupId,
                              const Vector3f& displacementParams) const;

    void _finalizeSDFGeometries(Model& model,
                                SDFMorphologyData& sdfMorphologyData) const;

    PropertyMap _defaults;
};
} // namespace loader
} // namespace io
} // namespace circuitexplorer
