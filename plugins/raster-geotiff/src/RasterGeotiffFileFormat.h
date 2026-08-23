// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pxr/pxr.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/usd/sdf/fileFormat.h"

PXR_NAMESPACE_OPEN_SCOPE

class UsdRasterGeoTiffFileFormat : public SdfFileFormat {
public:
    bool CanRead(const std::string& file) const override;
    bool Read(SdfLayer* layer, const std::string& resolvedPath,
              bool metadataOnly) const override;
    FileFormatArguments GetDefaultFileFormatArguments() const override;
    bool WriteToString(const SdfLayer& layer, std::string* str,
                       const std::string& comment = std::string()) const override;

protected:
    SDF_FILE_FORMAT_FACTORY_ACCESS;

    UsdRasterGeoTiffFileFormat();
    ~UsdRasterGeoTiffFileFormat() override;
};

PXR_NAMESPACE_CLOSE_SCOPE
