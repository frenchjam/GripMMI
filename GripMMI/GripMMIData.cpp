///
/// Module:	GripMMI
/// 
///	Author:					J. McIntyre, PsyPhy Consulting
/// Initial release:		18 December 2014
/// Modification History:	see https://github.com/PsyPhy/GripMMI
///
/// Copyright (c) 2014, 2015 PsyPhy Consulting
///

/// Methods for retrieving the telemetry data that is stored in cache files.

#include "stdafx.h"
#include <Windows.h>

#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <share.h>
#include <sys\stat.h>
#include <conio.h>
#include <math.h>
#include <float.h>
#include <vcclr.h>

#include "GripMMIDesktop.h"

#include "..\Useful\Useful.h"
#include "..\Useful\VectorsMixin.h"
#include "..\Useful\fMessageBox.h"
#include "..\Useful\fOutputDebugString.h"
#include "..\Grip\GripPackets.h"
#include "..\Grip\DexAnalogMixin.h"

using namespace GripMMI;

// Path to the cache files for telemetry data.
// This is initialized at startup and can be modified by a command line parameter.
char packetBufferPathRoot[MAX_PATHLENGTH] = "";		

// Max times to try to open the cache file before asking user to continue or not.
#define MAX_OPEN_CACHE_RETRIES	(5)
// Pause time in milliseconds between file open retries.
#define RETRY_PAUSE	20		
// Error code to return if the cache file cannot be opened.
#define ERROR_CACHE_NOT_FOUND	-1000
// Grip force threshold for a valid CoP.
#define COP_MIN_GRIP	0.5

// A hint about restarting that may resolve certain intermittant (and hopefully, rare) error conditions.
const char *restart_hint = 
	"This is a fatal error.\n\nTry restarting just the graphical interface using the RestartGripMMI.YYYY.MM.DD.bat file\nthat has been createdd in the cache or executables directory.\n\nIf that fails, kill GripGroundMonitorClient.exe, rename or copy to a safe location the cache files\nand execute RunGripMMI.bat again to restart.\n";

///
/// Show the data buffers as empty.
///
void GripMMIDesktop::ResetBuffers( void ){
	nFrames = 0;
}

/// Read in the cached realtime data packets.
/// The path to the cache file is presumed to be set in global variable packetBufferPathRoot.
/// The data is stored in the global arrays found in GripMMIGlobals.cpp.
/// TRUE is returned if there are new packets since the last call.

/// Note that if in a previous call the buffers were filled to the maximum, this
/// routine will simply return FALSE, leaving the buffers in their former state.

