"""Robot builder asset setup: creates all 13 assets via one C++ call, then saves."""

import unreal

count = unreal.RobotBuilderFunctionLibrary.create_builder_assets()
unreal.EditorAssetLibrary.save_directory("/Game/Input", False)
unreal.EditorAssetLibrary.save_directory("/Game/RobotParts", False)

unreal.log("[RobotBuilderSetup] created/updated %d assets" % count)
