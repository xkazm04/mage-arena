#pragma once

#include "CoreMinimal.h"
#include "Hands/HandFrame.h"

/**
 * VRC.Quest.Input.8: the system gesture is reserved and must not trigger anything else in the app.
 * Each detector subsystem keeps one of these and asks it about every hand frame. While a hand's frame
 * carries bSystemGesture the frame is dropped, and the frame that raises the bit asks the detector
 * to cancel what that hand had in progress, so nothing completes when the bit clears.
 */
struct FSystemGestureGate
{
	bool bLeft = false;
	bool bRight = false;

	void Reset()
	{
		bLeft = false;
		bRight = false;
	}

	/** True when the detector must not see this frame. bOutBegan is true on the frame that raised the bit. */
	bool Filter(const FHandFrame& Frame, bool& bOutBegan)
	{
		bOutBegan = false;
		bool* Active = Frame.Hand == EControllerHand::Left ? &bLeft : (Frame.Hand == EControllerHand::Right ? &bRight : nullptr);
		if (Active == nullptr)
		{
			return false;
		}
		bOutBegan = Frame.bSystemGesture && !*Active;
		*Active = Frame.bSystemGesture;
		return Frame.bSystemGesture;
	}
};
