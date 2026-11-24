#pragma once

#include "Containers/ArrayView.h"

class UStaticMesh;
class UMaterialInterface;

namespace DepotPropMeshes
{
    void BuildCrate(UStaticMesh& Mesh, TConstArrayView<UMaterialInterface*> Materials);
    void BuildDrum(UStaticMesh& Mesh, TConstArrayView<UMaterialInterface*> Materials);
    void BuildWeeds(UStaticMesh& Mesh, TConstArrayView<UMaterialInterface*> Materials);
}
