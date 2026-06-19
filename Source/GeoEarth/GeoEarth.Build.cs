// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class GeoEarth : ModuleRules
{
    public GeoEarth(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
            "CesiumRuntime", "HTTP", "Json", "JsonUtilities",
            "ProceduralMeshComponent", "UMG", "Slate", "SlateCore", "RenderCore", "RHI"
        });

        UndefinedIdentifierWarningLevel = WarningLevel.Off;
        PublicDefinitions.Add("NOMINMAX=1");
        PublicDefinitions.Add("TRIANGLE_EXPORTS");

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "VectorTile", "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "VectorTile", "Private"));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "UI", "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "UI", "Private"));
        PublicIncludePaths.Add(ModuleDirectory);
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty", "FlatBuffers", "include"));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty", "FlatBuffers", "generated", "include"));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty", "Triangle"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty", "Triangle", "Private"));
    }
}
