#pragma once

#include "PhotoRepository.h"

class TidyEngine {
public:
    explicit TidyEngine(PhotoRepository& repository);

    TidyReport rebuild();

private:
    PhotoRepository& repository_;
};
