#include "BuildPatrolLevelCommandlet.h"

#include "ActorFactories/ActorFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "EdGraphSchema_BehaviorTree.h"
#include "Builders/CubeBuilder.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "EngineUtils.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EdGraph/EdGraphSchema.h"
#include "DepotPropMeshes.h"
#include "Editor.h"
#include "Elements/PCGCreatePointsGrid.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGTransformPoints.h"
#include "Engine/DirectionalLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "Modules/ModuleManager.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "Patrol/PatrolGuard.h"
#include "Patrol/PatrolNodes.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGVolume.h"
#include "UObject/SavePackage.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, HW2PatrolEditor)

namespace
{
    constexpr TCHAR Folder[] = TEXT("/Game/PatrolDepot/");
    constexpr TCHAR FloorMaterial[] = TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark");
    constexpr TCHAR WallMaterial[] = TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_02");

    template<typename T>
    T* MakeAsset(const TCHAR* Name)
    {
        const FString PackageName = FString(Folder) + Name;
        if (FPackageName::DoesPackageExist(PackageName))
        {
            return LoadObject<T>(nullptr, *(PackageName + TEXT(".") + Name));
        }

        UPackage* Package = CreatePackage(*PackageName);
        return NewObject<T>(Package, Name, RF_Public | RF_Standalone);
    }

    bool SaveAsset(UObject* Asset)
    {
        FAssetRegistryModule::AssetCreated(Asset);
        UPackage* Package = Asset->GetOutermost();
        const FString Filename = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        return UPackage::SavePackage(Package, Asset, *Filename, Args);
    }

    void AddKey(UBlackboardData* Board, const TCHAR* Name, UBlackboardKeyType* Type)
    {
        FBlackboardEntry& Entry = Board->Keys.AddDefaulted_GetRef();
        Entry.EntryName = Name;
        Entry.KeyType = Type;
    }

    void ConfigureKey(UBTNode* Node, const TCHAR* Name, UBlackboardData* Board)
    {
        const FStructProperty* Property = FindFProperty<FStructProperty>(Node->GetClass(), TEXT("BlackboardKey"));
        FBlackboardKeySelector* Key = Property->ContainerPtrToValuePtr<FBlackboardKeySelector>(Node);
        Key->SelectedKeyName = Name;
        Key->ResolveSelectedKey(*Board);
    }

    template<typename T>
    T* AddTask(UBehaviorTree* Tree, UBTCompositeNode* Parent, const TCHAR* Name)
    {
        T* Task = NewObject<T>(Tree);
        Task->NodeName = Name;
        Parent->Children.AddDefaulted_GetRef().ChildTask = Task;
        return Task;
    }

    UBTComposite_Sequence* AddBranch(UBehaviorTree* Tree, const TCHAR* Name, const TCHAR* Condition = nullptr)
    {
        UBTComposite_Sequence* Branch = NewObject<UBTComposite_Sequence>(Tree);
        Branch->NodeName = Name;
        FBTCompositeChild& Child = Tree->RootNode->Children.AddDefaulted_GetRef();
        Child.ChildComposite = Branch;
        if (Condition)
        {
            UBTDecorator_Blackboard* Check = NewObject<UBTDecorator_Blackboard>(Tree);
            ConfigureKey(Check, Condition, Tree->BlackboardAsset);
            const FByteProperty* Abort = FindFProperty<FByteProperty>(Check->GetClass(), TEXT("FlowAbortMode"));
            Abort->SetPropertyValue_InContainer(Check, EBTFlowAbortMode::Both);
            Child.Decorators.Add(Check);
        }
        return Branch;
    }

