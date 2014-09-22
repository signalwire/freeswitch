/*
 ============================================================================
 Name		: CDelayRuner.h
 Author	  : R. Drutsky
 Version	 : 1.0
 Copyright   : Copyright (c) 2010 Soft Industry
 Description : CDelayRuner declaration
 ============================================================================
 */

#ifndef DELAYRUNER_H
#define DELAYRUNER_H

#include <e32base.h>	// For CActive, link against: euser.lib
#include <e32std.h>		// For RTimer, link against: euser.lib

#include <zrtp.h>
class CDelayRuner : public CActive
	{
public:
	// Cancel and destroy
	~CDelayRuner();

	// Two-phased constructor.
	static CDelayRuner* NewL();

	// Two-phased constructor.
	static CDelayRuner* NewLC();

public:
	// New functions
	// Function for making the initial request
	void StartL(zrtp_stream_t *ctx, zrtp_retry_task_t* ztask);

private:
	// C++ constructor
	CDelayRuner();

	// Second-phase constructor
	void ConstructL();

private:
	// From CActive
	// Handle completion
	void RunL();

	// How to cancel me
	void DoCancel();

	// Override to handle leaves from RunL(). Default implementation causes
	// the active scheduler to panic.
	TInt RunError(TInt aError);

private:
	enum TCDelayRunerState
		{
		EUninitialized, // Uninitialized
		EInitialized, // Initalized
		EError
		// Error condition
		};

private:
	TInt iState; // State of the active object
	RTimer iTimer; // Provides async timing service

	zrtp_stream_t *iCtx;
	zrtp_retry_task_t * iZTask;

	};

#endif // CDELAYRUNER_H
