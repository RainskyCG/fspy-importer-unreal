# Unreal Engine fSpy Importer

简体中文 | [English](README.en.md)

fSpy Importer 是一个 Unreal Engine 编辑器插件，用于将 `.fspy` 摄像机反求文件导入为 Cine Camera Actor，并把 fSpy 文件中嵌入的参考图作为摄像机后期叠加图使用。

## 下载

推荐直接下载 GitHub Releases 中的预编译包：

[下载 fspy_importer-v1.0.0-ue5.7-win64.zip](https://github.com/RainskyCG/fspy-importer-unreal/releases/download/v1.0.0/fspy_importer-v1.0.0-ue5.7-win64.zip)

当前发布包信息：

- 版本：`v1.0.0`
- Unreal Engine：`5.7`
- 平台：`Win64`
- SHA256：`94B1CB0B5C5A7CF1D67595EA00B351F06F2D16244DCAE3BFE0EF2653BC1F70A3`

也可以从 Release 页面查看所有发布文件：

[https://github.com/RainskyCG/fspy-importer-unreal/releases/tag/v1.0.0](https://github.com/RainskyCG/fspy-importer-unreal/releases/tag/v1.0.0)

## 功能

- 在 Unreal Editor 的 `File > Import fSpy Camera...` 菜单中添加导入入口。
- 读取 fSpy `.fspy` 项目文件。
- 根据 fSpy 反求结果创建 Cine Camera Actor。
- 导入摄像机位置、旋转、水平视场角、Filmback、画幅比例和主点偏移。
- 按 fSpy 文件中的参考距离单位换算 Unreal 场景比例。
- 提取 `.fspy` 文件内嵌的 JPEG 或 PNG 参考图。
- 在 `/Game/FSpyImports` 下创建参考图贴图、后期材质和材质实例。
- 将参考图作为后期叠加图绑定到导入的 Cine Camera，默认透明度为 `0.5`。

## 环境要求

预编译发布包：

- Unreal Engine `5.7`
- Windows 64-bit 编辑器

源码版本：

- Unreal Engine 5.x
- Windows 64-bit 编辑器目标
- 可用的 C++ 编译环境

插件当前在 `.uplugin` 中限制为 `Win64`，模块类型为 `Editor`，不支持运行时使用。

## 安装预编译包

1. 下载 `fspy_importer-v1.0.0-ue5.7-win64.zip`。
2. 解压后得到 `fspy_importer` 文件夹。
3. 将该文件夹复制到 Unreal 项目的 `Plugins` 目录：

   ```text
   YourProject/
     Plugins/
       fspy_importer/
         fspy_importer.uplugin
   ```

4. 打开 Unreal 项目。
5. 如果插件没有自动启用，在 **Edit > Plugins** 中启用 **fSpy Importer**。
6. 根据提示重启编辑器。

## 从源码安装

1. 将本仓库复制或克隆到 Unreal 项目的 `Plugins` 目录：

   ```text
   YourProject/
     Plugins/
       fspy_importer/
   ```

2. 重新生成项目文件。
3. 构建项目或直接打开 Unreal Editor 让 Unreal 编译插件。
4. 在 **Edit > Plugins** 中确认 **fSpy Importer** 已启用。

## 使用方法

1. 打开 Unreal 项目。
2. 选择 **File > Import fSpy Camera...**。
3. 选择一个由 [fSpy](https://fspy.io/) 保存的 `.fspy` 文件。
4. 插件会将摄像机导入到当前关卡，并自动选中新创建的 Cine Camera Actor。

导入生成的资源会放在：

```text
/Game/FSpyImports
```

摄像机 Actor 的命名格式为：

```text
CineCamera_Ref_<fspy_file_name>
```

## 导入内容

导入器会读取：

- 摄像机变换矩阵
- 水平视场角
- 图像分辨率和画幅比例
- 主点偏移
- 可用时读取摄像机传感器尺寸
- 参考距离单位
- 内嵌 JPEG 或 PNG 参考图

导入后，Cine Camera 的 Post Process Settings 会添加一个参考图材质实例。通过该材质实例中的 `Opacity` 参数可以调整参考图叠加透明度。

## 限制

- 目前仅支持 fSpy 项目格式版本 `1`。
- 目前仅支持 `.fspy` 内嵌的 JPEG 和 PNG 参考图。
- 该插件是 Editor 模块，不支持运行时导入。
- 当前发布包仅面向 Unreal Engine `5.7` / `Win64`。

## 开发

主要文件：

- `fspy_importer.uplugin`
- `Source/fspy_importer/fspy_importer.Build.cs`
- `Source/fspy_importer/Public/fspy_importer.h`
- `Source/fspy_importer/Private/fspy_importer.cpp`

发布包使用 Unreal Engine 5.7 的 `RunUAT BuildPlugin` 生成。

## 许可证

仓库当前尚未包含许可证文件。正式分发或接受外部贡献前，建议先补充许可证。