    UBehaviorTree* MakeTree(UBlackboardData* Board)
    {
        UBehaviorTree* Tree = MakeAsset<UBehaviorTree>(TEXT("BT_DepotGuard"));
        Tree->BlackboardAsset = Board;
        Tree->RootNode = NewObject<UBTComposite_Selector>(Tree);
        Tree->RootNode->NodeName = TEXT("Choose highest priority behavior");
        Tree->RootNode->Services.Add(NewObject<UBTService_PatrolAwareness>(Tree));

        UBTComposite_Sequence* Chase = AddBranch(Tree, TEXT("Chase visible player"), TEXT("TargetActor"));
        ConfigureKey(AddTask<UBTTask_MoveTo>(Tree, Chase, TEXT("Follow target")), TEXT("TargetActor"), Board);
        AddTask<UBTTask_Wait>(Tree, Chase, TEXT("Watch target nearby"))->WaitTime = 0.25f;

        UBTComposite_Sequence* Search = AddBranch(Tree, TEXT("Investigate evidence"), TEXT("SearchActive"));
        AddTask<UBTTask_PatrolDestination>(Tree, Search, TEXT("Sample search area"))->bSearch = true;
        ConfigureKey(AddTask<UBTTask_MoveTo>(Tree, Search, TEXT("Move to search point")), TEXT("Destination"), Board);
        AddTask<UBTTask_Wait>(Tree, Search, TEXT("Listen briefly"))->WaitTime = 0.65f;

        UBTComposite_Sequence* Patrol = AddBranch(Tree, TEXT("Patrol around home"));
        AddTask<UBTTask_PatrolDestination>(Tree, Patrol, TEXT("Sample patrol area"));
        ConfigureKey(AddTask<UBTTask_MoveTo>(Tree, Patrol, TEXT("Move to patrol point")), TEXT("Destination"), Board);
        AddTask<UBTTask_Wait>(Tree, Patrol, TEXT("Wait and observe"))->WaitTime = 0.9f;
        AddTask<UBTTask_Wait>(Tree, Tree->RootNode, TEXT("Retry when navigation is ready"))->WaitTime = 0.6f;

        UBehaviorTreeGraph* Graph = CastChecked<UBehaviorTreeGraph>(FBlueprintEditorUtils::CreateNewGraph(
            Tree, TEXT("Behavior Tree"), UBehaviorTreeGraph::StaticClass(), UEdGraphSchema_BehaviorTree::StaticClass()));
        Tree->BTGraph = Graph;
        Graph->LockUpdates();
        Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UBehaviorTreeGraphNode_Root* Root = Cast<UBehaviorTreeGraphNode_Root>(Node))
            {
                Root->BlackboardAsset = Board;
                Tree->BlackboardAsset = Board;
                break;
            }
        }
        Graph->OnCreated();

        // Space branches so their task rows do not overlap in the Behavior Tree editor.
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            const UBehaviorTreeGraphNode* GraphNode = Cast<UBehaviorTreeGraphNode>(Node);
            if (!GraphNode || !GraphNode->NodeInstance)
            {
                continue;
            }
            for (int32 BranchIndex = 0; BranchIndex < Tree->RootNode->Children.Num(); ++BranchIndex)
            {
                const FBTCompositeChild& Child = Tree->RootNode->Children[BranchIndex];
                if (GraphNode->NodeInstance == Child.ChildComposite || GraphNode->NodeInstance == Child.ChildTask)
                {
                    Node->NodePosX = BranchIndex * 1200;
                }
                if (Child.ChildComposite)
                {
                    for (int32 TaskIndex = 0; TaskIndex < Child.ChildComposite->Children.Num(); ++TaskIndex)
                    {
                        if (GraphNode->NodeInstance == Child.ChildComposite->Children[TaskIndex].ChildTask)
                        {
                            Node->NodePosX = BranchIndex * 1200 + TaskIndex * 340;
                        }
                    }
                }
            }
        }
        Graph->OnLoaded();
        Graph->UnlockUpdates();
        Graph->UpdateAsset();
        return Tree;
    }

    UMaterial* SolidFinish(const TCHAR* Name, const TCHAR* Hex, float Roughness, float Metallic = 0, bool bTwoSided = false)
    {
        UMaterial* Surface = MakeAsset<UMaterial>(Name);
        const FLinearColor Color = FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
        UMaterialEditorOnlyData* Inputs = Surface->GetEditorOnlyData();
        auto* Constant = Cast<UMaterialExpressionConstant3Vector>(Inputs->BaseColor.Expression);
        if (!Constant)
        {
            Constant = NewObject<UMaterialExpressionConstant3Vector>(Surface);
            Constant->Material = Surface;
            Constant->MaterialExpressionEditorX = -300;
            Surface->GetExpressionCollection().AddExpression(Constant);
            Inputs->BaseColor.Connect(0, Constant);
        }
        Constant->Constant = Color;
        Inputs->Roughness.Constant = Roughness;
        Inputs->Metallic.Constant = Metallic;
        Surface->bUsedWithInstancedStaticMeshes = true;
        Surface->TwoSided = bTwoSided;
        Surface->PostEditChange();
        return SaveAsset(Surface) ? Surface : nullptr;
    }

    bool SavePropMeshes()
    {
        UMaterialInterface* Amber = SolidFinish(TEXT("M_DepotCrateAmber"), TEXT("DB853B"), 0.77f);
        UMaterialInterface* Teal = SolidFinish(TEXT("M_DepotDrumTeal"), TEXT("16AB9B"), 0.40f, 0.28f);
        UMaterialInterface* Frame = SolidFinish(TEXT("M_DepotDarkFrame"), TEXT("263847"), 0.60f, 0.18f);
        UMaterialInterface* Yellow = SolidFinish(TEXT("M_DepotSafetyYellow"), TEXT("FFCB47"), 0.50f);
        if (!Amber || !Teal || !Frame || !Yellow)
        {
            return false;
        }

        UStaticMesh* Crate = MakeAsset<UStaticMesh>(TEXT("SM_DepotCargoCrate"));
        UStaticMesh* Drum = MakeAsset<UStaticMesh>(TEXT("SM_DepotSteelDrum"));
        UMaterialInterface* CrateMaterials[] = { Amber, Frame, Yellow };
        UMaterialInterface* DrumMaterials[] = { Teal, Frame, Yellow };
        DepotPropMeshes::BuildCrate(*Crate, CrateMaterials);
        DepotPropMeshes::BuildDrum(*Drum, DrumMaterials);
        return SaveAsset(Crate) && SaveAsset(Drum);
    }

    bool ReplaceInteractionScreenPrint()
    {
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr,
            TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter"));
        if (!Blueprint)
        {
            return false;
        }

        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (UEdGraph* Graph : Graphs)
        {
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                auto* Call = Cast<UK2Node_CallFunction>(Node);
                if (!Call || Call->FunctionReference.GetMemberName() != TEXT("PrintString"))
                {
                    continue;
                }

                const UEdGraphPin* Text = Call->FindPin(TEXT("InString"));
                UEdGraphPin* Screen = Call->FindPin(TEXT("bPrintToScreen"));
                if (Text && Screen && Text->DefaultValue.Contains(TEXT("Interaction triggered")))
                {
                    if (Screen->DefaultValue == TEXT("false"))
                    {
                        return true;
                    }
                    Call->Modify();
                    Graph->GetSchema()->TrySetDefaultValue(*Screen, TEXT("false"));
                    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
                    FKismetEditorUtilities::CompileBlueprint(Blueprint);
                    return Blueprint->Status != BS_Error && SaveAsset(Blueprint);
                }
            }
        }
        UE_LOG(LogTemp, Error, TEXT("Expected the interaction Print String in BP_ThirdPersonCharacter."));
        return false;
    }

    UInstancedStaticMeshComponent* AddFinishParts(AActor* Owner, FName Name, UStaticMesh* Model, UMaterialInterface* Finish)
    {
        auto* Parts = NewObject<UInstancedStaticMeshComponent>(Owner, Name, RF_Transactional);
        Parts->SetMobility(EComponentMobility::Static);
        Parts->SetupAttachment(Owner->GetRootComponent());
        Parts->SetStaticMesh(Model);
        if (Finish)
        {
            Parts->SetMaterial(0, Finish);
        }
        Parts->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Parts->SetCanEverAffectNavigation(false);
        Parts->SetCastShadow(false);
        Owner->AddInstanceComponent(Parts);
        Parts->RegisterComponent();
        return Parts;
    }

    void AddPart(UInstancedStaticMeshComponent* Parts, FVector Center, FVector Size, double Yaw = 0)
    {
        Parts->AddInstance(FTransform(FRotator(0, Yaw, 0), Center, Size / 100.0));
    }

    bool FinishDepot(UWorld* World)
    {
        UMaterial* Concrete = SolidFinish(TEXT("M_DepotFloor"), TEXT("687E86"), 0.86f);
        UMaterial* Walls = SolidFinish(TEXT("M_DepotWall"), TEXT("204554"), 0.72f);
        UMaterial* Panels = SolidFinish(TEXT("M_DepotPanels"), TEXT("327783"), 0.61f);
        UMaterial* Trim = SolidFinish(TEXT("M_DepotPaleTrim"), TEXT("C0D2CC"), 0.60f);
        UMaterial* Paint = SolidFinish(TEXT("M_DepotSafetyYellow"), TEXT("FFCB47"), 0.50f);
        UMaterial* Joint = SolidFinish(TEXT("M_DepotFloorJoint"), TEXT("405C67"), 0.89f);
        if (!Concrete || !Walls || !Panels || !Trim || !Paint || !Joint)
        {
            return false;
        }

        UMaterialInterface* WeedColors[] = {
            SolidFinish(TEXT("M_DepotWeedsMoss"), TEXT("749032"), 0.92f, 0, true),
            SolidFinish(TEXT("M_DepotWeedsLight"), TEXT("A3AB4D"), 0.92f, 0, true),
            SolidFinish(TEXT("M_DepotWeedsStraw"), TEXT("B99B55"), 0.92f, 0, true)
        };
        if (!WeedColors[0] || !WeedColors[1] || !WeedColors[2])
        {
            return false;
        }
        UStaticMesh* WeedMesh = MakeAsset<UStaticMesh>(TEXT("SM_DepotWeedClump"));
        DepotPropMeshes::BuildWeeds(*WeedMesh, WeedColors);
        if (!SaveAsset(WeedMesh))
        {
            return false;
        }

        for (TActorIterator<AActor> It(World); It; ++It)
        {
            const FString Label = It->GetActorLabel();
            if (Label == TEXT("Depot finish panels"))
            {
                World->EditorDestroyActor(*It, true);
            }
            else if (AStaticMeshActor* Existing = Cast<AStaticMeshActor>(*It))
            {
                if (Label == TEXT("Depot concrete"))
                {
                    Existing->GetStaticMeshComponent()->SetMaterial(0, Concrete);
                }
                else if (Label.EndsWith(TEXT(" wall")) || Label == TEXT("Sound test cover"))
                {
                    Existing->GetStaticMeshComponent()->SetMaterial(0, Walls);
                }
            }
        }

        AActor* Owner = World->SpawnActor<AActor>();
        Owner->SetActorLabel(TEXT("Depot finish panels"));
        Owner->Tags.Add(TEXT("DepotFinish"));
        auto* Root = NewObject<USceneComponent>(Owner, TEXT("FinishRoot"), RF_Transactional);
        Root->SetMobility(EComponentMobility::Static);
        Owner->SetRootComponent(Root);
        Owner->AddInstanceComponent(Root);
        Root->RegisterComponent();

        UStaticMesh* Profile = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        auto* PanelParts = AddFinishParts(Owner, TEXT("WallPanels"), Profile, Panels);
        auto* TrimParts = AddFinishParts(Owner, TEXT("PanelTrim"), Profile, Trim);
        auto* PaintParts = AddFinishParts(Owner, TEXT("SafetyMarkings"), Profile, Paint);
        auto* JointParts = AddFinishParts(Owner, TEXT("FloorJoints"), Profile, Joint);
        auto* Weeds = AddFinishParts(Owner, TEXT("PerimeterWeeds"), WeedMesh, nullptr);

        auto AddWall = [&](FVector2D Center, double Yaw, double Span)
        {
            const FRotator Rotation(0, Yaw, 0);
            const auto OnWall = [&](double Along, double Depth, double Z)
            {
                return FVector(Center.X, Center.Y, Z) + Rotation.RotateVector(FVector(Along, Depth, 0));
            };
            for (double X = -Span * 0.5 + 140; X + 130 < Span * 0.5; X += 280)
            {
                AddPart(PanelParts, OnWall(X, 0, 151), FVector(260, 8, 166), Yaw);
                AddPart(TrimParts, OnWall(X + 133, 5, 145), FVector(11, 12, 245), Yaw);
            }
            AddPart(TrimParts, OnWall(0, 4, 258), FVector(Span, 13, 13), Yaw);
            AddPart(PanelParts, OnWall(0, 4, 41), FVector(Span, 13, 65), Yaw);
            AddPart(PaintParts, OnWall(0, 11, 77), FVector(Span, 3, 8), Yaw);
        };

        AddWall(FVector2D(0, 2399), 180, 6000);
        AddWall(FVector2D(0, -2399), 0, 6000);
        AddWall(FVector2D(2999, 0), 90, 4800);
        AddWall(FVector2D(-2999, 0), -90, 4800);
        AddWall(FVector2D(-2301, 0), 90, 680);
        AddWall(FVector2D(-2199, 0), -90, 680);

        for (double X = -2400; X <= 2400; X += 600)
        {
            AddPart(JointParts, FVector(X, 0, 0.13), FVector(2.5, 4780, 0.22));
        }
        for (double Y = -1800; Y <= 1800; Y += 600)
        {
            AddPart(JointParts, FVector(0, Y, 0.13), FVector(5980, 2.5, 0.22));
        }
        for (double X : { -2900.0, -2480.0 })
        {
            AddPart(PaintParts, FVector(X, 0, 0.38), FVector(7, 4440, 0.24));
        }
        for (double Y : { -2230.0, 2230.0 })
        {
            AddPart(PaintParts, FVector(0, Y, 0.38), FVector(5680, 7, 0.24));
        }
        for (double Y = -1960; Y <= 1960; Y += 280)
        {
            AddPart(TrimParts, FVector(-2690, Y, 0.37), FVector(10, 96, 0.24));
        }

        FRandomStream Placement(614);
        auto Cluster = [&](FVector Center, int32 Count)
        {
            for (int32 Part = 0; Part < Count; ++Part)
            {
                FVector P = Center + FVector(Placement.FRandRange(-70, 70), Placement.FRandRange(-55, 55), 0);
                const float Scale = Placement.FRandRange(0.55f, 1.05f);
                Weeds->AddInstance(FTransform(FRotator(0, Placement.FRandRange(0, 360), 0), P, FVector(Scale)));
            }
        };
        for (double X : { -2680.0, -1350.0, 400.0, 1680.0, 2740.0 })
        {
            Cluster(FVector(X, -2320, 0), 3);
            Cluster(FVector(X, 2320, 0), 4);
        }
        for (double Y : { -1530.0, -540.0, 760.0, 1680.0 })
        {
            Cluster(FVector(2910, Y, 0), 4);
        }
        Cluster(FVector(-2230, -395, 0), 3);
        Cluster(FVector(-2230, 395, 0), 4);
        return true;
    }

    UPCGGraph* MakeScatter()
    {
        UPCGGraph* Graph = MakeAsset<UPCGGraph>(TEXT("PCG_DepotScatter"));
        TArray<UPCGNode*> OldNodes = Graph->GetNodes();
        Graph->RemoveNodes(OldNodes);

        UPCGCreatePointsGridSettings* Grid = nullptr;
        UPCGTransformPointsSettings* Jitter = nullptr;
        UPCGStaticMeshSpawnerSettings* Spawner = nullptr;
        UPCGNode* GridNode = Graph->AddNodeOfType(Grid);
        UPCGNode* JitterNode = Graph->AddNodeOfType(Jitter);
        UPCGNode* SpawnerNode = Graph->AddNodeOfType(Spawner);

        Grid->GridExtents = FVector(2160, 1920, 0);
        Grid->CellSize = FVector(480, 480, 400);
        Jitter->OffsetMin = FVector(-95, -95, 0);
        Jitter->OffsetMax = FVector(95, 95, 0);
        Jitter->RotationMax = FRotator(0, 360, 0);
        Jitter->ScaleMin = FVector(1.35, 1.35, 1.8);
        Jitter->ScaleMax = FVector(1.85, 1.85, 2.05);

        Spawner->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
        auto* Weighted = CastChecked<UPCGMeshSelectorWeighted>(Spawner->MeshSelectorParameters);
        auto AddMesh = [Weighted](const TCHAR* Path, int32 Weight)
        {
            FPCGMeshSelectorWeightedEntry& Entry = Weighted->MeshEntries.Emplace_GetRef(
                TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Path)), Weight);
            Entry.Descriptor.BodyInstance.SetCollisionProfileName(TEXT("BlockAll"));
            Entry.Descriptor.bCanEverAffectNavigation = true;

        };
        AddMesh(TEXT("/Game/PatrolDepot/SM_DepotCargoCrate.SM_DepotCargoCrate"), 2);
        AddMesh(TEXT("/Game/PatrolDepot/SM_DepotSteelDrum.SM_DepotSteelDrum"), 1);
        Spawner->bSynchronousLoad = true;

        GridNode->SetNodePosition(0, 0);
        JitterNode->SetNodePosition(270, 0);
        SpawnerNode->SetNodePosition(540, 0);
        Graph->GetInputNode()->SetNodePosition(-270, 0);
        Graph->GetOutputNode()->SetNodePosition(810, 0);
        Graph->AddEdge(GridNode, TEXT("Out"), JitterNode, TEXT("In"));
        Graph->AddEdge(JitterNode, TEXT("Out"), SpawnerNode, TEXT("In"));
        Graph->AddEdge(SpawnerNode, TEXT("Out"), Graph->GetOutputNode(), TEXT("Out"));
        return Graph;
    }

    void SetBounds(AVolume* Volume, const FVector& Dimensions)
    {
        UCubeBuilder* Builder = NewObject<UCubeBuilder>(Volume, NAME_None, RF_Transactional);
        Builder->X = Dimensions.X;
        Builder->Y = Dimensions.Y;
        Builder->Z = Dimensions.Z;
        UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
        Volume->ReregisterAllComponents();
        Volume->PostEditChange();
    }

    AStaticMeshActor* Block(UWorld* World, const TCHAR* Label, FVector Center, FVector Scale, const TCHAR* MaterialPath)
    {
        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Center, FRotator::ZeroRotator);
        Actor->SetActorLabel(Label);
        UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, MaterialPath));
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Actor->SetActorScale3D(Scale);
        return Actor;
    }
    bool SaveFinishedDepot(UWorld* World, const FString& Filename)
    {
        if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
        {
            for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
            {
                Nav->OnNavigationBoundsUpdated(*It);
            }
            Nav->Build();
        }
        return FEditorFileUtils::SaveMap(World, Filename);
    }
}

