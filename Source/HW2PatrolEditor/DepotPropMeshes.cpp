#include "DepotPropMeshes.h"

#include "Engine/StaticMesh.h"
#include "MeshDescriptionBuilder.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"

namespace
{
    class FPropMesh
    {
    public:
        FPropMesh()
        {
            FStaticMeshAttributes(Description).Register();
            Builder.SetMeshDescription(&Description);
            for (int32 Index = 0; Index < 3; ++Index)
            {
                Groups.Add(Builder.AppendPolygonGroup(SlotName(Index)));
            }
        }

        void SetSurface(int32 Index) { SurfaceIndex = Index; }

        void Quad(FVector A, FVector B, FVector C, FVector D, FVector Normal, FLinearColor Color)
        {
            if (FVector::DotProduct(FVector::CrossProduct(C - A, B - A), Normal) < 0)
            {
                Swap(B, D);
            }
            const FVector Tangent = (B - A).GetSafeNormal();
            const auto Vertex = [&](FVector Position, FVector2D UV)
            {
                const FVertexInstanceID Id = Builder.AppendInstance(Builder.AppendVertex(Position));
                Builder.SetInstanceTangentSpace(Id, Normal, Tangent, 1.0f);
                Builder.SetInstanceColor(Id, FVector4f(Color.R, Color.G, Color.B, 1.0f));
                Builder.SetInstanceUV(Id, UV);
                return Id;
            };
            const FVertexInstanceID V0 = Vertex(A, FVector2D(0, 0));
            const FVertexInstanceID V1 = Vertex(B, FVector2D(1, 0));
            const FVertexInstanceID V2 = Vertex(C, FVector2D(1, 1));
            const FVertexInstanceID V3 = Vertex(D, FVector2D(0, 1));
            Builder.AppendTriangle(V0, V1, V2, Groups[SurfaceIndex]);
            Builder.AppendTriangle(V0, V2, V3, Groups[SurfaceIndex]);
        }

        void Box(FVector Center, FVector Size, FLinearColor Color, float Yaw = 0)
        {
            const FRotator Rotation(0, Yaw, 0);
            const auto Point = [&](double X, double Y, double Z)
            {
                return Center + Rotation.RotateVector(FVector(X, Y, Z) * Size * 0.5);
            };
            const FVector A = Point(-1, -1, -1);
            const FVector B = Point(1, -1, -1);
            const FVector C = Point(1, 1, -1);
            const FVector D = Point(-1, 1, -1);
            const FVector E = Point(-1, -1, 1);
            const FVector F = Point(1, -1, 1);
            const FVector G = Point(1, 1, 1);
            const FVector H = Point(-1, 1, 1);
            Quad(A, B, C, D, -FVector::UpVector, Color);
            Quad(E, F, G, H, FVector::UpVector, Color);
            Quad(A, B, F, E, -Rotation.RotateVector(FVector::RightVector), Color);
            Quad(D, C, G, H, Rotation.RotateVector(FVector::RightVector), Color);
            Quad(A, D, H, E, -Rotation.Vector(), Color);
            Quad(B, C, G, F, Rotation.Vector(), Color);
        }

        void DrumWall(double Bottom, double Top, double BottomRadius, double TopRadius, FLinearColor Color)
        {
            constexpr int32 Sides = 24;
            for (int32 Side = 0; Side < Sides; ++Side)
            {
                const double AngleA = 2 * UE_PI * Side / Sides;
                const double AngleB = 2 * UE_PI * (Side + 1) / Sides;
                const FVector A(FMath::Cos(AngleA), FMath::Sin(AngleA), 0);
                const FVector B(FMath::Cos(AngleB), FMath::Sin(AngleB), 0);
                const FVector Normal = ((A + B).GetSafeNormal2D()
                    + FVector(0, 0, (BottomRadius - TopRadius) / (Top - Bottom))).GetSafeNormal();

                Quad(A * BottomRadius + FVector(0, 0, Bottom), B * BottomRadius + FVector(0, 0, Bottom),
                    B * TopRadius + FVector(0, 0, Top), A * TopRadius + FVector(0, 0, Top), Normal, Color);
            }
        }

