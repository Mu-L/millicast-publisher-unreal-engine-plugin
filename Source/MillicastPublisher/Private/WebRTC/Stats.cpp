// Copyright Millicast 2022. All Rights Reserved.

#include "Stats.h"
#include "WebRTC/PeerConnection.h"
#include "MillicastPublisherPrivate.h"
#include "api/stats/rtcstats_objects.h"
#include "util.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMillicastPublisherStats, Log, All);
DEFINE_LOG_CATEGORY(LogMillicastPublisherStats);

CSV_DEFINE_CATEGORY(Millicast_Publisher, false);

namespace MillicastPublisher
{

FPublisherStats FPublisherStats::Instance;

double CalcEMA(double PrevAvg, int NumSamples, double Value)
{
	const double Mult = 2.0 / (NumSamples + 1.0);
	const double Result = (Value - PrevAvg) * Mult + PrevAvg;
	return Result;
}

void FPublisherStats::TextureReadbackStart()
{
	TextureReadbackStartTime = FPlatformTime::Cycles64();
}

void FPublisherStats::TextureReadbackEnd()
{
	uint64 ThisTime = FPlatformTime::Cycles64();
	double SecondsDelta = FPlatformTime::ToSeconds64(ThisTime - TextureReadbackStartTime);
	TextureReadbacks = FPlatformMath::Min(Frames + 1, 60);
	TextureReadbackAvg = CalcEMA(TextureReadbackAvg, TextureReadbacks, SecondsDelta);
}

void FPublisherStats::FrameRendered()
{
	uint64 ThisTime = FPlatformTime::Cycles64();
	double SecondsDelta = FPlatformTime::ToSeconds64(ThisTime - LastFrameRendered);
	double FPS = 1.0 / SecondsDelta;
	Frames = FPlatformMath::Min(Frames + 1, 60);
	SubmitFPS = CalcEMA(SubmitFPS, Frames, FPS);
	LastFrameRendered = ThisTime;
}

void FPublisherStats::SetEncoderStats(double LatencyMs, double BitrateMbps, int QP)
{
	EncoderStatSamples = FPlatformMath::Min(EncoderStatSamples + 1, 60);
	EncoderLatencyMs = CalcEMA(EncoderLatencyMs, EncoderStatSamples, LatencyMs);
	EncoderBitrateMbps = CalcEMA(EncoderBitrateMbps, EncoderStatSamples, BitrateMbps);
	EncoderQP = CalcEMA(EncoderQP, EncoderStatSamples, QP);
}

void FPublisherStats::Tick(float DeltaTime)
{
	if (!GEngine)
	{
		return;
	}

	if (!bRegisterEngineStats)
	{
		RegisterEngineHooks();
	}
}

bool FPublisherStats::OnToggleStats(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	return true;
}

template<typename T>
std::tuple<T, FString> GetInUnit(T Value, const FString& Unit)
{
	constexpr auto MEGA = 1'000'000.;
	constexpr auto KILO = 1'000.;

	if (Value >= MEGA)
	{
		return { Value / MEGA, TEXT("M") + Unit };
	}

	if (Value >= KILO)
	{
		return { Value / KILO, TEXT("K") + Unit };
	}

	return { Value, Unit };
}

int32 FPublisherStats::OnRenderStats(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	int MessageKey = 100;

	int i = 0;
	for (FRTCStatsCollector* Collector : StatsCollectors)
	{
		Collector->Poll();

		if (Collector->QualityLimitationReason.IsSet())
		{
			GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Quality limitation reason = %s"), *Collector->QualityLimitationReason.GetValue()), true);
		}
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Video Frame dropped = %d"), Collector->QualityLimitationResolutionChange), true);

		if (Collector->ContentType.IsSet())
		{
			GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Content Type = %s"), *Collector->ContentType.GetValue()), true);
		}
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Video Frame dropped = %d"), Collector->FramesDropped), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Video Packet Retransmitted = %d"), Collector->VideoPacketRetransmitted), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Audio Packet Retransmitted = %d"), Collector->AudioPacketRetransmitted), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Total Encode Time = %.2f s"), Collector->TotalEncodeTime), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Avg Encode Time = %.2f ms"), Collector->AvgEncodeTime), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("RTT = %.2f ms"), Collector->Rtt), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Video resolution = %dx%d"), Collector->Width, Collector->Height), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("FPS = %d"), Collector->FramePerSecond), true);

		auto [VideoBitrate, VideoBitrateUnit] = GetInUnit(Collector->VideoBitrate, TEXT("bps"));
		auto [AudioBitrate, AudioBitrateUnit] = GetInUnit(Collector->AudioBitrate, TEXT("bps"));

		auto [VideoBytes, VideoBytesUnit] = GetInUnit(Collector->VideoTotalSent, TEXT("B"));
		auto [AudioBytes, AudioBytesUnit] = GetInUnit(Collector->AudioTotalSent, TEXT("B"));

		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Video Bitrate = %.2f %s"), VideoBitrate, *VideoBitrateUnit), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Audio Bitrate = %.2f %s"), AudioBitrate, *AudioBitrateUnit), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Video Total Sent = %lld %s"), VideoBytes, *VideoBytesUnit), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Audio Total Sent = %lld %s"), AudioBytes, *AudioBytesUnit), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Codecs = %s,%s"), *Collector->VideoCodec, *Collector->AudioCodec), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Cluster = %s"), *Collector->Cluster()), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Server = %s"), *Collector->Server()), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Stats Collector %d"), i), true);

		CSV_CUSTOM_STAT(Millicast_Publisher, Rtt, (float)Collector->Rtt, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, Width, (int)Collector->Width, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, Height, (int)Collector->Height, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, FramePerSecond, (int)Collector->FramePerSecond, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, VideoBitrate, (float)Collector->VideoBitrate, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, AudioBitrate, (float)Collector->AudioBitrate, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, VideoTotalSent, (int)Collector->VideoTotalSent, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, AudioTotalSent, (int)Collector->AudioTotalSent, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, VideoPacketRetransmitted, Collector->VideoPacketRetransmitted, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, AudioPacketRetransmitted, Collector->AudioPacketRetransmitted, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, TotalEncodedFrames, (float)Collector->TotalEncodedFrames, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, TotalEncodeTime, (float)Collector->TotalEncodeTime, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, AvgEncodeTime, (float)Collector->AvgEncodeTime, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, FramesDropped, Collector->FramesDropped, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, QualityLimitationResolutionChange, (int)Collector->QualityLimitationResolutionChange, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, Timestamp, (float)Collector->Timestamp, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, AudioNackCount, Collector->AudioNackCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Millicast_Publisher, VideoNackCount, Collector->VideoNackCount, ECsvCustomStatOp::Set);

		++i;
	}

	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("SubmitFPS = %.2f"), SubmitFPS), true);
	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("TextureReadTime = %.6f s"), TextureReadbackAvg), true);
	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Encode Latency = %.2f ms"), EncoderLatencyMs), true);
	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Encode Bitrate = %.2f Mbps"), EncoderBitrateMbps), true);
	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Encode QP = %.0f"), EncoderQP), true);

	return Y;
}

