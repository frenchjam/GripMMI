//
// Packet definitions for realtime data from Grip.
//
#pragma once

#include "..\Useful\Useful.h"

// The port number used to access EPM servers.
// EPM-OHB-SP-0005 says:
//  The Port number for all EPM LAN connections is 2345.
#define EPM_DEFAULT_PORT "2345"

// Per EPM-OHB-SP-0005, packets shall not exceed 1412 octets.
#define EPM_BUFFER_LENGTH	1412
#define EPM_TRANSFER_FRAME_HEADER_LENGTH	12
#define EPM_TELEMETRY_HEADER_LENGTH			30

// Definitions for Transfer Frame headers, per EPM-OHB-SP-0005.
#define EPM_TRANSFER_FRAME_SYNC_VALUE	0xAA49DBFF
#define TRANSFER_FRAME_CONNECT			0x0001
#define TRANSFER_FRAME_ALIVE			0x0002
#define TRANSFER_FRAME_TELECOMMAND		0x1154
#define TRANSFER_FRAME_TELEMETRY		0x1153

#define GRIP_MMI_SOFTWARE_UNIT_ID		43
#define GRIP_MMI_SOFTWARE_ALT_UNIT_ID	42
#define GRIP_SUBSYSTEM_ID				0x21

// The MMI uses shared binary files to cache the telemetry packets.
// If one process is reading or writing, another may be denied access.
// Typically, we will retry some number of times before generating an error.
#define MAX_OPEN_CACHE_RETRIES	(5)
// Pause time in milliseconds between file open retries.
#define RETRY_PAUSE	20		
// Error code to return if the cache file cannot be opened.
#define ERROR_CACHE_NOT_FOUND	-1000
// If the time between two realtime data packets exceeds the following threshold
//  then we insert a blank record into the data buffer to show the break in the strip charts.
#define PACKET_STREAM_BREAK_THRESHOLD	1.0
#define PACKET_STREAM_BREAK_INSERT_SAMPLES	10

// These constants help make it clear in initialization lists
//  when we are just filling a spare slot in a structure or
//  when we are initializing a field for which the value is not
//  yet known and will be filled in later.
#define SPARE	0
#define UNKNOWN	0

// Definitions for Telemetry Packet headers.
#define EPM_TELEMETRY_SYNC_VALUE		0xFFDB544D
#define GRIP_HK_ID	0x0301
#define GRIP_RT_ID	0x1001

// Compute the time in seconds.
#define RT_SLICES_PER_PACKET 10
#define RT_DEFAULT_SECONDS_PER_SLICE 0.050
#define RT_SECONDS_PER_TICK	0.001

typedef struct {
	unsigned long	epmLanSyncMarker;
	unsigned char	spare1;
	unsigned char	softwareUnitID;
	unsigned short	packetType;
	unsigned short	spare2;
	unsigned short	numberOfWords;
} EPMTransferFrameHeaderInfo;

typedef struct {

	EPMTransferFrameHeaderInfo	transferFrameInfo;

	unsigned long  epmSyncMarker;
	unsigned char  subsystemMode;
	unsigned char  subsystemID;
	unsigned char  destination;
	unsigned char  subsystemUnitID;
	unsigned short TMIdentifier;
	unsigned short TMCounter;
	unsigned char  model;
	unsigned char  taskID;
	unsigned short subsystemUnitVersion;
	unsigned long  coarseTime;
	unsigned short fineTime;
	unsigned char  timerStatus;
	unsigned char  experimentMode;
	unsigned short checksumIndicator;
	unsigned char  receiverSubsystemID;
	unsigned char  receiverSubsystemUnitID;
	unsigned short numberOfWords;

} EPMTelemetryHeaderInfo;

typedef struct {
	// DATA_MANIP_POSE
	unsigned long	poseTick;
	Vector3			position;
	Quaternion		quaternion;
	unsigned long	markerVisibility[2];  // One for each coda;
	unsigned char	manipulandumVisibility;
	// DATA_IOC_FTG
	unsigned long	analogTick;
	struct	{
		Vector3	force;
		Vector3	torque;
	} ft[2];
	Vector3			acceleration;
	// Each data sample occurs at a separate instant in time, but the timestamp is not
	//  transmitted with each sample. We add a timestamp as best we can.
	long double bestGuessPoseTimestamp;
	long double bestGuessAnalogTimestamp;
} ManipulandumPacket;
 
