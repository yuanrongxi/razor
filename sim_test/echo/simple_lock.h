
#pragma once
#include <windows.h>

class SimpleLock
{
public:
	SimpleLock()
	{
		::InitializeCriticalSection(&cs_);
	}
	virtual ~SimpleLock()
	{
		::DeleteCriticalSection(&cs_);
	}

	SimpleLock(const SimpleLock&) = delete;
	SimpleLock& operator=(const SimpleLock&) = delete;

public:
	void Lock()
	{
		::EnterCriticalSection(&cs_);
	}

	void Unlock()
	{
		::LeaveCriticalSection(&cs_);
	}

	bool TryLock()
	{
		return (FALSE != ::TryEnterCriticalSection(&cs_));
	}

private:
	CRITICAL_SECTION cs_;
};

class AutoSpLock
{
public:
	AutoSpLock(SimpleLock &lock)
		: lock_(lock)
	{
		lock_.Lock();
	}

	virtual ~AutoSpLock()
	{
		lock_.Unlock();
	}

private:
	SimpleLock &lock_;
};
