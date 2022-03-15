// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>

#include <Components/ActorComponent.h>
#include "MillicastPublisherSource.h"

#include "MillicastPublisherComponent.generated.h"

// Forward declarations
class IWebSocket;
class FWebRTCPeerConnection;

// Event declaration
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FMillicastPublisherComponentAuthenticated, UMillicastPublisherComponent, OnAuthenticated);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FMillicastPublisherComponentAuthenticationFailure, UMillicastPublisherComponent, OnAuthenticationFailure, int, Code, const FString&, Msg);

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FMillicastPublisherComponentPublishing, UMillicastPublisherComponent, OnPublishing);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FMillicastPublisherComponentPublishingError, UMillicastPublisherComponent, OnPublishingError, const FString&, ErrorMsg);

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FMillicastPublisherComponentActive, UMillicastPublisherComponent, OnActive);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FMillicastPublisherComponentInactive, UMillicastPublisherComponent, OnInactive);

/**
	A component used to receive audio, video from a Millicast feed.
*/
UCLASS(BlueprintType, Blueprintable, Category = "MillicastPublisher",
	   META = (DisplayName = "Millicast Publisher Component", BlueprintSpawnableComponent))
class MILLICASTPUBLISHER_API UMillicastPublisherComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

private:
	TMap <FString, TFunction<void()>> EventBroadcaster;

	/** The Millicast Media Source representing the configuration of the network source */
	UPROPERTY(EditDefaultsOnly, Category = "Properties",
			  META = (DisplayName = "Millicast Publisher Source", AllowPrivateAccess = true))
	UMillicastPublisherSource* MillicastMediaSource = nullptr;

public:
	~UMillicastPublisherComponent();

	/**
		Initialize this component with the media source required for receiving NDI audio, video, and metadata.
		Returns false, if the MediaSource is already been set. This is usually the case when this component is
		initialized in Blueprints.
	*/
	bool Initialize(UMillicastPublisherSource* InMediaSource = nullptr);

	/**
		Begin publishing audio/video to Millicast using the info in the Publisher source object
		and calling the Publisher api
	*/
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "Publish"))
	bool Publish();

	/**
		Begin publishing audio/video to Millicast using the websocket URL and JSON Web Token
	*/
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "PublishWithWsAndJwt"))
	bool PublishWithWsAndJwt(const FString& WebSocketUrl, const FString& Jwt);

	/**
		Attempts to stop receiving video from the Millicast feed
	*/
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "Unpublish"))
	void UnPublish();

	/**
	* UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "Unsubscribe"))
	*/
	bool IsPublishing() const;

public:
	/** Called when the response from the Publisher api is successfull */
	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FMillicastPublisherComponentAuthenticated OnAuthenticated;

	/** Called when the response from the Publisher api is an error */
	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FMillicastPublisherComponentAuthenticationFailure OnAuthenticationFailure;

	/** Called when the publisher is publishing to Millicast */
	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FMillicastPublisherComponentPublishing OnPublishing;

	/** Called when the publisher is failed to publish to Millicast */
	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FMillicastPublisherComponentPublishingError OnPublishingError;

	/** Called when the first viewer starts viewing the stream being published by this publisher */
	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FMillicastPublisherComponentActive OnActive;

	/** Called when the last viewer quit viewing the stream being published by this publisher */
	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FMillicastPublisherComponentInactive OnInactive;

private:
	/** Websocket Connection */
	bool StartWebSocketConnection(const FString& url, const FString& jwt);
	void OnConnected();
	void OnConnectionError(const FString& Error);
	void OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void OnMessage(const FString& Msg);

	/** Media Tracks */
	void CaptureAndAddTracks();

	/** Create the peerconnection and starts subscribing*/
	bool PublishToMillicast();

	/** WebSocket Connection */
	TSharedPtr<IWebSocket> WS;
	FDelegateHandle OnConnectedHandle;
	FDelegateHandle OnConnectionErrorHandle;
	FDelegateHandle OnClosedHandle;
	FDelegateHandle OnMessageHandle;

	FWebRTCPeerConnection* PeerConnection;
	bool bIsPublishing;
};
