#pragma once
#include "Brofiler.h"

#include "Event.h"
#include "Serialization.h"
#include "MemoryPool.h"
#include "Sampler.h"
#include "Tracer.h"
//#include "Graphics.h"
#include <map>


namespace Brofiler
{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ScopeHeader
{
	EventTime event;
	uint32 boardNumber;
	uint32 threadNumber;

	ScopeHeader();
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator << ( OutputDataStream& stream, const ScopeHeader& ob);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ScopeData
{
	ScopeHeader header;
	std::vector<EventData> categories;
	std::vector<EventData> events;

	void AddEvent(const EventData& data)
	{
		events.push_back(data);
		if (data.description->color != Color::Null)
		{
			categories.push_back(data);
		}
	}

	void InitRootEvent(const EventData& data)
	{
		header.event = data;
		AddEvent(data);
	}

	void Send();
	void Clear();
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OutputDataStream& operator << ( OutputDataStream& stream, const ScopeData& ob);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef MemoryPool<EventData, 1024> EventBuffer;
typedef MemoryPool<const EventData*, 32> CategoryBuffer;
typedef MemoryPool<SyncData, 1024> SynchronizationBuffer;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct EventStorage
{
	EventBuffer eventBuffer;
	CategoryBuffer categoryBuffer; 
	SynchronizationBuffer synchronizationBuffer;

	MT::Atomic32<uint32> isSampling;

	EventStorage();

	BRO_INLINE EventData& NextEvent() 
	{
		return eventBuffer.Add(); 
	}

	BRO_INLINE void RegisterCategory(const EventData& eventData) 
	{ 
		categoryBuffer.Add() = &eventData;
	}

	// Free all temporary memory
	void Clear(bool preserveContent)
	{
		eventBuffer.Clear(preserveContent);
		categoryBuffer.Clear(preserveContent);
		synchronizationBuffer.Clear(preserveContent);
	}

	void Reset()
	{
		Clear(true);
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ThreadDescription
{
	const char* name;
	MT::ThreadId threadID;

	ThreadDescription(const char* threadName, const MT::ThreadId& id): name(threadName), threadID(id) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ThreadEntry
{
	ThreadDescription description;
	EventStorage storage;
	EventStorage** threadTLS;

	ThreadEntry(const ThreadDescription& desc, EventStorage** tls) : description(desc), threadTLS(tls) {}
	void Activate(bool isActive);
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Core
{
	MT::Mutex lock;

	MT::ThreadId mainThreadID;

	std::vector<ThreadEntry*> threads;
	std::vector<ThreadEntry*> fibers;

	int64 progressReportedLastTimestampMS;

	std::vector<EventTime> frames;

	void UpdateEvents();
	void Update();

	Core();
	~Core();

	static Core notThreadSafeInstance;

	void DumpCapturingProgress();
	void SendHandshakeResponse(EtwStatus status);

	void DumpThread(const ThreadEntry& entry, const EventTime& timeSlice, ScopeData& scope);
public:
	void Activate(bool active);
	bool isActive;

	// Active Frame (is used as buffer)
	static mt_thread_local EventStorage* storage;

#if USE_BROFILER_SAMPLING
	// Controls sampling routine
	Sampler sampler;
#endif

	// Controls GPU activity
	// Graphics graphics;

#if USE_BROFILER_ETW
	// Event Trace for Windows interface
	ETW etw;
#endif

	// Returns thread collection
	const std::vector<ThreadEntry*>& GetThreads() const;

	// Starts sampling process
	void StartSampling();

	// Serialize and send current profiling progress
	void DumpProgress(const char* message = "");

	// Too much time from last report
	bool IsTimeToReportProgress() const;

	// Serialize and send frames
	void DumpFrames();

	// Serialize and send sampling data
	void DumpSamplingData();

	// Registers thread and create EventStorage
	bool RegisterThread(const ThreadDescription& description, EventStorage** slot);

	// Registers finer and create EventStorage
	bool RegisterFiber(const ThreadDescription& description, EventStorage** slot);

	// NOT Thread Safe singleton (performance)
	static BRO_INLINE Core& Get() { return notThreadSafeInstance; }

	// Main Update Function
	static void NextFrame() { Get().Update(); }

	// Get Active ThreadID
	//static BRO_INLINE uint32 GetThreadID() { return Get().mainThreadID; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}