void FPublisherStats::RegisterStatsCollector(FRTCStatsCollector* Connection)
{
	StatsCollectors.Add(Connection);
}

void FPublisherStats::UnregisterStatsCollector(FRTCStatsCollector* Connection)
{
	StatsCollectors.Remove(Connection);
}

void FPublisherStats::RegisterEngineHooks()
{
	const FName StatName("STAT_Millicast_Publisher");
	const FName StatCategory("STATCAT_Millicast_Publisher");
	const FText StatDescription(FText::FromString("Millicast Publisher streaming stats."));
	UEngine::FEngineStatRender RenderStatFunc = UEngine::FEngineStatRender::CreateRaw(this, &FPublisherStats::OnRenderStats);
	UEngine::FEngineStatToggle ToggleStatFunc = UEngine::FEngineStatToggle::CreateRaw(this, &FPublisherStats::OnToggleStats);
	GEngine->AddEngineStat(StatName, StatCategory, StatDescription, RenderStatFunc, ToggleStatFunc, false);

	bRegisterEngineStats = true;
}

/*
 * FRTCStatsCollector
 */

FRTCStatsCollector::FRTCStatsCollector(class FWebRTCPeerConnection* InPeerConnection)
	: PeerConnection(InPeerConnection)
{
	FPublisherStats::Get().RegisterStatsCollector(this);

	Rtt = 0;
	Width = 0;
	Height = 0;
	FramePerSecond = 0;
	VideoBitrate = 0;
	AudioBitrate = 0;
	VideoTotalSent = 0;
	AudioTotalSent = 0;
	VideoPacketRetransmitted = 0;
	AudioPacketRetransmitted = 0;
	FramesDropped = 0;
	VideoNackCount = 0;
	AudioNackCount = 0;
	TotalEncodedFrames = 0;
	AvgEncodeTime = 0;
	TotalEncodeTime = 0;

	LastAudioStatTimestamp = 0;
	LastVideoStatTimestamp = 0;
	Timestamp = 0;
}

