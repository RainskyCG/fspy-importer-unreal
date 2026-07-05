# Unreal Engine fSpy Importer

[English](README.md) | 简体中文

这是一个 Unreal Engine 编辑器插件，用于将 `.fspy` 摄像机反求文件导入为 Cine Camera Actor，并把 fSpy 文件中嵌入的参考图作为后期叠加图应用到摄像机上。

## 功能

- 在 Unreal Editor 的 `File > Import fSpy Camera...` 菜单中添加导入入口。
- 直接读取 `.fspy` 二进制项目文件。
- 根据 fSpy 反求结果创建 Cine Camera Actor。
- 导入摄像机变换、Filmback、视场角、画幅比例和主点偏移。
- 提取 `.fspy` 文件内嵌的 JPEG 或 PNG 参考图。
- 在 `/Game/FSpyImports` 下创建贴图、后期材质和材质实例。
- 将参考图作为后期叠加图绑定到导入的摄像机，并提供可调透明度。

## 环境要求

- Unreal Engine 5.x
- Windows 64-bit 编辑器目标
- 由 [fSpy](https://fspy.io/) 保存的有效 `.fspy` 项目文件

当前插件配置为 `Win64` 编辑器构建。

## 安装

1. 将本仓库复制到 Unreal 项目的 `Plugins` 目录：

   ```text
   YourProject/
     Plugins/
       fspy_importer/
         fspy_importer.uplugin
   ```

2. 如有需要，重新生成项目文件。
3. 构建或打开 Unreal 项目。
4. 如果插件没有自动启用，在 **Edit > Plugins** 中启用 **fSpy Importer**。
5. 根据提示重启编辑器。

## 使用方法

1. 打开 Unreal 项目。
2. 选择 **File > Import fSpy Camera...**。
3. 选择一个 `.fspy` 文件。
4. 插件会将摄像机导入到当前关卡，并自动选中该摄像机。

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

- 摄像机变换
- 水平视场角
- 图像分辨率和画幅比例
- 主点偏移
- 可用时读取摄像机传感器尺寸
- 参考距离单位，用于场景比例换算
- 内嵌的 JPEG 或 PNG 参考图

导入后的 Cine Camera 会自动获得一个后期材质实例，用于在摄像机视图中叠加显示 fSpy 参考图。默认参考图透明度为 `0.5`。

## 限制

- 目前仅支持 fSpy 项目格式版本 `1`。
- 目前仅支持内嵌 JPEG 和 PNG 参考图。
- 该模块仅用于编辑器，不支持运行时使用。
- 当前插件限制为 `Win64` 平台。

## 开发

插件模块位于：

```text
Source/fspy_importer
```

主要文件：

- `fspy_importer.uplugin`
- `Source/fspy_importer/fspy_importer.Build.cs`
- `Source/fspy_importer/Public/fspy_importer.h`
- `Source/fspy_importer/Private/fspy_importer.cpp`

## 许可证

仓库当前尚未包含许可证文件。正式分发或接受外部贡献前，建议先补充许可证。