/// Note also that if the buffers reach their maximum, the 'live' mode for RT packets will be disabled.
int GripMMIDesktop::GetGripRT( void ) {

	// Keep track of the last packet TM counter from previous call.
	// This is how we know if new data has arrived.
	static unsigned short previousTMCounter = 0;

	// Keep track of whether we have already seen that the buffers are full.
	static bool buffers_full_alert = false;

	// Buffers and structures to hold data from the real time science packets.
	EPMTelemetryPacket		packet;
	EPMTelemetryHeaderInfo	epmHeader;
	GripRealtimeDataInfo	rt;

	// Will hold the filename (path) of the packet file.
	char filename[MAX_PATHLENGTH];
	// Will hold the pointer to the open packet file.
	int  fid;

	// Various local counters and flags.
	int bytes_read;
	int packets_read;
	int return_code;
	int mrk, coda, count;

	// If buffers were full the last time through, then don't fill them again.
	// Just leave the buffers in their previous state and return saying that
	// there is no new data.
	if ( buffers_full_alert ) {
		// Disable Live mode for science data so that the 
		dataLiveCheckbox->Checked = false;
		dataLiveCheckbox->Enabled = false;
		return( FALSE );
	}

	// Create the path to the realtime science packet file, based on the root and the packet type.
	// The global variable 'packetBufferPathRoot' has been initialized elsewhere.
	CreateGripPacketCacheFilename( filename, sizeof( filename ), GRIP_RT_SCIENCE_PACKET, packetBufferPathRoot );

	// Empty the data buffers.
	ResetBuffers();

	// Attempt to open the packet cache to read the accumulated packets.
	// If it is not immediately available, keep trying for a few seconds.
	for ( int retry_count = 0; retry_count  < MAX_OPEN_CACHE_RETRIES; retry_count ++ ) {
		// Try to open the packet cache file.
		fid = _sopen( filename, _O_RDONLY | _O_BINARY, _SH_DENYNO, _S_IWRITE | _S_IREAD  );
		// If open succeeds, it will return zero. So if zero return, break from retry loop.
		if ( fid >= 0 ) break;
		// Wait a second before trying again.
		Sleep( RETRY_PAUSE );
	}
	// If fid is negative, file is not open. This should not happen, because GripMMIStartup should verify 
	// the availability of files containing packets before the GripMMIDesktop form is executed.
	// But if we do fail to open the file, just signal the error and exit the hard way.
	if ( fid < 0 ) {
			fMessageBox( MB_OK, "GripMMI", "Error opening packet file %s.\n\n%s", filename, restart_hint );
			exit( -1 );
	}

	// Prepare for reading in packets. This is used to calculate the elapsed time between two packets.
	// By setting it to zero here, the first packet read will be signaled as having arrived after a long delay.
	double previous_packet_timestamp = 0.0;

	// Read in all of the data packets in the file.
	// Be careful not to overrun the data buffers.
	packets_read = 0;
	while ( nFrames < MAX_FRAMES ) {

		// Attempt to read next packet. Any error is terminal.
		bytes_read = _read( fid, &packet, rtPacketLengthInBytes );
		if ( bytes_read < 0 ) {
			fMessageBox( MB_OK, "GripMMI", "Error reading from %s.\n\n%s", filename, restart_hint );
			exit( -1 );
		}

		// If the number of bytes read is less than the expected number
		//  we are at the end of the file and should break out of the loop.
		if ( rtPacketLengthInBytes != bytes_read ) break;

		// We have a valid packet.
		packets_read++;

		// Check that it is a valid GRIP packet. It would be strange if it was not.
		ExtractEPMTelemetryHeaderInfo( &epmHeader, &packet );
		if ( epmHeader.epmSyncMarker != EPM_TELEMETRY_SYNC_VALUE || epmHeader.TMIdentifier != GRIP_RT_ID ) {
			fMessageBox( MB_OK, "GripMMIlite", "Unrecognized packet from %s.\n\n%s", filename, restart_hint );
			exit( -1 );
		}
			
		// Packets are stings of bytes. Extract the data values into a more usable form.
		ExtractGripRealtimeDataInfo( &rt, &packet );

		// If there has been a break in the arrival of the packets, insert
		//  a blank frame into the data buffer. This will cause breaks in
		//  the traces in the data graphs.
		if ( (rt.packetTimestamp - previous_packet_timestamp) > PACKET_STREAM_BREAK_THRESHOLD ) {
			// Subsampling in graphs will be used when the data record is very long.
			// Insert enough points so that we see the break even if we are sub-sampling in the graphs.
			// MAX_PLOT_STEP defines the maximum number of frames that will be skipped when plotting.
			for ( int count = 0; count < MAX_PLOT_STEP && nFrames < MAX_FRAMES - 1; count++ ) {
				ManipulandumPosition[nFrames][X] = MISSING_DOUBLE;
				ManipulandumPosition[nFrames][Y] = MISSING_DOUBLE;
				ManipulandumPosition[nFrames][Z] = MISSING_DOUBLE;
				ManipulandumRotations[nFrames][X] = MISSING_DOUBLE;
				ManipulandumRotations[nFrames][Y] = MISSING_DOUBLE;
				ManipulandumRotations[nFrames][Z] = MISSING_DOUBLE;
				GripForce[nFrames] = MISSING_DOUBLE;
				GripForce[nFrames] = MISSING_DOUBLE;
				NormalForce[LEFT_ATI][nFrames] = MISSING_DOUBLE;
				NormalForce[LEFT_ATI][nFrames] = MISSING_DOUBLE;
				NormalForce[RIGHT_ATI][nFrames] = MISSING_DOUBLE;
				NormalForce[RIGHT_ATI][nFrames] = MISSING_DOUBLE;
				Acceleration[nFrames][X] = MISSING_DOUBLE;
				Acceleration[nFrames][Y] = MISSING_DOUBLE;
				Acceleration[nFrames][Z] = MISSING_DOUBLE;
				for ( mrk = 0; mrk < CODA_MARKERS; mrk++ ) MarkerVisibility[nFrames][mrk] = MISSING_DOUBLE;
				ManipulandumVisibility[nFrames] = MISSING_DOUBLE;
				FrameVisibility[nFrames] = MISSING_DOUBLE;
				WristVisibility[nFrames] = MISSING_DOUBLE;
				PacketReceived[nFrames] = MISSING_DOUBLE;
				RealMarkerTime[nFrames] = MISSING_DOUBLE;
				nFrames++;
			}
		}
		previous_packet_timestamp = rt.packetTimestamp;

		for ( int slice = 0; slice < RT_SLICES_PER_PACKET && nFrames < MAX_FRAMES; slice++ ) {
			// Get the time of the slice.
			RealMarkerTime[nFrames] = rt.dataSlice[slice].bestGuessPoseTimestamp;
			RealAnalogTime[nFrames] = rt.dataSlice[slice].bestGuessAnalogTimestamp;
			if ( rt.dataSlice[slice].manipulandumVisibility ) {
				// Retrieve the position and convert to mm.
				ManipulandumPosition[nFrames][X] = rt.dataSlice[slice].position[X] / 10.0;
				ManipulandumPosition[nFrames][Y] = rt.dataSlice[slice].position[Y] / 10.0;
				ManipulandumPosition[nFrames][Z] = rt.dataSlice[slice].position[Z] / 10.0;
				// Convert quaternion to a form that is easier to understand in graphs.
				dex.QuaternionToCannonicalRotations( ManipulandumRotations[nFrames], rt.dataSlice[slice].quaternion );
				// Apply recursive filter to position data for this slice.
				dex.FilterManipulandumPosition( ManipulandumPosition[nFrames] );
				// If the orientation is available, filter it as well.
				if ( _finite( ManipulandumRotations[nFrames][X] ) ) dex.FilterManipulandumRotations( ManipulandumRotations[nFrames] );
			}
			else {
				// Manipulandum was not visible, so record as missing data.
				ManipulandumPosition[nFrames][X] = MISSING_DOUBLE;
				ManipulandumPosition[nFrames][Y] = MISSING_DOUBLE;
				ManipulandumPosition[nFrames][Z] = MISSING_DOUBLE;
				ManipulandumRotations[nFrames][X] = MISSING_DOUBLE;
				ManipulandumRotations[nFrames][Y] = MISSING_DOUBLE;
				ManipulandumRotations[nFrames][Z] = MISSING_DOUBLE;
			}
			// The GRIP ICD does not say what is the reference frame for the force data.
			// I'm pretty sure that this is right.
			GripForce[nFrames] = (float) dex.ComputeGripForce( rt.dataSlice[slice].ft[LEFT_ATI].force, rt.dataSlice[slice].ft[RIGHT_ATI].force );
			GripForce[nFrames] = (float) dex.FilterGripForce( GripForce[nFrames] );
			// It is useful to plot the normal force from each ATI sensor. They should be very similar unless
			//  the subject is touching the manipulandum outside the ATI sensor surfaces.
			NormalForce[LEFT_ATI][nFrames] = - (float) rt.dataSlice[slice].ft[LEFT_ATI].force[X];
			NormalForce[LEFT_ATI][nFrames] = (float) dex.FilterNormalForce( NormalForce[LEFT_ATI][nFrames], LEFT_ATI );
			NormalForce[RIGHT_ATI][nFrames] = (float) rt.dataSlice[slice].ft[RIGHT_ATI].force[X];
			NormalForce[RIGHT_ATI][nFrames] = (float) dex.FilterNormalForce( NormalForce[RIGHT_ATI][nFrames], RIGHT_ATI );
			// Compute the acceleration, load force, load force magnitude and center-of-pressures, and filter appropriately.
			dex.ComputeLoadForce( LoadForce[nFrames], rt.dataSlice[slice].ft[0].force, rt.dataSlice[slice].ft[1].force );
			LoadForceMagnitude[nFrames] = dex.FilterLoadForce( LoadForce[nFrames] );
			for ( int ati = 0; ati < N_FORCE_TRANSDUCERS; ati++ ) {
				double cop_distance = dex.ComputeCoP( CenterOfPressure[ati][nFrames], rt.dataSlice[slice].ft[ati].force, rt.dataSlice[slice].ft[ati].torque, COP_MIN_GRIP );
				if ( cop_distance >= 0.0 ) dex.FilterCoP( ati, CenterOfPressure[ati][nFrames] );
			}
			Acceleration[nFrames][X] = (float) rt.dataSlice[slice].acceleration[X];
			Acceleration[nFrames][Y] = (float) rt.dataSlice[slice].acceleration[Y];
			Acceleration[nFrames][Z] = (float) rt.dataSlice[slice].acceleration[Z];
			dex.FilterAcceleration( Acceleration[nFrames] );

			// Fill some data arrays to show when each marker is visible.
			// We consider a marker visible if it is seen by either coda.
			// Set a non-zero value if it is visible, MISSING if it is obscured.
			// The non-zero values that are set when the marker is visible are a convenient
			//  trick to make it easy to plot the traces for all markers in one graph.
			for ( mrk = MANIPULANDUM_FIRST_MARKER; mrk <= MANIPULANDUM_LAST_MARKER; mrk++ ) {
				unsigned long bit = 0x01 << mrk;
				if ( rt.dataSlice[slice].markerVisibility[0] & bit || rt.dataSlice[slice].markerVisibility[1] & bit ) MarkerVisibility[nFrames][mrk] = mrk + 1;
				else MarkerVisibility[nFrames][mrk] = MISSING_DOUBLE;
			}
			if (  (rt.dataSlice[slice].manipulandumVisibility & 0x01) ) ManipulandumVisibility[nFrames] = 10;
			else ManipulandumVisibility[nFrames] = MISSING_DOUBLE;
			for ( mrk = FRAME_FIRST_MARKER, count = 0; mrk <= FRAME_LAST_MARKER; mrk++ ) {
				unsigned long bit = 0x01 << mrk;
				if ( rt.dataSlice[slice].markerVisibility[0] & bit || rt.dataSlice[slice].markerVisibility[1] & bit ) {
					MarkerVisibility[nFrames][mrk] = mrk + 3;
					count++;
				}
				else MarkerVisibility[nFrames][mrk] = MISSING_DOUBLE;
			}
			if ( count == 4 ) FrameVisibility[nFrames] = 30;
			else FrameVisibility[nFrames] = MISSING_DOUBLE;

			for ( mrk = WRIST_FIRST_MARKER, count = 0; mrk <= WRIST_LAST_MARKER; mrk++ ) {
				unsigned long bit = 0x01 << mrk;
				if ( rt.dataSlice[slice].markerVisibility[0] & bit || rt.dataSlice[slice].markerVisibility[1] & bit ) {
					MarkerVisibility[nFrames][mrk] = mrk + 5;
					count++;
				}
				else MarkerVisibility[nFrames][mrk] = MISSING_DOUBLE;
			}
			if ( count >= 3 ) WristVisibility[nFrames] = 50;
			else WristVisibility[nFrames] = MISSING_DOUBLE;
			// Indicate that for this instant in time we received a data packet.
			PacketReceived[nFrames] = -10.0;

			// Count the number of frames.
			nFrames++;
		}

	}
	// Finished reading. Close the file and check for errors.
	return_code = _close( fid );
	if ( return_code ) {
		fMessageBox( MB_OK, "GripMMI", "Error closing %s after binary read.\nError code: %s\n\n%s", filename, return_code, restart_hint );
		exit( return_code );
	}
	// Compute the visibility strings for the markers from the last frame.
	for (coda = 0; coda < CODA_UNITS; coda++ ) {
		strcpy( markerVisibilityString[coda], "" );
		for ( mrk = 0; mrk < CODA_MARKERS; mrk++ ) {
			unsigned long bit = 0x01 << mrk;
			if ( mrk == 8 || mrk == 12 ) strcat( markerVisibilityString[coda], "  " );
			if ( rt.dataSlice[RT_SLICES_PER_PACKET - 1].markerVisibility[coda] & bit ) strcat( markerVisibilityString[coda], "u" );
			else strcat( markerVisibilityString[coda], "m" );
		}
	}
	fOutputDebugString( "Acquired Frames (max %d): %d\n", MAX_FRAMES, nFrames );
	if ( nFrames >= MAX_FRAMES ) {
		char filename2[MAX_PATHLENGTH];
		CreateGripPacketCacheFilename( filename2, sizeof( filename ), GRIP_HK_BULK_PACKET, packetBufferPathRoot );
		fMessageBox( MB_OK | MB_ICONERROR, "GripMMI", 
			"Internal buffers are full.\n\nYou can continue plotting existing data.\nTracking of script progress will also continue.\n\nTo resume following new data transmissions:\n\n1) Halt GripMMI.exe (this program).\n2) Halt GripGroundMonitorClient.exe.\n3) Rename or move:\n      %s\n      %s\n4) Restart using RunGripMMI.bat.",
			filename, filename2 );
		// Signal for the next call that we have already reached the limit of the buffers.
		buffers_full_alert = true;
	}
	// Check if there were new packets since the last time we read the cache.
	// Return TRUE if yes, FALSE if no.
	if ( previousTMCounter != epmHeader.TMCounter ) {
		previousTMCounter = epmHeader.TMCounter;
		return( TRUE );
	}
	else return ( FALSE );
}
/// Simulate a set of realtime data packets.
/// This is not an option that is available at run time. It can only be used by modifying the code
///  to call this routine instead of GetGripRT().
void GripMMIDesktop::SimulateGripRT ( void ) {

	// Simulate some data to test memory limites and graphics functions.
	// Each time through it adds more data to the buffers, so as to 
	//  simulate the progressive arrival of real-time data packets.

	int i, mrk;

	static int count = 0;

	fOutputDebugString( "Start SimulateGripRT().\n" );
	count++;
	unsigned int fill_frames = 60 * 20 * count;
	for ( nFrames = 0; nFrames <= fill_frames && nFrames < MAX_FRAMES; nFrames++ ) {

		RealMarkerTime[nFrames] = (float) nFrames * 0.05f;
		ManipulandumPosition[nFrames][X] = 30.0 * sin( RealMarkerTime[nFrames] * Pi * 2.0 / 30.0 );
		ManipulandumPosition[nFrames][Y] = 300.0 * cos( RealMarkerTime[nFrames] * Pi * 2.0 / 30.0 ) + 200.0;
		ManipulandumPosition[nFrames][Z] = -75.0 * sin( RealMarkerTime[nFrames] * Pi * 2.0 / 155.0 ) - 300.0;

		GripForce[nFrames] = (float) abs( -5.0 * sin( RealMarkerTime[nFrames] * Pi * 2.0 / 155.0 )  );
		for ( i = X; i <= Z; i++ ) {
			LoadForce[nFrames][i] = ManipulandumPosition[nFrames][ (i+2) % 3] / 200.0;
		}

		for ( mrk = 0; mrk <CODA_MARKERS; mrk++ ) {

			int grp = ( mrk >= 8 ? ( mrk >= 16 ? mrk + 20 : mrk + 10 ) : mrk ) + 35;
			if ( nFrames == 0 ) MarkerVisibility[nFrames][mrk] = grp;
			else {
				if ( MarkerVisibility[nFrames-1][mrk] != MISSING_CHAR ) {
					if ( rand() % 1000 < 1 ) MarkerVisibility[nFrames][mrk] = MISSING_CHAR;
					else MarkerVisibility[nFrames][mrk] = grp;
				}
				else {
					if ( rand() % 1000 < 1 ) MarkerVisibility[nFrames][mrk] = grp;
					else MarkerVisibility[nFrames][mrk] = MISSING_CHAR;
				}
			}
		}
			
		ManipulandumVisibility[nFrames] = 0;
		for ( mrk = MANIPULANDUM_FIRST_MARKER; mrk <= MANIPULANDUM_LAST_MARKER; mrk++ ) {
			if ( MarkerVisibility[nFrames][mrk] != MISSING_CHAR ) ManipulandumVisibility[nFrames]++;
		}
		if ( ManipulandumVisibility[nFrames] < 3 ) ManipulandumPosition[nFrames][X] = ManipulandumPosition[nFrames][Y] = ManipulandumPosition[nFrames][Z] = MISSING_DOUBLE;
		ManipulandumVisibility[nFrames] *= 3;

	}
	fOutputDebugString( "End SimulateGripRT().\n" );
	fOutputDebugString( "nFrames: %d %d\n", nFrames, MAX_FRAMES );
}

