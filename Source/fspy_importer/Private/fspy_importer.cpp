#include "fspy_importer.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Engine/Scene.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Factories/TextureFactory.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IDesktopPlatform.h"
#include "IAssetTools.h"
#include "Json.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "UObject/Package.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "Ffspy_importerModule"

namespace
{
	constexpr uint32 FSpyMagic = 2037412710u;
	constexpr double DefaultFilmbackWidthMm = 36.0;
	constexpr double DefaultFilmbackHeightMm = 24.0;
	constexpr float DefaultReferenceOpacity = 0.5f;
	const FName ReferenceImageParameterName(TEXT("ReferenceImage"));
	const FName ReferenceOpacityParameterName(TEXT("Opacity"));

	struct FFSpyProject
	{
		FString SourcePath;
		FString BaseName;
		FString ReferenceDistanceUnit;
		FVector2D PrincipalPoint = FVector2D::ZeroVector;
		double HorizontalFovRadians = 0.0;
		double VerticalFovRadians = 0.0;
		double RelativeFocalLength = 0.0;
		double SensorWidthMm = 0.0;
		double SensorHeightMm = 0.0;
		int32 ImageWidth = 0;
		int32 ImageHeight = 0;
		double CameraMatrix[4][4] = {};
		TArray<uint8> ImageData;
		FString ImageType;
	};

	uint32 ReadUInt32LE(const TArray<uint8>& Data, const int32 Offset)
	{
		return static_cast<uint32>(Data[Offset]) |
			(static_cast<uint32>(Data[Offset + 1]) << 8) |
			(static_cast<uint32>(Data[Offset + 2]) << 16) |
			(static_cast<uint32>(Data[Offset + 3]) << 24);
	}

