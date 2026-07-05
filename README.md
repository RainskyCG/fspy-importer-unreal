# fSpy Importer for Unreal Engine

English | [简体中文](README.zh-CN.md)

An Unreal Engine editor plugin that imports `.fspy` camera solve files as Cine Camera actors, including the embedded reference image as a post-process overlay.

## Features

- Adds `File > Import fSpy Camera...` to the Unreal Editor menu.
- Reads fSpy project files directly from `.fspy` binary data.
- Creates a Cine Camera actor with the solved camera transform.
- Applies filmback, field of view, aspect ratio, and principal point offsets from the fSpy solve.
- Extracts the embedded JPEG or PNG reference image.
- Creates texture, post-process material, and material instance assets under `/Game/FSpyImports`.
- Assigns the reference image to the imported camera as a post-process overlay with adjustable opacity.

## Requirements

- Unreal Engine 5.x
- Windows 64-bit editor target
- A valid `.fspy` project file exported from [fSpy](https://fspy.io/)

The plugin is currently configured for `Win64` editor builds.

## Installation

1. Copy this repository into your project's `Plugins` directory:

   ```text
   YourProject/
     Plugins/
       fspy_importer/
         fspy_importer.uplugin
   ```

2. Regenerate your project files if needed.
3. Build or open the Unreal project.
4. Enable **fSpy Importer** in **Edit > Plugins** if it is not already enabled.
5. Restart the editor when prompted.

## Usage

1. Open your Unreal project.
2. Choose **File > Import fSpy Camera...**.
3. Select a `.fspy` file.
4. The plugin imports the camera into the current level and selects it.

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

- camera transform
- horizontal field of view
- image resolution and aspect ratio
- principal point offset
- camera sensor size when available
- reference distance unit for scene scale
- embedded JPEG or PNG reference image

The imported Cine Camera receives a post-process material instance that displays the fSpy reference image over the camera view. The default reference opacity is `0.5`.

## Limitations

- Only fSpy project format version `1` is supported.
- Only embedded JPEG and PNG reference images are supported.
- The module is editor-only and does not run at runtime.
- The plugin is currently restricted to `Win64`.

## Development

The plugin module is located at:

```text
Source/fspy_importer
```

Main files:

- `fspy_importer.uplugin`
- `Source/fspy_importer/fspy_importer.Build.cs`
- `Source/fspy_importer/Public/fspy_importer.h`
- `Source/fspy_importer/Private/fspy_importer.cpp`

## License

No license file is included yet. Add a license before distributing or accepting external contributions.