/// Read housekeeping cache, taking just the most recent value.
/// The path to the cache file is presumed to be set in global variable 'packetBufferPathRoot'.
/// The contents of the latest HK packet are returned in the structure pointed to by parameter 'hk'.
int GripMMIDesktop::GetLatestGripHK( GripHealthAndStatusInfo *hk ) {

	static int count = 0;

	int  fid;
	int packets_read = 0;
	int bytes_read;
	int return_code;
	static unsigned short previousTMCounter = 0;
	unsigned long bit = 0;
	int retry_count;

	EPMTelemetryPacket packet;
	EPMTelemetryHeaderInfo epmHeader;

	char filename[1024];

	// Create the path to the housekeeping packet file, based on the root and the packet type.
	// The global variable 'packetBufferPathRoot' has been initialized elsewhere.
	CreateGripPacketCacheFilename( filename, sizeof( filename ), GRIP_HK_BULK_PACKET, packetBufferPathRoot );

	// Attempt to open the packet cache to read the accumulated packets.
	// If it is not immediately available, try for a few seconds.
	for ( retry_count = 0; retry_count  < MAX_OPEN_CACHE_RETRIES; retry_count ++ ) {
		// Try to open the packet cache file.
		fid = _open( filename, _O_RDONLY | _O_BINARY, _S_IWRITE | _S_IREAD  );
		// If open succeeds, it will return zero. So if zero return, break from retry loop.
		if ( fid >= 0 ) break;
		// Wait a second before trying again.
		Sleep( RETRY_PAUSE );
	}
	// If fid is negative, file is not open. This should not happen, because GripMMIStartup should verify 
	// the availability of files containing packets before the GripMMIDesktop form is executed.
	// So if we do fail to open the file, signal the error and exit.
	if ( fid < 0 ) {
		fMessageBox( MB_OK, "GripMMI", "Error reading from %s.\n\n*s", filename, restart_hint );
		exit( -1 );
	}

	// Read in all of the data packets in the file.
	packets_read = 0;
	while ( true ) {
		bytes_read = _read( fid, &packet, hkPacketLengthInBytes );
		// Return less than zero means read error.
		if ( bytes_read < 0 ) {
			fMessageBox( MB_OK, "GripMMI", "Error reading from %s.\n\n%s", filename, restart_hint );
			exit( -1 );
		}
		// Return less than expected number of bytes means we have read all packets.
		if ( bytes_read < hkPacketLengthInBytes ) break;

		packets_read++;
		// Check that it is a valid GRIP packet. It would be strange if it was not.
		ExtractEPMTelemetryHeaderInfo( &epmHeader, &packet );
		if ( epmHeader.epmSyncMarker != EPM_TELEMETRY_SYNC_VALUE || epmHeader.TMIdentifier != GRIP_HK_ID ) {
			fMessageBox( MB_OK, "GripMMI", "Unrecognized packet from %s.\n\n%s", filename, restart_hint );
			exit( -1 );
		}
		// Extract the interesting info in proper byte order.
		ExtractGripHealthAndStatusInfo( hk, &packet );
	}
	// Finished reading. Close the file and check for errors.
	return_code = _close( fid );
	if ( return_code ) {
		fMessageBox( MB_OK, "GripMMI", "Error closing %s after binary read.\nError code: %s\n\n%s", filename, return_code, restart_hint );
		exit( return_code );
	}

	// The structure pointed to by 'hk' contains the data from the last valid packet that was read from the cache file.
	// Check if there were new packets since the last time we read the cache.
	// Return TRUE if yes, FALSE if no.
	if ( previousTMCounter != epmHeader.TMCounter ) {
		previousTMCounter = epmHeader.TMCounter;
		return( TRUE );
	}
	else return ( FALSE );
}

