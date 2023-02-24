// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "AudioCaptureDeviceInterface.h"
#include "IMillicastSource.h"
#include "RtcCodecsConstants.h"
#include "StreamMediaSource.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ObjectMacros.h"
#include "Sound/SoundSubmix.h"

#include "MillicastPublisherSource.generated.h"

USTRUCT(BlueprintType)
struct FMillicastLayeredTexture
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	const UTexture* Texture = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector2D Position;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector2D Size;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFrameRendered, UCanvas*, Canvas);

/**
 * Media source description for Millicast Publisher.
 */
UCLASS(BlueprintType, hideCategories=(Platforms,Object),
       META = (DisplayName = "Millicast Publisher Source"))
class MILLICASTPUBLISHER_API UMillicastPublisherSource : public UStreamMediaSource
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnFrameRendered OnFrameRendered;

	/* Can be set if no layered textures are provided to expose a canvas anyway */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Video)
	bool bSupportCustomDrawCanvas = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Video)
	TArray<FMillicastLayeredTexture> LayeredTextures;


public:
	UMillicastPublisherSource(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "Initialize"))
	void Initialize(const FString& InPublishingToken, const FString& InStreamName, const FString& InSourceId, const FString& InStreamUrl = "https://director.millicast.com/api/director/publish");

	/* Required for watermark feature */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", meta = (WorldContext = "WorldContextObject"))
	void RegisterWorldContext(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher")
	void UnregisterWorldContext();
	/* Watermark feature over */


	/** The Millicast Stream name. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Stream, AssetRegistrySearchable)
	FString StreamName;

	/** Publishing token. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Stream, AssetRegistrySearchable)
	FString PublishingToken;

	/** Source id to use Millicast multisource feature */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Stream, AssetRegistrySearchable)
	FString SourceId;

	/** Whether we should capture video or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Video, AssetRegistrySearchable)
	bool CaptureVideo = true;

	/** Publish video from this render target */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Video, AssetRegistrySearchable)
	UTextureRenderTarget2D* RenderTarget = nullptr;

	/** Whether we should capture game audio or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Audio, AssetRegistrySearchable)
	bool CaptureAudio = true;

	/** Which audio capturer to use */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Audio, AssetRegistrySearchable)
	TEnumAsByte<AudioCapturerType> AudioCaptureType;

	/** Audio submix */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Audio, AssetRegistrySearchable)
	USoundSubmix* Submix;

	/** Capture device index  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Audio, AssetRegistrySearchable)
	int32 CaptureDeviceIndex; // UMETA(ArrayClamp = "CaptureDevicesName");

	/** Apply a volume multiplier for the recorded data in dB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Audio, AssetRegistrySearchable)
	float VolumeMultiplier = 20.f;

public:
	/** Mute the video stream */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "MuteVideo"))
	void MuteVideo(bool Muted);

	/** Set a new render target while publishing */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "ChangeRenderTarget"))
	void ChangeRenderTarget(UTextureRenderTarget2D * InRenderTarget);

public:
	/** Mute the audio stream */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "MuteAudio"))
	void MuteAudio(bool Muted);

	/** Set the audio capture device by its id */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "SetAudioDeviceById"))
	void SetAudioDeviceById(const FString& Id);

	/** Set the audio capture device by its name */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "SetAudioDeviceByName"))
	void SetAudioDeviceByName(const FString& Name);

	/** Apply a volume multiplier for the recorded data in dB */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "SetVolumeMultiplier"))
	void SetVolumeMultiplier(float f);

public:
	//~ IMediaOptions interface

	FString GetMediaOption(const FName& Key, const FString& DefaultValue) const override;
	bool HasMediaOption(const FName& Key) const override;

public:
	//~ UMediaSource interface
	FString GetUrl() const override;
	bool Validate() const override;

public:
	/**
	   Called before destroying the object.  This is called immediately upon deciding to destroy the object,
	   to allow the object to begin an asynchronous cleanup process.
	 */
	void BeginDestroy() override;

public:
	//~ UObject interface
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& InPropertyChangedEvent) override;
#endif //WITH_EDITOR
	//~ End UObject interface

public:
	/** 
	* Create a capturer from the configuration set for video and audio and start the capture
	* You can set a callback to get the track returns by the capturer when starting the capture
	*/
	void StartCapture(TFunction<void(IMillicastSource::FStreamTrackInterface)> Callback = nullptr);

	/** Stop the capture and destroy all capturers */
	void StopCapture();

private:
	void TryInitRenderTargetCanvas();

private:
	TSharedPtr<IMillicastVideoSource> VideoSource;
	TSharedPtr<IMillicastAudioSource> AudioSource;

	// Custom DrawCanvas
	UPROPERTY()
	UCanvas* RenderTargetCanvas = nullptr;

	FDrawToRenderTargetContext RenderTargetCanvasCtx;
	bool bRenderTargetInitialized = false; // TODO [RW] atomic enum with 3 stages for absolute state management would be best, but won't touch the bool now unless it becomes problematic

	UObject* WorldContext = nullptr;
	// Custom DrawCanvas End
};
