/*
 ============================================================================
 Name		: CDelayRuner.cpp
 Author	  : R. Drutsky
 Version	 : 1.0
 Copyright   : Copyright (c) 2010 Soft Industry
 Description : CCDelayRuner implementation
 ============================================================================
 */

#include "DelayRuner.h"
#include "zrtp_iface_system.h"

void zrtp_internal_delete_task_from_list(zrtp_stream_t* ctx, zrtp_retry_task_t* ztask);

CDelayRuner::CDelayRuner() :
	CActive(EPriorityLow) // Standard priority
	{
	}

CDelayRuner* CDelayRuner::NewLC()
	{
	CDelayRuner* self = new (ELeave) CDelayRuner();
	CleanupStack::PushL(self);
	self->ConstructL();
	return self;
	}

CDelayRuner* CDelayRuner::NewL()
	{
	CDelayRuner* self = CDelayRuner::NewLC();
	CleanupStack::Pop(); // self;
	return self;
	}

void CDelayRuner::ConstructL()
	{
	User::LeaveIfError(iTimer.CreateLocal()); // Initialize timer
	CActiveScheduler::Add(this); // Add to scheduler
	}

CDelayRuner::~CDelayRuner()
	{
	Cancel(); // Cancel any request, if outstanding
	iTimer.Close(); // Destroy the RTimer object
	// Delete instance variables if any
	}

void CDelayRuner::DoCancel()
	{
	iTimer.Cancel();
	}

void CDelayRuner::StartL(zrtp_stream_t *ctx, zrtp_retry_task_t* ztask)
	{
	Cancel(); // Cancel any request, just to be sure
	//iState = EUninitialized;
	iCtx = ctx;
	iZTask = ztask;
	iTimer.After(iStatus, ztask->timeout * 1000); // Set for later
	SetActive(); // Tell scheduler a request is active
	}

void CDelayRuner::RunL()
	{
	if (iStatus == KErrNone)
		{
		// Do something useful
		iZTask->_is_busy = 1 ; // may be we don't need this
		(iZTask->callback)(iCtx,iZTask);
		iZTask->_is_busy = 0 ; // may be we don't need this
		}
	zrtp_internal_delete_task_from_list(iCtx,iZTask);
	}

TInt CDelayRuner::RunError(TInt aError)
	{
	return aError;
	}