/// Update the script crawler windows and state indicators (markers, targets, etc.)
/// based on realtime HK data packet info.
void GripMMIDesktop::UpdateStatus( bool force ) {

	int i;
	unsigned long bit;
	char target_state_string[256];
	char mass_state_string[16];
	char coda_state_string[256];
	char acquisition_state_string[16];

	GripHealthAndStatusInfo hk_info;

	int return_code;

	// Get the latest hk packet info.
	return_code = GetLatestGripHK( &hk_info );
	if ( ERROR_CACHE_NOT_FOUND == return_code ) {
		return;
	}
	else if ( return_code == TRUE || force ) {

		// Show the state of the script engine.
		if ( hk_info.task != 0 && hk_info.scriptEngineStatusEnum == 0x1000 ) scriptErrorCheckbox->Checked = true;
		else scriptErrorCheckbox->Checked = false;

		// Update the Script Crawler display as if somone has entered the subject, protocol, task and step 
		//  IDs by hand and then pushes the GoTo button.
		GoToSpecifiedIDs( hk_info.user, hk_info.protocol, hk_info.task, hk_info.step );

		// State of the target LEDs.
		strcpy( target_state_string, "" );
		for ( i = 0, bit = 0x01; i < 10; i++, bit = bit << 1 ) {
			if ( bit & hk_info.horizontalTargetFeedback ) strcat( target_state_string, "u" );
			else strcat( target_state_string, "m" );
		}
		strcat( target_state_string, "\r\n" );
		for ( i = 0, bit = 0x01; i < 13; i++, bit = bit << 1 ) {
			if ( bit & hk_info.verticalTargetFeedback ) strcat( target_state_string, "u" );
			else strcat( target_state_string, "m" );
		}
		targetsTextBox->Clear();
		targetsTextBox->AppendText( gcnew String( target_state_string ));

		// State of the tone generator.
		tonesTextBox->Clear();
		tonesTextBox->AppendText( gcnew String( soundBar[ hk_info.toneFeedback] ));

		// State of the mass cradles.
		strcpy( mass_state_string, "" );
		strcat( mass_state_string, massDecoder[ hk_info.cradleDetectors >> 0 & 0x03 ] );
		strcat( mass_state_string, " " );
		strcat( mass_state_string, massDecoder[ hk_info.cradleDetectors >> 2 & 0x03 ] );
		strcat( mass_state_string, " " );
		strcat( mass_state_string, massDecoder[ hk_info.cradleDetectors >> 4 & 0x03 ] );
		cradlesTextBox->Clear();
		cradlesTextBox->AppendText( gcnew String( mass_state_string ));

		// Marker visibility.
		markersTextBox->Clear();
		strcpy( coda_state_string, markerVisibilityString[0] );
		strcat( coda_state_string, "\r\n" );
		strcat( coda_state_string, markerVisibilityString[1] );
		markersTextBox->AppendText( gcnew String( coda_state_string ));

		// Acquisition mode (markers and video).
		strcpy( acquisition_state_string, "");
		if ( hk_info.motionTrackerStatusEnum == 2 ) strcat( acquisition_state_string, "A" );
		else strcat( acquisition_state_string, " " );
		if ( hk_info.crewCameraStatusEnum == 2 ) strcat( acquisition_state_string, " F" );
		else strcat( acquisition_state_string, " " );
		acquisitionTextBox->Clear();
		acquisitionTextBox->AppendText( gcnew String( acquisition_state_string ));

	}
}