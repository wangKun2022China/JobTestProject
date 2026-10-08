#include "StateSerial.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"


namespace MetaHumanStateSerial
{
	TSharedPtr<FJsonObject> StateToJson(const FCharacterState& State)
	{
		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

		// character_state.body（身体）
		TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("animation_asset"), State.Body.AnimationAsset);
		Body->SetNumberField(TEXT("time_position"), State.Body.TimePosition);

		// character_state.facial_curves（面部曲线）
		TSharedPtr<FJsonObject> Facial = MakeShared<FJsonObject>();
		for (const TPair<FName, float>& Pair : State.FacialCurves.Values)
		{
			Facial->SetNumberField(Pair.Key.ToString(), Pair.Value);
		}

		TSharedPtr<FJsonObject> CharacterState = MakeShared<FJsonObject>();
		CharacterState->SetObjectField(TEXT("body"), Body);
		CharacterState->SetObjectField(TEXT("facial_curves"), Facial);
		Root->SetObjectField(TEXT("character_state"), CharacterState);

		// render_settings（顶层兄弟节点，与题目规范一致）
		TSharedPtr<FJsonObject> Render = MakeShared<FJsonObject>();
		Render->SetStringField(TEXT("camera"), State.RenderSettings.Camera);
		Render->SetStringField(TEXT("output_filename"), State.RenderSettings.OutputFilename);
		Root->SetObjectField(TEXT("render_settings"), Render);

		return Root;
	}

	FCharacterState JsonToState(const TSharedPtr<FJsonObject>& Root)
	{
		FCharacterState State;

		const TSharedPtr<FJsonObject>* CharacterState = nullptr;
		if (Root->TryGetObjectField(TEXT("character_state"), CharacterState))
		{
			const TSharedPtr<FJsonObject>* Body = nullptr;
			if ((*CharacterState)->TryGetObjectField(TEXT("body"), Body))
			{
				(*Body)->TryGetStringField(TEXT("animation_asset"), State.Body.AnimationAsset);
				(*Body)->TryGetNumberField(TEXT("time_position"), State.Body.TimePosition);
			}

			const TSharedPtr<FJsonObject>* Facial = nullptr;
			if ((*CharacterState)->TryGetObjectField(TEXT("facial_curves"), Facial))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Facial)->Values)
				{
					double Value = 0.0;
					if (Pair.Value && Pair.Value->TryGetNumber(Value))
					{
						State.FacialCurves.Values.Add(FName(*Pair.Key), static_cast<float>(Value));
					}
				}
			}
		}

		const TSharedPtr<FJsonObject>* Render = nullptr;
		if (Root->TryGetObjectField(TEXT("render_settings"), Render))
		{
			(*Render)->TryGetStringField(TEXT("camera"), State.RenderSettings.Camera);
			(*Render)->TryGetStringField(TEXT("output_filename"), State.RenderSettings.OutputFilename);
		}

		return State;
	}
}

bool UMetaHumanStateSerial::ExportStateToJson(const FCharacterState& State, const FString& FilePath)
{
	FString OutString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutString);
	FJsonSerializer::Serialize(MetaHumanStateSerial::StateToJson(State).ToSharedRef(), Writer);
	return FFileHelper::SaveStringToFile(OutString, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool UMetaHumanStateSerial::ImportStateFromJson(const FString& FilePath, FCharacterState& OutState)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	OutState = MetaHumanStateSerial::JsonToState(Root);
	return true;
}
