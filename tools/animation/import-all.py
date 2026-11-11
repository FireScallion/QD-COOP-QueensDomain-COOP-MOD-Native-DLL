import os

import unreal


def main():
    manifest = os.environ["QDCOOP_BAKED_MANIFEST"]
    for operation in (unreal.QDAnimTools.import_all, unreal.QDAnimTools.build_all):
        result = operation(manifest)
        if not result.startswith("OK "):
            raise RuntimeError(result)
        unreal.log(result)


main()
