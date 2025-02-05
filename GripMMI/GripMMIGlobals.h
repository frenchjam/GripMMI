#pragma once

///
/// Module:	GripMMI
/// 
///	Author:					J. McIntyre, PsyPhy Consulting
/// Initial release:		18 December 2014
/// Modification History:	see https://github.com/PsyPhy/GripMMI
///
/// Copyright (c) 2014, 2015 PsyPhy Consulting
///

/// Definition of preprocessor constants and global variables for GripMMI.

/// <summary>
/// Buffers to hold the GRIP data.
/// The reason that all of these are doubles (or vectors of doubles) is because
///  we want to plot vs. time and time is an array of doubles.
/// The XY plot functions do not, as of now, allow one to have different types
///  for the X and Y axes.
/// </summary>
#define MAX_FRAMES (12*60*60*20) // Max number of frames (data slices)
#define CODA_MARKERS 20
#define	CODA_UNITS	2

#define N_VERTICAL_TARGETS		13
#define N_HORIZONTAL_TARGETS	10

// Buffers to hold the data.
extern Vector3 ManipulandumRotations[MAX_FRAMES];
extern Vector3 ManipulandumPosition[MAX_FRAMES];
extern Vector3 Acceleration[MAX_FRAMES];
extern double GripForce[MAX_FRAMES];
extern Vector3 LoadForce[MAX_FRAMES];
extern double NormalForce[N_FORCE_TRANSDUCERS][MAX_FRAMES];
extern double LoadForceMagnitude[MAX_FRAMES];
extern Vector3 CenterOfPressure[N_FORCE_TRANSDUCERS][MAX_FRAMES];
extern double RealMarkerTime[MAX_FRAMES];
extern double CompressedMarkerTime[MAX_FRAMES];
extern double RealAnalogTime[MAX_FRAMES];
extern double CompressedAnalogTime[MAX_FRAMES];
extern double  MarkerVisibility[MAX_FRAMES][CODA_MARKERS];
#define MANIPULANDUM_FIRST_MARKER 0
#define MANIPULANDUM_LAST_MARKER  7
#define FRAME_FIRST_MARKER 8
#define FRAME_LAST_MARKER 11
#define WRIST_FIRST_MARKER 12
#define WRIST_LAST_MARKER 19
extern double  ManipulandumVisibility[MAX_FRAMES];
extern double  FrameVisibility[MAX_FRAMES];
extern double  WristVisibility[MAX_FRAMES];
extern double  PacketReceived[MAX_FRAMES];
extern char markerVisibilityString[CODA_UNITS][32];
extern unsigned int nFrames;
/// <summary>
/// Data display.
/// </summary>
#define PHASEPLOTS 3
#define STRIPCHARTS	6
#define SPAN_VALUES	8
extern double windowSpanSeconds[SPAN_VALUES]; // Number of seconds to plot for each position of the span selector.
#define MAX_PLOT_STEP 8				// Maximum down sampling to display data.
#define MAX_PLOT_SAMPLES (30 * 60 * 20)		// Max samples to plot.
/// <summary>
/// Strings used to construct the path to various files.
/// </summary>

// Per https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247%28v=vs.85%29.aspx#maxpath
//  MAX_PATHLENGTH is 260 caracters. Here we make it 1024 so that we are sure
//  to not overrun the buffer when constructing paths. We rely on error generated when 
//  opening the file to signal if a pathname has exceeded the limit. 
#define MAX_PATHLENGTH	1024
extern char packetBufferPathRoot[MAX_PATHLENGTH];
extern char scriptDirectory[MAX_PATHLENGTH];
extern char pictureFilenamePrefix[MAX_PATHLENGTH];

/// <summary>
/// Buffers to hold the contents of the scripts.
/// </summary>
#define MAX_TOKENS				32			// No DEX command has close to this many tokens.
#define MAX_STEPS				4096		// I am not aware of any limit on the number of steps in a task definition.
#define MAX_MENU_ITEMS			256			// This should be more than enough for subject, protocol and task menus.
#define MAX_MENU_ITEM_LENGTH	1024		// Text strings in the ListBoxes will be much smaller than this as well.
#define MAX_PICTURE_LENGTH	256				// Name of a picture file will be much shorter than this.
#define MAX_MESSAGE_LENGTH	256				// DEX Messages are also shorter than this.
#define MAX_ERROR_MESSAGE_LENGTH 2048		// More than long enough to hold a path and a lot of text.


// Define the buffers used to hold the contents of the currently selected task script.
extern char picture[MAX_STEPS][MAX_PICTURE_LENGTH];
extern char message[MAX_STEPS][MAX_MESSAGE_LENGTH];
extern const char *type[MAX_STEPS];
extern bool comment[MAX_STEPS];
/// <summary>
/// Constants to translate enums to strings.
/// </summary>
extern char *soundBar[16];
extern char *massDecoder[4];
/// <summary>
/// A helper object for performing vector ops and DEX data ops.
/// </summary>
extern DexAnalogMixin	dex;
extern int TimebaseOffset;