typedef struct {
	long double packetTimestamp;
	unsigned long acquisitionID;
	unsigned long rtPacketCount;
	ManipulandumPacket dataSlice[RT_SLICES_PER_PACKET];
} GripRealtimeDataInfo;

typedef struct {

#if 0

	// ExtractGripHealthAndStatusInfo() does not yet fill in the entire structure.
	// These values are hidden inside the #if 0 so that no one tries to use them.
	// When ExtractGripHealthAndStatusInfo() gets updated, these items can be exposed again.

	unsigned short	nHousekeepingValue;
	unsigned short	checkStatusListOffset;
	unsigned short	unused1;
	unsigned short	unused2;

	unsigned short	currentMode;
	unsigned short	nextMode;
	unsigned short	timerStatus;
	unsigned short	correctiveAction;
	unsigned short	fileTransferStatus;

	// Various temperature and voltage readings.
	// We don't use them, so I don't name them separately.
	short	temperature[10];
	short	voltage[8];
	unsigned short	selftest;
	float	rxDataRate;
	float	txDataRate;
	unsigned short	fanStatus;
	unsigned short	epmInteraceStatusEnum;
	
	unsigned long	unexplained;

	unsigned short	smokeDetectorStatus;
	unsigned short  OCD;

#endif

	unsigned short	horizontalTargetFeedback;
	unsigned short	verticalTargetFeedback;
	unsigned char	toneFeedback;
	unsigned char	cradleDetectors;

	unsigned short	user;
	unsigned short	protocol;
	unsigned short	task;
	unsigned short	step;

	unsigned short	scriptEngineStatusEnum;
	unsigned short	iochannelStatusEnum;
	unsigned short	motionTrackerStatusEnum;
	unsigned short	crewCameraStatusEnum;

	unsigned short	crewCameraRate; // fps

	unsigned short	runningBits;	// Bit 0: shell command  Bit 1: system acquiring
	unsigned short	cpuUsage;		// percent
	unsigned short	memoryUsage;	// percent

	unsigned long	freeDiskSpaceC;
	unsigned long	freeDiskSpaceD;
	unsigned long	freeDiskSpaceE;

	unsigned short	crc;

} GripHealthAndStatusInfo;

typedef union {
	// A buffer containing the entire packet.
	char buffer[EPM_BUFFER_LENGTH];
	// Allows easy acces to the header elements by name.
	struct {
		unsigned char rawTransferFrameHeader[EPM_TRANSFER_FRAME_HEADER_LENGTH];
		unsigned char rawTelemetryHeader[EPM_TELEMETRY_HEADER_LENGTH];
		unsigned char rawData[EPM_BUFFER_LENGTH - (EPM_TRANSFER_FRAME_HEADER_LENGTH + EPM_TELEMETRY_HEADER_LENGTH) - 2];
		unsigned char rawCRC[2];
	} sections;
} EPMTelemetryPacket; 

// Define a static lan packet for sending a connect command from the GRIP-MMI to EPM.
static EPMTransferFrameHeaderInfo connectPacket = { EPM_TRANSFER_FRAME_SYNC_VALUE, SPARE, GRIP_MMI_SOFTWARE_UNIT_ID, TRANSFER_FRAME_CONNECT, SPARE, 6 };
static int connectPacketLengthInBytes = 12;
static int connectPacketLengthInWords = 6;

static EPMTransferFrameHeaderInfo alivePacket = { EPM_TRANSFER_FRAME_SYNC_VALUE, SPARE, GRIP_MMI_SOFTWARE_UNIT_ID, TRANSFER_FRAME_ALIVE, SPARE, 6 };
static int alivePacketLengthInBytes = 12;
static int alivePacketLengthInWords = 6;