	FString SanitizeAssetName(FString Name)
	{
		for (TCHAR& Character : Name)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
			{
				Character = TEXT('_');
			}
		}
		if (Name.IsEmpty())
		{
			Name = TEXT("fspy_camera");
		}
		return Name;
	}

	bool IsChineseEditorCulture()
	{
		const FString CultureName = FInternationalization::Get().GetCurrentCulture()->GetName();
		return CultureName.StartsWith(TEXT("zh"), ESearchCase::IgnoreCase);
	}

	FText LocalizedText(const TCHAR* Chinese, const TCHAR* English)
	{
		return FText::FromString(IsChineseEditorCulture() ? FString(Chinese) : FString(English));
	}

	double UnitToCentimeters(const FString& Unit)
	{
		if (Unit == TEXT("Millimeters"))
		{
			return 0.1;
		}
		if (Unit == TEXT("Centimeters"))
		{
			return 1.0;
		}
		if (Unit == TEXT("Meters"))
		{
			return 100.0;
		}
		if (Unit == TEXT("Kilometers"))
		{
			return 100000.0;
		}
		if (Unit == TEXT("Inches"))
		{
			return 2.54;
		}
		if (Unit == TEXT("Feet"))
		{
			return 30.48;
		}
		if (Unit == TEXT("Miles"))
		{
			return 160934.4;
		}
		return 100.0;
	}

	FVector BlenderVectorToUnreal(const FVector& BlenderVector, const double Scale = 1.0)
	{
		return FVector(BlenderVector.Y, BlenderVector.X, BlenderVector.Z) * Scale;
	}

	bool TryGetNumber(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		double& OutValue,
		FText& OutError)
	{
		if (!Object.IsValid() || !Object->TryGetNumberField(FieldName, OutValue))
		{
			OutError = FText::Format(LOCTEXT("MissingNumberField", "Missing numeric field '{0}'."), FText::FromString(FieldName));
			return false;
		}
		return true;
	}

	bool ParseCameraMatrix(
		const TSharedPtr<FJsonObject>& CameraParameters,
		FFSpyProject& OutProject,
		FText& OutError)
	{
		const TSharedPtr<FJsonObject>* CameraTransformObject = nullptr;
		if (!CameraParameters->TryGetObjectField(TEXT("cameraTransform"), CameraTransformObject))
		{
			OutError = LOCTEXT("MissingCameraTransform", "The fSpy file has no cameraTransform.");
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (!(*CameraTransformObject)->TryGetArrayField(TEXT("rows"), Rows) || Rows->Num() != 4)
		{
			OutError = LOCTEXT("InvalidCameraTransformRows", "The fSpy cameraTransform must contain four rows.");
			return false;
		}

		for (int32 RowIndex = 0; RowIndex < 4; ++RowIndex)
		{
			const TArray<TSharedPtr<FJsonValue>>* Columns = nullptr;
			if (!(*Rows)[RowIndex]->TryGetArray(Columns) || Columns->Num() != 4)
			{
				OutError = LOCTEXT("InvalidCameraTransformColumns", "The fSpy cameraTransform rows must each contain four values.");
				return false;
			}

			for (int32 ColumnIndex = 0; ColumnIndex < 4; ++ColumnIndex)
			{
				OutProject.CameraMatrix[RowIndex][ColumnIndex] = (*Columns)[ColumnIndex]->AsNumber();
			}
		}

		return true;
	}

	bool DetectImageType(const TArray<uint8>& ImageData, FString& OutType)
	{
		if (ImageData.Num() >= 3 &&
			ImageData[0] == 0xff &&
			ImageData[1] == 0xd8 &&
			ImageData[2] == 0xff)
		{
			OutType = TEXT("jpg");
			return true;
		}
		if (ImageData.Num() >= 8 &&
			ImageData[0] == 0x89 &&
			ImageData[1] == 0x50 &&
			ImageData[2] == 0x4e &&
			ImageData[3] == 0x47)
		{
			OutType = TEXT("png");
			return true;
		}
		return false;
	}

	bool ParseFSpyProject(const FString& FilePath, FFSpyProject& OutProject, FText& OutError)
	{
		TArray<uint8> FileBytes;
		if (!FFileHelper::LoadFileToArray(FileBytes, *FilePath))
		{
			OutError = FText::Format(LOCTEXT("ReadFailed", "Could not read '{0}'."), FText::FromString(FilePath));
			return false;
		}

		if (FileBytes.Num() < 16 || ReadUInt32LE(FileBytes, 0) != FSpyMagic)
		{
			OutError = LOCTEXT("NotFSpy", "This is not a valid fSpy project file.");
			return false;
		}

		const uint32 Version = ReadUInt32LE(FileBytes, 4);
		if (Version != 1)
		{
			OutError = FText::Format(LOCTEXT("UnsupportedVersion", "Unsupported fSpy project version {0}."), FText::AsNumber(Version));
			return false;
		}

		const uint32 StateSize = ReadUInt32LE(FileBytes, 8);
		const uint32 ImageSize = ReadUInt32LE(FileBytes, 12);
		const int64 ExpectedSize = 16ll + static_cast<int64>(StateSize) + static_cast<int64>(ImageSize);
		if (StateSize == 0 || ImageSize == 0 || ExpectedSize > FileBytes.Num())
		{
			OutError = LOCTEXT("InvalidSizes", "The fSpy file has invalid JSON or image data sizes.");
			return false;
		}

		const FUTF8ToTCHAR ConvertedState(reinterpret_cast<const ANSICHAR*>(FileBytes.GetData() + 16), StateSize);
		const FString StateJson(ConvertedState.Length(), ConvertedState.Get());

		TSharedPtr<FJsonObject> StateObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StateJson);
		if (!FJsonSerializer::Deserialize(Reader, StateObject) || !StateObject.IsValid())
		{
			OutError = LOCTEXT("JsonParseFailed", "Could not parse fSpy JSON state.");
			return false;
		}

		const TSharedPtr<FJsonObject>* CameraParameters = nullptr;
		if (!StateObject->TryGetObjectField(TEXT("cameraParameters"), CameraParameters) || !CameraParameters->IsValid())
		{
			OutError = LOCTEXT("MissingCameraParameters", "The fSpy project has no camera parameters.");
			return false;
		}

		const TSharedPtr<FJsonObject>* PrincipalPointObject = nullptr;
		if (!(*CameraParameters)->TryGetObjectField(TEXT("principalPoint"), PrincipalPointObject))
		{
			OutError = LOCTEXT("MissingPrincipalPoint", "The fSpy project has no principalPoint.");
			return false;
		}

		double PrincipalX = 0.0;
		double PrincipalY = 0.0;
		if (!TryGetNumber(*PrincipalPointObject, TEXT("x"), PrincipalX, OutError) ||
			!TryGetNumber(*PrincipalPointObject, TEXT("y"), PrincipalY, OutError) ||
			!TryGetNumber(*CameraParameters, TEXT("horizontalFieldOfView"), OutProject.HorizontalFovRadians, OutError))
		{
			return false;
		}
		(*CameraParameters)->TryGetNumberField(TEXT("verticalFieldOfView"), OutProject.VerticalFovRadians);
		(*CameraParameters)->TryGetNumberField(TEXT("relativeFocalLength"), OutProject.RelativeFocalLength);

		OutProject.ImageWidth = (*CameraParameters)->GetIntegerField(TEXT("imageWidth"));
		OutProject.ImageHeight = (*CameraParameters)->GetIntegerField(TEXT("imageHeight"));
		if (OutProject.ImageWidth <= 0 || OutProject.ImageHeight <= 0)
		{
			OutError = LOCTEXT("InvalidImageResolution", "The fSpy image resolution is invalid.");
			return false;
		}

		OutProject.PrincipalPoint = FVector2D(PrincipalX, PrincipalY);
		if (!ParseCameraMatrix(*CameraParameters, OutProject, OutError))
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* CalibrationSettings = nullptr;
		if (StateObject->TryGetObjectField(TEXT("calibrationSettingsBase"), CalibrationSettings) && CalibrationSettings->IsValid())
		{
			(*CalibrationSettings)->TryGetStringField(TEXT("referenceDistanceUnit"), OutProject.ReferenceDistanceUnit);

			const TSharedPtr<FJsonObject>* CameraData = nullptr;
			if ((*CalibrationSettings)->TryGetObjectField(TEXT("cameraData"), CameraData) && CameraData->IsValid())
			{
				const TSharedPtr<FJsonObject>* PresetData = nullptr;
				if ((*CameraData)->TryGetObjectField(TEXT("presetData"), PresetData) && PresetData->IsValid())
				{
					(*PresetData)->TryGetNumberField(TEXT("sensorWidth"), OutProject.SensorWidthMm);
					(*PresetData)->TryGetNumberField(TEXT("sensorHeight"), OutProject.SensorHeightMm);
				}

				if (OutProject.SensorWidthMm <= 0.0 || OutProject.SensorHeightMm <= 0.0)
				{
					(*CameraData)->TryGetNumberField(TEXT("customSensorWidth"), OutProject.SensorWidthMm);
					(*CameraData)->TryGetNumberField(TEXT("customSensorHeight"), OutProject.SensorHeightMm);
				}
			}
		}

		OutProject.SourcePath = FilePath;
		OutProject.BaseName = SanitizeAssetName(FPaths::GetBaseFilename(FilePath));
		OutProject.ImageData.Append(FileBytes.GetData() + 16 + StateSize, ImageSize);
		if (!DetectImageType(OutProject.ImageData, OutProject.ImageType))
		{
			OutError = LOCTEXT("UnsupportedImage", "The embedded fSpy reference image is not a supported JPEG or PNG image.");
			return false;
		}

		return true;
	}

	FTransform BuildUnrealCameraTransform(const FFSpyProject& Project)
	{
		const double UnitScale = UnitToCentimeters(Project.ReferenceDistanceUnit);
		const FVector BlenderRight(
			Project.CameraMatrix[0][0],
			Project.CameraMatrix[1][0],
			Project.CameraMatrix[2][0]);
		const FVector BlenderUp(
			Project.CameraMatrix[0][1],
			Project.CameraMatrix[1][1],
			Project.CameraMatrix[2][1]);
		const FVector BlenderBack(
			Project.CameraMatrix[0][2],
			Project.CameraMatrix[1][2],
			Project.CameraMatrix[2][2]);
		const FVector BlenderLocation(
			Project.CameraMatrix[0][3],
			Project.CameraMatrix[1][3],
			Project.CameraMatrix[2][3]);

		const FVector Forward = BlenderVectorToUnreal(-BlenderBack).GetSafeNormal();
		const FVector Up = BlenderVectorToUnreal(BlenderUp).GetSafeNormal();
		const FRotator Rotation = FRotationMatrix::MakeFromXZ(Forward, Up).Rotator();
		const FVector Location = BlenderVectorToUnreal(BlenderLocation, UnitScale);
		return FTransform(Rotation, Location);
	}

	UObject* CreateUniqueAsset(
		const FString& BasePackageName,
		UClass* AssetClass,
		UFactory* Factory,
		UObject* Context = nullptr)
	{
		FString PackageName;
		FString AssetName;
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		AssetToolsModule.Get().CreateUniqueAssetName(BasePackageName, TEXT(""), PackageName, AssetName);

		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			return nullptr;
		}

		UObject* NewAsset = Factory->FactoryCreateNew(
			AssetClass,
			Package,
			*AssetName,
			RF_Public | RF_Standalone | RF_Transactional,
			Context,
			GWarn);

		if (NewAsset)
		{
			FAssetRegistryModule::AssetCreated(NewAsset);
			Package->MarkPackageDirty();
		}
		return NewAsset;
	}

	UTexture2D* ImportReferenceTexture(const FFSpyProject& Project)
	{
		const FString BasePackageName = FString::Printf(TEXT("/Game/FSpyImports/%s_Reference"), *Project.BaseName);
		FString PackageName;
		FString AssetName;
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		AssetToolsModule.Get().CreateUniqueAssetName(BasePackageName, TEXT(""), PackageName, AssetName);

		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			return nullptr;
		}

		UTextureFactory* TextureFactory = NewObject<UTextureFactory>();
		TextureFactory->AddToRoot();
		TextureFactory->bCreateMaterial = false;
		TextureFactory->bDeferCompression = true;
		TextureFactory->MipGenSettings = TMGS_NoMipmaps;
		TextureFactory->SuppressImportOverwriteDialog();

		const uint8* Buffer = Project.ImageData.GetData();
		const uint8* BufferEnd = Buffer + Project.ImageData.Num();
		UObject* TextureObject = TextureFactory->FactoryCreateBinary(
			UTexture2D::StaticClass(),
			Package,
			*AssetName,
			RF_Public | RF_Standalone | RF_Transactional,
			nullptr,
			*Project.ImageType,
			Buffer,
			BufferEnd,
			GWarn);
		TextureFactory->RemoveFromRoot();

		UTexture2D* Texture = Cast<UTexture2D>(TextureObject);
		if (Texture)
		{
			Texture->Modify();
			Texture->SRGB = false;
			Texture->LODGroup = TEXTUREGROUP_UI;
			Texture->MipGenSettings = TMGS_NoMipmaps;
			Texture->PostEditChange();
			FAssetRegistryModule::AssetCreated(Texture);
			Package->MarkPackageDirty();
		}
		return Texture;
	}

	UMaterial* CreateReferencePostProcessMaterial(const FFSpyProject& Project, UTexture2D* Texture)
	{
		if (!Texture)
		{
			return nullptr;
		}

		UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
		UMaterial* Material = Cast<UMaterial>(CreateUniqueAsset(
			FString::Printf(TEXT("/Game/FSpyImports/%s_Reference_PP"), *Project.BaseName),
			UMaterial::StaticClass(),
			Factory));

		if (!Material)
		{
			return nullptr;
		}

		Material->Modify();
		Material->MaterialDomain = MD_PostProcess;
		Material->BlendMode = BLEND_Translucent;
		Material->BlendableOutputAlpha = true;
		Material->BlendableLocation = BL_SceneColorAfterTonemapping;

		UMaterialExpressionTextureSampleParameter2D* TextureExpression =
			NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
		TextureExpression->MaterialExpressionEditorX = -420;
		TextureExpression->MaterialExpressionEditorY = -80;
		TextureExpression->ParameterName = ReferenceImageParameterName;
		TextureExpression->Texture = Texture;
		TextureExpression->AutoSetSampleType();
		Material->GetExpressionCollection().AddExpression(TextureExpression);

		UMaterialExpressionScalarParameter* OpacityExpression =
			NewObject<UMaterialExpressionScalarParameter>(Material);
		OpacityExpression->MaterialExpressionEditorX = -420;
		OpacityExpression->MaterialExpressionEditorY = 160;
		OpacityExpression->ParameterName = ReferenceOpacityParameterName;
		OpacityExpression->DefaultValue = DefaultReferenceOpacity;
		OpacityExpression->SliderMin = 0.0f;
		OpacityExpression->SliderMax = 1.0f;
		Material->GetExpressionCollection().AddExpression(OpacityExpression);

		if (UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData())
		{
			const TArray<FExpressionOutput> TextureOutputs = TextureExpression->GetOutputs();
			if (TextureOutputs.Num() > 0)
			{
				const FExpressionOutput& Output = TextureOutputs[0];
				MaterialEditorOnly->EmissiveColor.Expression = TextureExpression;
				MaterialEditorOnly->EmissiveColor.Mask = Output.Mask;
				MaterialEditorOnly->EmissiveColor.MaskR = Output.MaskR;
				MaterialEditorOnly->EmissiveColor.MaskG = Output.MaskG;
				MaterialEditorOnly->EmissiveColor.MaskB = Output.MaskB;
				MaterialEditorOnly->EmissiveColor.MaskA = Output.MaskA;
			}

			MaterialEditorOnly->Opacity.Expression = OpacityExpression;
			MaterialEditorOnly->Opacity.Mask = 1;
			MaterialEditorOnly->Opacity.MaskR = 1;
			MaterialEditorOnly->Opacity.MaskG = 0;
			MaterialEditorOnly->Opacity.MaskB = 0;
			MaterialEditorOnly->Opacity.MaskA = 0;
		}

		Material->PostEditChange();
		Material->MarkPackageDirty();
		return Material;
	}

	UMaterialInstanceConstant* CreateReferenceMaterialInstance(
		const FFSpyProject& Project,
		UMaterial* ParentMaterial,
		UTexture2D* Texture)
	{
		if (!ParentMaterial || !Texture)
		{
			return nullptr;
		}

		UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
		Factory->InitialParent = ParentMaterial;
		UMaterialInstanceConstant* Material = Cast<UMaterialInstanceConstant>(CreateUniqueAsset(
			FString::Printf(TEXT("/Game/FSpyImports/%s_Reference_PP_MI"), *Project.BaseName),
			UMaterialInstanceConstant::StaticClass(),
			Factory));
		if (Material)
		{
			Material->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(ReferenceImageParameterName), Texture);
			Material->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ReferenceOpacityParameterName), DefaultReferenceOpacity);
			Material->PostEditChange();
			Material->MarkPackageDirty();
		}
		return Material;
	}

	void ConfigureCineCamera(const FFSpyProject& Project, UCineCameraComponent* CameraComponent)
	{
		if (!CameraComponent)
		{
			return;
		}

		const double AspectRatio = static_cast<double>(Project.ImageWidth) / static_cast<double>(Project.ImageHeight);
		const double SensorWidth = Project.SensorWidthMm > 0.0 ? Project.SensorWidthMm : DefaultFilmbackWidthMm;
		const double SensorHeight = Project.SensorHeightMm > 0.0 ? Project.SensorHeightMm : DefaultFilmbackHeightMm;

		FCameraFilmbackSettings Filmback;
		Filmback.SensorWidth = static_cast<float>(SensorWidth);
		Filmback.SensorHeight = static_cast<float>(SensorHeight);
		Filmback.SensorHorizontalOffset = static_cast<float>(-0.5 * Project.PrincipalPoint.X * SensorWidth);
		Filmback.SensorVerticalOffset = static_cast<float>(-0.5 * Project.PrincipalPoint.Y * SensorHeight);
		Filmback.RecalcSensorAspectRatio();

		FPlateCropSettings CropSettings;
		CropSettings.AspectRatio = static_cast<float>(AspectRatio);

		CameraComponent->Modify();
		CameraComponent->SetFilmback(Filmback);
		CameraComponent->SetCropSettings(CropSettings);
		CameraComponent->SetFieldOfView(FMath::RadiansToDegrees(Project.HorizontalFovRadians));
		CameraComponent->bConstrainAspectRatio = true;
		CameraComponent->AspectRatio = static_cast<float>(AspectRatio);
		CameraComponent->PostProcessBlendWeight = 1.0f;
	}

	void AddReferencePostProcessMaterial(
		UCineCameraComponent* CameraComponent,
		UMaterialInstanceConstant* MaterialInstance)
	{
		if (!CameraComponent || !MaterialInstance)
		{
			return;
		}

		FPostProcessSettings& PostProcessSettings = CameraComponent->PostProcessSettings;
		PostProcessSettings.WeightedBlendables.Array.RemoveAll(
			[](const FWeightedBlendable& Blendable)
			{
				return Blendable.Object && Blendable.Object->GetName().Contains(TEXT("_Reference_PP_MI"));
			});
		PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, MaterialInstance));
	}

	void ShowNotification(const FText& Message, const SNotificationItem::ECompletionState State)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 5.0f;
		Info.bFireAndForget = true;
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if (Notification.IsValid())
		{
			Notification->SetCompletionState(State);
		}
	}
}

