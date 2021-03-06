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

#include <brayns/common/loader/Loader.h>
#include <brayns/common/types.h>

namespace circuitexplorer
{
namespace neuroscience
{
namespace astrocyte
{
using namespace brayns;

class AstrocyteLoader : public Loader
{
public:
    AstrocyteLoader(Scene &scene,
                    const ApplicationParameters &applicationParameters,
                    PropertyMap &&loaderParams);

    std::string getName() const final;

    std::vector<std::string> getSupportedExtensions() const final;

    bool isSupported(const std::string &filename,
                     const std::string &extension) const final;

    static PropertyMap getCLIProperties();

    /** @copydoc Loader::importFromBlob */
    ModelDescriptorPtr importFromBlob(
        Blob &&blob, const LoaderProgress &callback,
        const PropertyMap &properties) const final;

    /** @copydoc Loader::importFromFile */
    ModelDescriptorPtr importFromFile(
        const std::string &filename, const LoaderProgress &callback,
        const PropertyMap &properties) const final;

private:
    void _importMorphologiesFromURIs(const PropertyMap &properties,
                                     const std::vector<std::string> &uris,
                                     const LoaderProgress &callback,
                                     Model &model) const;
    const ApplicationParameters &_applicationParameters;
    PropertyMap _defaults;
    PropertyMap _fixedDefaults;
};
} // namespace astrocyte
} // namespace neuroscience
} // namespace circuitexplorer
