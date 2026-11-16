#pragma once

#include "CoreMinimal.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// Tiny helpers so gameplay actors work with zero imported art (engine primitives only).
// Replace these with real meshes/materials later.
namespace DriveShapes
{
	// Call the first time from inside an actor constructor (all our actors do).
	inline UStaticMesh* Cube()     { static ConstructorHelpers::FObjectFinder<UStaticMesh> F(TEXT("/Engine/BasicShapes/Cube.Cube"));         return F.Object; }
	inline UStaticMesh* Cylinder() { static ConstructorHelpers::FObjectFinder<UStaticMesh> F(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); return F.Object; }
	inline UStaticMesh* Sphere()   { static ConstructorHelpers::FObjectFinder<UStaticMesh> F(TEXT("/Engine/BasicShapes/Sphere.Sphere"));     return F.Object; }

	// Colours a basic-shape component (BasicShapeMaterial exposes a "Color" parameter).
	inline void Tint(UStaticMeshComponent* C, const FLinearColor& Col)
	{
		if (!C) return;
		if (UMaterialInstanceDynamic* M = C->CreateAndSetMaterialInstanceDynamic(0))
		{
			M->SetVectorParameterValue(TEXT("Color"), Col);
		}
	}
}