// Define a static packet header that is representative of a housekeeping packet.
// We don't try to simulate all the details, so most of the parameters are set to zero.
// The TM Identifier is 0x0301 for DATA_BULK_HK per DEX-ICD-00383-QS.
// The total number of words is 114 / 2 = 57 for the GRIP packet, 6 for the Transfer Frame header, 
//  15 for the Telemetry header and 1 for the checksum = 79 words = 158 bytes.
// THIS IS ACTUALLY WRONG because it ignores certain housekeeping packets that are actually appended to the 
//  end of the packet, but since that was not given in the documentation provided by Qinetiq/OHB/CADMOS, I am 
//  not going to try to reverse engineer the details. The size of 158 works fine for the GripMMI.
#define BULK_HK_BYTES	158
static EPMTelemetryHeaderInfo hkHeader = { 
	EPM_TRANSFER_FRAME_SYNC_VALUE, SPARE, GRIP_MMI_SOFTWARE_UNIT_ID, TRANSFER_FRAME_TELEMETRY, SPARE, BULK_HK_BYTES,
	EPM_TELEMETRY_SYNC_VALUE, 0, GRIP_SUBSYSTEM_ID, 0, 0, GRIP_HK_ID, UNKNOWN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static int hkPacketLengthInBytes = BULK_HK_BYTES;

// Define a static packet header that is representative of a realtime data packet.
// Not all of the members are properly filled. Just the ones important for the GripMMI.
// The TM Identifier is 0x1001 for DATA_RT_SCIENCE per DEX-ICD-00383-QS.
// The total number of words is 758 / 2 = 379 for the GRIP packet, 6 for the Transfer Frame header,
//  15 for the EPM header and 1 for the checksum = 401 words = 802 bytes.
#define RT_SCIENCE_BYTES	802
static EPMTelemetryHeaderInfo rtHeader = { 
	EPM_TRANSFER_FRAME_SYNC_VALUE, SPARE, GRIP_MMI_SOFTWARE_UNIT_ID, TRANSFER_FRAME_TELEMETRY, SPARE, RT_SCIENCE_BYTES,
	EPM_TELEMETRY_SYNC_VALUE, 0, GRIP_SUBSYSTEM_ID, 0, 0, GRIP_RT_ID, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static int rtPacketLengthInBytes = RT_SCIENCE_BYTES;


typedef enum { GRIP_RT_SCIENCE_PACKET, GRIP_HK_BULK_PACKET, GRIP_UNKNOWN_PACKET } GripPacketType;

#ifdef __cplusplus
extern "C" {
#endif

long double EPMtoSeconds( EPMTelemetryHeaderInfo *header );
int  InsertEPMTransferFrameHeaderInfo ( EPMTelemetryPacket *epm_packet, const EPMTransferFrameHeaderInfo *header  );
void ExtractEPMTransferFrameHeaderInfo ( EPMTransferFrameHeaderInfo *header, const EPMTelemetryPacket *epm_packet );
void ExtractEPMTelemetryHeaderInfo ( EPMTelemetryHeaderInfo *header, const EPMTelemetryPacket *epm_packet  );
int  InsertEPMTelemetryHeaderInfo ( EPMTelemetryPacket *epm_packet,  const EPMTelemetryHeaderInfo *header  );
void ExtractGripRealtimeDataInfo( GripRealtimeDataInfo *realtime_packet, const EPMTelemetryPacket *epm_packet );
void InsertGripRealtimeDataInfo( EPMTelemetryPacket *epm_packet, const GripRealtimeDataInfo *realtime_packet );
void ExtractGripHealthAndStatusInfo( GripHealthAndStatusInfo *health_packet, const EPMTelemetryPacket *epm_packet );
void InsertGripHealthAndStatusInfo( EPMTelemetryPacket *epm_packet, const GripHealthAndStatusInfo *health_packet );

void CreateGripPacketCacheFilename( char *filename, int max_characters, const GripPacketType type, const char *root );
int GetLastPacketHK( EPMTelemetryHeaderInfo *epmHeader, GripHealthAndStatusInfo *hk, char *filename_root );

#ifdef __cplusplus
}
#endif 
 