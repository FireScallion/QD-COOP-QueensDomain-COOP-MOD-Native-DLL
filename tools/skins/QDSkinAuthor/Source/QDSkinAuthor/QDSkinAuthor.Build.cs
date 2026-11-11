using UnrealBuildTool;
public class QDSkinAuthor:ModuleRules { public QDSkinAuthor(ReadOnlyTargetRules Target):base(Target){PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine"});PrivateDependencyModuleNames.AddRange(new[]{"UnrealEd","Json"});} }