        void DrumCap(double Height, double Radius, FLinearColor Color, bool bTop)
        {
            constexpr int32 Sides = 24;
            const FVector Center(0, 0, Height);
            const FVector Normal = bTop ? FVector::UpVector : -FVector::UpVector;
            for (int32 Side = 0; Side < Sides; ++Side)
            {
                const double AngleA = 2 * UE_PI * Side / Sides;
                const double AngleB = 2 * UE_PI * (Side + 1) / Sides;
                const FVector A = Center + FVector(FMath::Cos(AngleA), FMath::Sin(AngleA), 0) * Radius;
                const FVector B = Center + FVector(FMath::Cos(AngleB), FMath::Sin(AngleB), 0) * Radius;
                Quad(Center, A, (A + B) * 0.5, B, Normal, Color);
            }
        }

        void Build(UStaticMesh& Mesh, TConstArrayView<UMaterialInterface*> Materials, bool bCollidable = true)
        {
            Mesh.GetStaticMaterials().Reset();
            check(Materials.Num() == Groups.Num());
            for (int32 Index = 0; Index < Materials.Num(); ++Index)
            {
                const FName Name = SlotName(Index);
                Mesh.GetStaticMaterials().Emplace(Materials[Index], Name, Name);
            }
            Mesh.bDoFastBuild = false;
            Mesh.CreateBodySetup();
            UBodySetup* Body = Mesh.GetBodySetup();
            Body->RemoveSimpleCollision();
            Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;

            if (bCollidable)
            {
                const FBox Bounds = Builder.ComputeBoundingBox();
                FKBoxElem Shape;
                Shape.Center = Bounds.GetCenter();
                Shape.X = Bounds.GetSize().X;
                Shape.Y = Bounds.GetSize().Y;
                Shape.Z = Bounds.GetSize().Z;
                Body->AggGeom.BoxElems.Add(Shape);
            }
            Body->InvalidatePhysicsData();

            Mesh.SetNumSourceModels(1);
            FMeshBuildSettings& Settings = Mesh.GetSourceModel(0).BuildSettings;
            Settings.bRecomputeNormals = false;
            Settings.bRecomputeTangents = false;
            Settings.bGenerateLightmapUVs = false;
            UStaticMesh::FBuildMeshDescriptionsParams Params;
            Params.bUseHashAsGuid = true;
            verify(Mesh.BuildFromMeshDescriptions({ &Description }, Params));
        }

    private:
        FMeshDescription Description;
        FMeshDescriptionBuilder Builder;
        static FName SlotName(int32 Index)
        {
            return FName(FString::Printf(TEXT("Finish_%d"), Index));
        }

        TArray<FPolygonGroupID> Groups;
        int32 SurfaceIndex = 0;
    };
}

void DepotPropMeshes::BuildCrate(UStaticMesh& Mesh, TConstArrayView<UMaterialInterface*> Materials)
{
    FPropMesh Prop;
    const FLinearColor Oak(0.34f, 0.15f, 0.052f);
    const FLinearColor Frame(0.17f, 0.073f, 0.029f);
    const FLinearColor Steel(0.085f, 0.10f, 0.105f);
    Prop.SetSurface(1);
    Prop.Box(FVector(0, 0, 49), FVector(88, 88, 90), Frame);

    Prop.SetSurface(0);
    const FLinearColor Boards[] = {
        FLinearColor(0.28f, 0.116f, 0.038f), Oak, FLinearColor(0.40f, 0.181f, 0.063f),
        FLinearColor(0.30f, 0.128f, 0.042f), FLinearColor(0.37f, 0.164f, 0.058f)
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Boards); ++Index)
    {
        const double Z = 15 + Index * 17;
        const FLinearColor Color = Boards[Index];
        Prop.Box(FVector(0, -45, Z), FVector(89, 3, 16), Color);
        Prop.Box(FVector(0, 45, Z), FVector(89, 3, 16), Color);
        Prop.Box(FVector(-45, 0, Z), FVector(3, 89, 16), Color);
        Prop.Box(FVector(45, 0, Z), FVector(3, 89, 16), Color);
        Prop.Box(FVector(-34 + Index * 17, 0, 94), FVector(16, 88, 4), Color);
    }

    Prop.SetSurface(1);
    for (double Z : { 4.0, 95.0 })
    {
        for (double Sign : { -1.0, 1.0 })
        {
            Prop.Box(FVector(0, Sign * 45, Z), FVector(100, 10, 8), Frame);
            Prop.Box(FVector(Sign * 45, 0, Z), FVector(10, 80, 8), Frame);
        }
    }

    for (double X : { -44.0, 44.0 })
    {
        for (double Y : { -44.0, 44.0 })
        {
            Prop.SetSurface(1);
            Prop.Box(FVector(X, Y, 49), FVector(9, 9, 84), Frame);
            Prop.SetSurface(2);
            Prop.Box(FVector(X, Y, 11), FVector(11, 11, 11), Steel);
            Prop.Box(FVector(X, Y, 87), FVector(11, 11, 11), Steel);
        }
    }
    Prop.Build(Mesh, Materials);
}

