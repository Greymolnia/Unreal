using UnrealBuildTool;

public class HW2PatrolEditor : ModuleRules
{
    public HW2PatrolEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "UnrealEd", "AssetRegistry",
            "AIModule", "AIGraph", "BehaviorTreeEditor", "NavigationSystem", "PCG", "HW2",
            "MeshDescription", "StaticMeshDescription", "MeshConversion", "BlueprintGraph"
        });
    }
}