int32 UBuildPatrolLevelCommandlet::Main(const FString& Params)
{
    const FString MapPath = FString(Folder) + TEXT("Lvl_PatrolDepot");
    if (FParse::Param(*Params, TEXT("RefreshFinish")))
    {
        const FString Filename = FPackageName::LongPackageNameToFilename(MapPath, FPackageName::GetMapPackageExtension());
        UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(Filename);
        return World && SavePropMeshes() && ReplaceInteractionScreenPrint()
            && FinishDepot(World) && SaveFinishedDepot(World, Filename) ? 0 : 6;
    }

    if (FParse::Param(*Params, TEXT("RefreshProps")))
    {
        return SavePropMeshes() && SaveAsset(MakeScatter()) ? 0 : 5;
    }

    if (FPackageName::DoesPackageExist(MapPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Patrol Depot already exists. Move the existing assets before rebuilding."));
        return 1;
    }

    UBehaviorTree* Tree = nullptr;
    UPCGGraph* Scatter = nullptr;
    if (FParse::Param(*Params, TEXT("RebuildLayout")))
    {
        Tree = MakeAsset<UBehaviorTree>(TEXT("BT_DepotGuard"));
        Scatter = MakeAsset<UPCGGraph>(TEXT("PCG_DepotScatter"));
        if (!Tree || !Tree->RootNode || !Scatter || !SavePropMeshes())
        {
            return 2;
        }
    }
    else
    {
        UBlackboardData* Board = MakeAsset<UBlackboardData>(TEXT("BB_DepotGuard"));
        UBlackboardKeyType_Object* TargetType = NewObject<UBlackboardKeyType_Object>(Board);
        TargetType->BaseClass = AActor::StaticClass();
        AddKey(Board, TEXT("TargetActor"), TargetType);
        AddKey(Board, TEXT("SearchActive"), NewObject<UBlackboardKeyType_Bool>(Board));
        AddKey(Board, TEXT("EvidenceLocation"), NewObject<UBlackboardKeyType_Vector>(Board));
        AddKey(Board, TEXT("SearchRadius"), NewObject<UBlackboardKeyType_Float>(Board));
        AddKey(Board, TEXT("Home"), NewObject<UBlackboardKeyType_Vector>(Board));
        AddKey(Board, TEXT("Destination"), NewObject<UBlackboardKeyType_Vector>(Board));
        Board->UpdateKeyIDs();

        Tree = MakeTree(Board);
        if (!SavePropMeshes())
        {
            return 5;
        }
        Scatter = MakeScatter();
        if (!Tree->RootNode || Tree->RootNode->Children.Num() != 4
            || !SaveAsset(Board) || !SaveAsset(Tree) || !SaveAsset(Scatter))
        {
            return 2;
        }
    }

    UWorld* World = GEditor->NewMap(false);
    Block(World, TEXT("Depot concrete"), FVector(0, 0, -50), FVector(62, 50, 1), FloorMaterial);
    Block(World, TEXT("North wall"), FVector(0, 2450, 140), FVector(61, 1, 2.8), WallMaterial);
    Block(World, TEXT("South wall"), FVector(0, -2450, 140), FVector(61, 1, 2.8), WallMaterial);
    Block(World, TEXT("East wall"), FVector(3050, 0, 140), FVector(1, 50, 2.8), WallMaterial);
    Block(World, TEXT("West wall"), FVector(-3050, 0, 140), FVector(1, 50, 2.8), WallMaterial);
    Block(World, TEXT("Sound test cover"), FVector(-2250, 0, 145), FVector(1, 7, 2.9), WallMaterial);

    auto* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 1000), FRotator(-45, -35, 0));
    Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    auto* Sky = World->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetRealTimeCapture(true);
    World->SpawnActor<ASkyAtmosphere>();

    World->SpawnActor<APlayerStart>(FVector(-2700, 0, 110), FRotator(0, 0, 0));
    auto* Guard = World->SpawnActor<APatrolGuard>(FVector(-1020, 0, 100), FRotator(0, 180, 0));
    Guard->Behavior = Tree;
    Guard->SetActorLabel(TEXT("Depot guard"));

    auto* NavBounds = World->SpawnActor<ANavMeshBoundsVolume>(FVector(0, 0, 150), FRotator::ZeroRotator);
    SetBounds(NavBounds, FVector(6100, 4900, 800));
    if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
    {
        Nav->OnNavigationBoundsUpdated(NavBounds);
    }

    auto* Volume = World->SpawnActor<APCGVolume>();
    Volume->SetActorLabel(TEXT("Depot props - PCG"));
    SetBounds(Volume, FVector(4400, 4000, 500));
    UPCGComponent* Component = Volume->FindComponentByClass<UPCGComponent>();
    if (!Component)
    {
        return 3;
    }
    Component->Seed = 2411;
    Component->SetGraph(Scatter);

    const FString Filename = FPackageName::LongPackageNameToFilename(MapPath, FPackageName::GetMapPackageExtension());
    return ReplaceInteractionScreenPrint() && FinishDepot(World) && FEditorFileUtils::SaveMap(World, Filename) ? 0 : 4;
}
