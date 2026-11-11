using UnrealBuildTool;
public class QDAuthor : ModuleRules {
 public QDAuthor(ReadOnlyTargetRules Target):base(Target) {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine"});
  PrivateDependencyModuleNames.AddRange(new string[]{"UnrealEd","AnimGraph","AnimGraphRuntime","BlueprintGraph","Kismet","KismetCompiler","AssetRegistry","Json"});
 }
}
