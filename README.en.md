# fSpy Importer for Unreal Engine

[简体中文](README.md) | English

fSpy Importer is an Unreal Engine editor plugin that imports `.fspy` camera solve files as Cine Camera actors and applies the embedded reference image as a camera post-process overlay.

## Download

The recommended option is the prebuilt package from GitHub Releases:

[Download fspy_importer-v1.0.0-ue5.7-win64.zip](https://github.com/RainskyCG/fspy-importer-unreal/releases/download/v1.0.0/fspy_importer-v1.0.0-ue5.7-win64.zip)

Current release package:

- Version: `v1.0.0`
- Unreal Engine: `5.7`
- Platform: `Win64`
- SHA256: `94B1CB0B5C5A7CF1D67595EA00B351F06F2D16244DCAE3BFE0EF2653BC1F70A3`

You can also open the release page:

[https://github.com/RainskyCG/fspy-importer-unreal/releases/tag/v1.0.0](https://github.com/RainskyCG/fspy-importer-unreal/releases/tag/v1.0.0)

## Features

- Adds `File > Import fSpy Camera...` to the Unreal Editor menu.
- Reads fSpy `.fspy` project files.
- Creates a Cine Camera actor from the solved camera data.
- Imports camera location, rotation, horizontal field of view, filmback, aspect ratio, and principal point offsets.
- Converts scene scale from the reference distance unit stored in the fSpy file.
- Extracts the embedded JPEG or PNG reference image.
- Creates texture, post-process material, and material instance assets under `/Game/FSpyImports`.
- Assigns the reference image to the imported Cine Camera as a post-process overlay with default opacity `0.5`.

## Requirements

Prebuilt release package:

- Unreal Engine `5.7`
- Windows 64-bit editor

Source version:

- Unreal Engine 5.x
- Windows 64-bit editor target
- A working C++ build environment

The plugin is currently restricted to `Win64` in the `.uplugin` file. It is an editor module and does not run at runtime.

## Installing the Prebuilt Package

1. Download `fspy_importer-v1.0.0-ue5.7-win64.zip`.
2. Extract it to get the `fspy_importer` folder.
3. Copy that folder into your Unreal project's `Plugins` directory:

   ```text
   YourProject/
     Plugins/
       fspy_importer/
         fspy_importer.uplugin
   ```

4. Open the Unreal project.
5. Enable **fSpy Importer** in **Edit > Plugins** if it is not already enabled.
6. Restart the editor when prompted.

## Installing from Source

1. Copy or clone this repository into your Unreal project's `Plugins` directory:

   ```text
   YourProject/
     Plugins/
       fspy_importer/
   ```

2. Regenerate project files.
3. Build the project, or open Unreal Editor and let Unreal compile the plugin.
4. Confirm that **fSpy Importer** is enabled in **Edit > Plugins**.

## Usage

1. Open your Unreal project.
2. Choose **File > Import fSpy Camera...**.
3. Select a `.fspy` file saved from [fSpy](https://fspy.io/).
4. The plugin imports the camera into the current level and selects the new Cine Camera actor.

Imported assets are created in:

```text
/Game/FSpyImports
```

The camera actor is named with this pattern:

```text
CineCamera_Ref_<fspy_file_name>
```

## Import Behavior

The importer reads:

- camera transform matrix
- horizontal field of view
- image resolution and aspect ratio
- principal point offset
- camera sensor size when available
- reference distance unit
- embedded JPEG or PNG reference image

After import, the Cine Camera's Post Process Settings include a reference image material instance. Adjust the `Opacity` parameter on that material instance to change the overlay opacity.

## Limitations

- Only fSpy project format version `1` is supported.
- Only embedded JPEG and PNG reference images are supported.
- The plugin is an editor module and does not support runtime import.
- The current release package targets Unreal Engine `5.7` / `Win64` only.

## Development

Main files:

- `fspy_importer.uplugin`
- `Source/fspy_importer/fspy_importer.Build.cs`
- `Source/fspy_importer/Public/fspy_importer.h`
- `Source/fspy_importer/Private/fspy_importer.cpp`

The release package is generated with Unreal Engine 5.7 `RunUAT BuildPlugin`.

## License

No license file is included yet. Add a license before distributing or accepting external contributions.