void Ffspy_importerModule::StartupModule()
{
	ToolMenusStartupHandle = UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &Ffspy_importerModule::RegisterMenus));
}

void Ffspy_importerModule::ShutdownModule()
{
	if (ToolMenusStartupHandle.IsValid())
	{
		UToolMenus::UnRegisterStartupCallback(ToolMenusStartupHandle);
		ToolMenusStartupHandle.Reset();
	}
	UToolMenus::UnregisterOwner(this);
}

void Ffspy_importerModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* FileMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.File"));
	if (!FileMenu)
	{
		return;
	}

	FToolMenuSection& Section = FileMenu->FindOrAddSection(TEXT("FileActors"));
	FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
		TEXT("ImportFSpyCamera"),
		LocalizedText(TEXT("导入 fSpy 摄像机..."), TEXT("Import fSpy Camera...")),
		LocalizedText(
			TEXT("导入 fSpy 摄像机反求文件，并创建带后期参考图的 Cine Camera。"),
			TEXT("Import an fSpy camera solve file and create a cine camera with a post-process reference image.")),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.CineCameraActor")),
		FUIAction(FExecuteAction::CreateRaw(this, &Ffspy_importerModule::ImportFSpyFromDialog)));
	Entry.InsertPosition = FToolMenuInsert(TEXT("ExportSelected"), EToolMenuInsertType::After);
	Section.AddEntry(Entry);
}

