#pragma once

#include "Hands/HandFrame.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHandSourceFrame, const FHandFrame& /*Frame*/);

/**
 * The one hand-input source. October feeds it a clip. November adds a device
 * source behind this same interface; detectors keep reading frames from here.
 */
class IHandSource
{
public:
	virtual ~IHandSource() = default;

	/** Latest emitted pose for that hand. False if that hand has no sample yet. */
	virtual bool GetLatest(EControllerHand Hand, FHandFrame& Out) const = 0;

	/** Increments once per emitted hand frame (once per hand when both are playing). */
	virtual uint64 GetFrameSequence() const = 0;

	/** Fired once for each newly emitted hand frame. */
	virtual FOnHandSourceFrame& OnHandFrame() = 0;
};