FRTCStatsCollector::~FRTCStatsCollector()
{
	FPublisherStats::Get().UnregisterStatsCollector(this);
}

void FRTCStatsCollector::Poll()
{
	PeerConnection->PollStats();
}

template<typename StatMember, typename T>
auto ValueOrDefault(StatMember&& Stat, T Default) -> typename std::remove_reference<decltype(*Stat)>::type
{
	return ((Stat.is_defined()) ? *Stat : Default);
}

template<typename T>
auto GetOptionalStat(const webrtc::RTCStatsMember<T>& Member)
{
	if constexpr (std::is_same_v<T, std::string>)
	{
		return ((Member.is_defined()) ? ToString(Member->c_str()) : TOptional<FString>{});
	}
	else
	{
		return ((Member.is_defined()) ? *Member : TOptional<T>{});
	}
}

void FRTCStatsCollector::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& Report)
{
	constexpr uint32_t NUM_US = 1'000'000; // number of microseconds in 1 second

	double NewTotalEncodedFrames = 0;
	double NewTotalEncodeTime = 0;

	auto get_codec = [](auto&& Report, auto&& OutboundStat) -> FString {
		auto CodecStats = Report->GetStatsOfType<webrtc::RTCCodecStats>();

		auto it = std::find_if(CodecStats.begin(), CodecStats.end(),
			[&OutboundStat](auto&& e) { return e->id() == *OutboundStat.codec_id; });

		if (it != CodecStats.end())
		{
			return ToString(*(*it)->mime_type);
		}
		
		return FString();
	};

	for (const webrtc::RTCStats& Stats : *Report)
	{
		const FString StatsType = FString(Stats.type());
		const FString StatsId = FString(Stats.id().c_str());

		//UE_LOG(LogMillicastStats, Log, TEXT("Type: %s Id: %s"), *StatsType, *StatsId);

		if (StatsType == webrtc::RTCOutboundRTPStreamStats::kType)
		{
			auto OutboundStat = Stats.cast_to<webrtc::RTCOutboundRTPStreamStats>();

			if (*OutboundStat.kind == webrtc::RTCMediaStreamTrackKind::kVideo)
			{
				auto lastByteCount = VideoTotalSent;
				auto timestamp = Stats.timestamp_us();

				Width = ValueOrDefault(OutboundStat.frame_width, 0);
				Height = ValueOrDefault(OutboundStat.frame_height, 0);
				FramePerSecond = ValueOrDefault(OutboundStat.frames_per_second, 0);
				// VideoTargetBitrate = ValueOrDefault(OutboundStat.target_bitrate, 0);
				VideoTotalSent = ValueOrDefault(OutboundStat.bytes_sent, 0);
				NewTotalEncodeTime = ValueOrDefault(OutboundStat.total_encode_time, 0);
				NewTotalEncodedFrames = ValueOrDefault(OutboundStat.frames_encoded, 0);
				VideoNackCount = ValueOrDefault(OutboundStat.nack_count, 0);
				VideoPacketRetransmitted = ValueOrDefault(OutboundStat.retransmitted_packets_sent, 0);

				QualityLimitationReason = GetOptionalStat(OutboundStat.quality_limitation_reason);
				ContentType = GetOptionalStat(OutboundStat.content_type);
				QualityLimitationResolutionChange = ValueOrDefault(OutboundStat.quality_limitation_resolution_changes, 0);

				if (LastVideoStatTimestamp != 0 && VideoTotalSent!= lastByteCount)
				{
					VideoBitrate = (VideoTotalSent- lastByteCount) * NUM_US * 8. / (timestamp - LastVideoStatTimestamp);
				}

				LastVideoStatTimestamp = timestamp;

				VideoCodec = get_codec(Report, OutboundStat);
			}
			else
			{
				auto lastByteCount = AudioTotalSent;
				auto timestamp = Stats.timestamp_us();

				AudioTotalSent = ValueOrDefault(OutboundStat.bytes_sent, 0);
				AudioNackCount = ValueOrDefault(OutboundStat.nack_count, 0);
				// AudioTargetBitrate = ValueOrDefault(OutboundStat.target_bitrate, 0);
				AudioPacketRetransmitted = ValueOrDefault(OutboundStat.retransmitted_packets_sent, 0);

				if (LastAudioStatTimestamp != 0 && AudioTotalSent!= lastByteCount)
				{
					AudioBitrate = (AudioTotalSent - lastByteCount) * NUM_US * 8 / (timestamp - LastAudioStatTimestamp);
				}

				LastAudioStatTimestamp = timestamp;

				AudioCodec = get_codec(Report, OutboundStat);
			}
		}
		else if (StatsType == webrtc::RTCMediaStreamTrackStats::kType)
		{
			auto MediaStats = Stats.cast_to<webrtc::RTCMediaStreamTrackStats>();

			if (*MediaStats.kind == webrtc::RTCMediaStreamTrackKind::kVideo)
			{
				FramesDropped = ValueOrDefault(MediaStats.frames_dropped, 0);
			}
		}
		else if (StatsType == webrtc::RTCIceCandidateStats::kType)
		{
			auto CandidateStat = Stats.cast_to<webrtc::RTCIceCandidatePairStats>();
			Rtt = ValueOrDefault(CandidateStat.current_round_trip_time, 0.) * 1000.;
		}
	}

	Timestamp = Report->timestamp_us();

	const double EncodedFramesDelta = NewTotalEncodedFrames - TotalEncodedFrames;
	if (EncodedFramesDelta)
	{
		const double EncodeTimeDelta = (NewTotalEncodeTime - TotalEncodeTime) * 1000.0;
		AvgEncodeTime = EncodeTimeDelta / EncodedFramesDelta;
		TotalEncodedFrames = NewTotalEncodedFrames;
		TotalEncodeTime = NewTotalEncodeTime;
	}
}

void FRTCStatsCollector::AddRef() const
{
	FPlatformAtomics::InterlockedIncrement(&RefCount);
}

rtc::RefCountReleaseStatus FRTCStatsCollector::Release() const
{
	if (FPlatformAtomics::InterlockedDecrement(&RefCount) == 0)
	{
		return rtc::RefCountReleaseStatus::kDroppedLastRef;
	}

	return rtc::RefCountReleaseStatus::kOtherRefsRemained;
}

}