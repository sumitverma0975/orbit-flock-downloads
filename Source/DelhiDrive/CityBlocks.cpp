#include "CityBlocks.h"
#include "Components/SceneComponent.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInterface.h"

namespace
{
struct FBlockSurface
{
    TArray<FVector> Positions, Normals;
    TArray<int32> Indices;
    TArray<FProcMeshTangent> Tangents;
    TArray<FVector2D> UV;

    void AddQuad(const FVector& A, const FVector& B, double Base, double Top, const FVector& Normal)
    {
        const int32 K = Positions.Num();
        const double Span = FMath::Max<int32>(1, FMath::RoundToInt(FVector::Distance(A,B) / 450.)) * .5;
        const double Stories = FMath::Max<int32>(1, FMath::RoundToInt((Top-Base) / 310.)) * .5;
        Positions.Append({A,B,B,A});
        Positions[K].Z = Positions[K+1].Z = Base;
        Positions[K+2].Z = Positions[K+3].Z = Top;
        for (int32 I=0; I<4; ++I) { Normals.Add(Normal); Tangents.Emplace((B-A).GetSafeNormal(),false); }
        UV.Append({{0.,Stories},{Span,Stories},{Span,0.},{0.,0.}});
        Indices.Append({K,K+2,K+1,K,K+3,K+2});
    }

    void Publish(UProceduralMeshComponent* Mesh, int32 Section, UMaterialInterface* Material, bool Solid)
    {
        if (Indices.IsEmpty()) return;
        Mesh->CreateMeshSection(Section, Positions, Indices, Normals, UV, TArray<FColor>(), Tangents, Solid);
        if (Material) Mesh->SetMaterial(Section,Material);
    }
};
struct FBlockTile { FBlockSurface Walls, Roofs; };

void AddBlock(FBlockTile& Tile,const TArray<FVector>& Ring,const TArray<int32>& Triangles,double H)
{
    const int32 First = Tile.Roofs.Positions.Num();
    for (FVector P : Ring)
    {
        P.Z = H;
        Tile.Roofs.Positions.Add(P);
        Tile.Roofs.Normals.Add(FVector::UpVector);
        Tile.Roofs.Tangents.Emplace(FVector::ForwardVector,false);
        Tile.Roofs.UV.Add({P.X/300.,P.Y/300.});
    }
    for (int32 Idx : Triangles) Tile.Roofs.Indices.Add(First+Idx);
    for (int32 I=0; I<Ring.Num(); ++I)
    {
        const FVector A = Ring[I], B = Ring[(I+1)%Ring.Num()];
        const FVector Dir = (B-A).GetSafeNormal();
        const FVector Out(Dir.Y,-Dir.X,0.);
        Tile.Walls.AddQuad(A,B,0.,H,Out);

    }
}
}

ACityBlocks::ACityBlocks()
{
    PrimaryActorTick.bCanEverTick=false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("District")));
    RootComponent->SetMobility(EComponentMobility::Static);
}

bool ACityBlocks::InitCity(const FString& RoadFile)
{
    const FString Name = FPaths::GetBaseFilename(RoadFile)+TEXT("_buildings.json");
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir()/TEXT("Data")/Name))) return false;
    TSharedPtr<FJsonObject> Data;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data) || !Data.IsValid()) return false;
    const TArray<TSharedPtr<FJsonValue>>* Objects=nullptr;
    if (!Data->TryGetArrayField(TEXT("buildings"),Objects)) return false;
    TMap<FIntPoint,FBlockTile> Tiles;
    for (const auto& V : *Objects)
    {
        const TSharedPtr<FJsonObject> Obj=V->AsObject();
        if (!Obj.IsValid()) continue;
        const auto& Points=Obj->GetArrayField(TEXT("p"));
        const auto& Indices=Obj->GetArrayField(TEXT("roof"));
        TArray<FVector> Ring;
        for (const auto& Pt : Points)
        {
            const auto& XY=Pt->AsArray();
            if (XY.Num()==2) Ring.Emplace(XY[0]->AsNumber(),XY[1]->AsNumber(),0.);
        }
        if (Ring.Num()<3) continue;
        TArray<int32> Roof;
        for (const auto& Id : Indices) Roof.Add(static_cast<int32>(Id->AsNumber()));
        if (Roof.Num() != 3*(Ring.Num()-2) || Roof.ContainsByPredicate([&Ring](int32 Id){return !Ring.IsValidIndex(Id);})) continue;
        const FIntPoint Key(FMath::FloorToInt(Ring[0].X/20000.),FMath::FloorToInt(Ring[0].Y/20000.));
        AddBlock(Tiles.FindOrAdd(Key),Ring,Roof,Obj->GetNumberField(TEXT("height_cm")));
    }
    UMaterialInterface* Wall=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/M_Facade.M_Facade"));
    UMaterialInterface* Roof=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/M_Roof.M_Roof"));
    for (auto& Pair : Tiles)
    {
        UProceduralMeshComponent* Mesh=NewObject<UProceduralMeshComponent>(this);
        AddInstanceComponent(Mesh);
        Mesh->SetMobility(EComponentMobility::Static);
        Mesh->SetupAttachment(RootComponent);
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->bUseComplexAsSimpleCollision=true;
        Mesh->SetCullDistance(130000.f);
        Mesh->RegisterComponent();
        Pair.Value.Walls.Publish(Mesh,0,Wall,true);
        Pair.Value.Roofs.Publish(Mesh,1,Roof,true);

    }
    return !Tiles.IsEmpty();
}
