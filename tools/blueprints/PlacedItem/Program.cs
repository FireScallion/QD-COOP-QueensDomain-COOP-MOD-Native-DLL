using UAssetAPI;
var a=UAsset.DeserializeJson(File.ReadAllText(args[0]));
a.Write(args[1]);
var b=new UAsset(args[1],UAssetAPI.UnrealTypes.EngineVersion.VER_UE4_27);
File.WriteAllText(args[1]+".json",b.SerializeJson());
Console.WriteLine($"Built {args[1]}, {b.Exports.Count} exports");
