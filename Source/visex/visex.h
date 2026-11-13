#pragma once

#include "CoreMinimal.h"

#include "Engine/EngineTypes.h"

/** Her feet trace this channel ("FootTrace", Config/DefaultEngine.ini): only the visible
 *  walkable surfaces block it (UVisexFootTrace, at BeginPlay), not the hidden walking
 *  collision under the steps. */
constexpr ECollisionChannel VisexFootTraceChannel = ECC_GameTraceChannel1;
