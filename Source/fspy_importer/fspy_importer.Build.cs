using UnrealBuildTool;

public class fspy_importer : ModuleRules
{
	public fspy_importer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core"
			});

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"AssetRegistry",
				"AssetTools",
				"CinematicCamera",
				"CoreUObject",
				"DesktopPlatform",
				"Engine",
				"InputCore",
				"Json",
				"LevelEditor",
				"MaterialEditor",
				"Projects",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd"
			});
	}
}