void DepotPropMeshes::BuildDrum(UStaticMesh& Mesh, TConstArrayView<UMaterialInterface*> Materials)
{
    FPropMesh Prop;
    const FLinearColor Paint(0.043f, 0.255f, 0.24f);
    const FLinearColor Edge(0.079f, 0.12f, 0.125f);
    const FLinearColor Rib(0.075f, 0.34f, 0.31f);
    const FLinearColor Band(0.72f, 0.31f, 0.048f);

    Prop.SetSurface(1);
    Prop.DrumWall(0, 5, 41, 45, Edge);
    Prop.SetSurface(0);
    Prop.DrumWall(5, 12, 45, 43, Paint);
    Prop.DrumWall(12, 90, 43, 43, Paint);
    Prop.DrumWall(90, 99, 43, 45, Paint);
    Prop.SetSurface(1);
    Prop.DrumWall(99, 104, 45, 45, Edge);
    Prop.DrumWall(101, 104, 39, 39, Edge);
    Prop.DrumCap(0, 41, Edge, false);
    Prop.DrumCap(101, 44.9, Edge, true);
    Prop.SetSurface(0);
    for (double Z : { 24.0, 78.0 })
    {
        Prop.DrumWall(Z - 2, Z, 43, 46, Rib);
        Prop.DrumWall(Z, Z + 4, 46, 46, Rib);
        Prop.DrumWall(Z + 4, Z + 6, 46, 43, Rib);
    }
    Prop.SetSurface(2);
    Prop.DrumWall(49, 55, 43.15, 43.15, Band);
    Prop.Box(FVector(15, 0, 103), FVector(12, 12, 4), Band, 30);
    Prop.Box(FVector(-19, 0, 102.5), FVector(7, 7, 3), Edge, 30);
    Prop.Build(Mesh, Materials);
}

void DepotPropMeshes::BuildWeeds(UStaticMesh& Mesh, TConstArrayView<UMaterialInterface*> Materials)
{
    FPropMesh Prop;
    FRandomStream Seed(8729);
    for (int32 Blade = 0; Blade < 20; ++Blade)
    {
        const double Angle = Seed.FRandRange(0, 2 * UE_PI);
        const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0);
        const FVector Side(-Direction.Y, Direction.X, 0);
        const FVector Base = Direction * Seed.FRandRange(1.5f, 12.0f);
        const double Height = Seed.FRandRange(13.0f, 31.0f);
        const double Bend = Seed.FRandRange(4.0f, 14.0f);
        const double HalfWidth = Seed.FRandRange(0.60f, 1.50f);
        const FVector Knee = Base + Direction * (Bend * 0.28) + FVector(0, 0, Height * 0.55);
        const FVector Tip = Base + Direction * Bend + FVector(0, 0, Height);
        const FVector Normal = FVector::CrossProduct(Tip - Base, Side).GetSafeNormal();
        Prop.SetSurface(Blade % 3);
        Prop.Quad(Base - Side * HalfWidth, Base + Side * HalfWidth,
            Knee + Side * (HalfWidth * 0.66), Knee - Side * (HalfWidth * 0.66),
            Normal, FLinearColor::White);
        Prop.Quad(Knee - Side * (HalfWidth * 0.66), Knee + Side * (HalfWidth * 0.66),
            Tip + Side * 0.07, Tip - Side * 0.07, Normal, FLinearColor::White);
    }
    Prop.Build(Mesh, Materials, false);
}