void Ffspy_importerModule::ImportFSpyFromDialog()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	void* ParentWindowHandle = nullptr;
	if (FSlateApplication::IsInitialized())
	{
		const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr);
		if (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}
	}

	TArray<FString> SelectedFiles;
	const bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Import fSpy Camera"),
		FPaths::GetPath(FPaths::ProjectDir()),
		TEXT(""),
		TEXT("fSpy Project (*.fspy)|*.fspy"),
		EFileDialogFlags::None,
		SelectedFiles);

	if (bOpened && SelectedFiles.Num() > 0)
	{
		ImportFSpyFile(SelectedFiles[0]);
	}
}

bool Ffspy_importerModule::ImportFSpyFile(const FString& FilePath) const
{
	FFSpyProject Project;
	FText Error;
	if (!ParseFSpyProject(FilePath, Project, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("ImportFailedFormat", "fSpy import failed:\n{0}"), Error));
		return false;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoEditorWorld", "Could not find an editor world to import into."));
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("ImportFSpyTransaction", "Import fSpy Camera"));
	World->Modify();

	UTexture2D* Texture = ImportReferenceTexture(Project);
	UMaterial* ParentMaterial = CreateReferencePostProcessMaterial(Project, Texture);
	UMaterialInstanceConstant* MaterialInstance = CreateReferenceMaterialInstance(Project, ParentMaterial, Texture);

	const FString CameraBaseName = FString::Printf(TEXT("CineCamera_Ref_%s"), *Project.BaseName);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(World->PersistentLevel, ACineCameraActor::StaticClass(), FName(*CameraBaseName));
	SpawnParameters.OverrideLevel = World->PersistentLevel;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACineCameraActor* CameraActor = World->SpawnActor<ACineCameraActor>(
		ACineCameraActor::StaticClass(),
		BuildUnrealCameraTransform(Project),
		SpawnParameters);
	if (!CameraActor)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SpawnCameraFailed", "Could not create the fSpy cine camera actor."));
		return false;
	}

	CameraActor->Modify();
	CameraActor->SetActorLabel(CameraBaseName);
	CameraActor->Tags.AddUnique(FName(TEXT("fspy_importer")));
	CameraActor->Tags.AddUnique(FName(*FString::Printf(TEXT("fspy_source:%s"), *FPaths::GetCleanFilename(Project.SourcePath))));

	ConfigureCineCamera(Project, CameraActor->GetCineCameraComponent());
	AddReferencePostProcessMaterial(CameraActor->GetCineCameraComponent(), MaterialInstance);

	GEditor->SelectNone(false, true);
	GEditor->SelectActor(CameraActor, true, true);
	GEditor->RedrawAllViewports();
	ULevel::LevelDirtiedEvent.Broadcast();

	ShowNotification(
		FText::Format(LOCTEXT("ImportFinished", "Imported fSpy camera '{0}'."), FText::FromString(Project.BaseName)),
		SNotificationItem::CS_Success);
	return true;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(Ffspy_importerModule, fspy_importer)
