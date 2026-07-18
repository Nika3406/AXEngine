#pragma once

#include <memory>
#include <string>
#include "assets/GltfAsset.h"

class GltfLoader {
public:
    std::shared_ptr<GltfAsset> loadFromFile(const std::string& path);

    const std::string& getLastError() const;
    const std::string& getLastWarning() const;

private:
    std::string error_;
    std::string warning_;
};
