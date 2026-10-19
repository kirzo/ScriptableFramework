// Copyright 2026 kirzo

#include "Tasks/ScriptableTask_MakeLimbicTarget.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	/** The same place as Limbic sees it: a scene source keeps its literal transform as the offset from what it follows. */
	FLimbicTarget ToLimbicTarget(const FKzTransformSource& Source)
	{
		switch (Source.SourceType)
		{
		case EKzTransformSourceType::Scene:
			return Source.SourceComponent ? ULimbicTargetLibrary::MakeLimbicTargetOnComponent(const_cast<USceneComponent*>(Source.SourceComponent.Get()), Source.SourceSocketName, Source.LiteralTransform) : FLimbicTarget();

		case EKzTransformSourceType::Actor:
			return Source.SourceActor && Source.SourceActor->GetRootComponent() ? ULimbicTargetLibrary::MakeLimbicTargetOnComponent(Source.SourceActor->GetRootComponent(), NAME_None, Source.LiteralTransform) : FLimbicTarget();

		case EKzTransformSourceType::Literal:
			return ULimbicTargetLibrary::MakeLimbicTargetInWorld(Source.LiteralTransform);

		default:
			return FLimbicTarget();
		}
	}
}

void UScriptableTask_MakeLimbicTarget::BeginTask()
{
	Target = ToLimbicTarget(Source.IsValid() ? Source : (Actor ? ComponentReference.ToTransformSource(Actor) : FKzTransformSource()));

	Finish();
}