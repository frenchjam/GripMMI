/*********************************************************************************/
/*                                                                               */
/*                               DexAnalogMixin.cpp                              */
/*                                                                               */
/*********************************************************************************/

// Methods to process Dex (Grip) analog data, notably the ATI data.
// Copyright (c) 2012-2013 PsyPhy Consulting. All rights reserved.

// This code was written when developing and testing the algorithms for the 
//  DEX (now GRIP) hardware. It has be reused in the devepment of the GripMMI.

#include <windows.h>
#include <mmsystem.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h> 
#include <memory.h>
#include <process.h>

#include "..\Useful\fMessageBox.h"
#include "..\Useful\VectorsMixin.h"
#include "..\Useful\Useful.h"

#include "DexAnalogMixin.h"

/***************************************************************************/

DexAnalogMixin::DexAnalogMixin( void ) {

	Quaternion align, flip;

	// Define the rotation of the ATI sensors with respect to the local 
	// manipulandum reference frame. These are probably constants, 
	// but perhaps they should be read from the model-specific parameter file as well.
	ATIRotationAngle[LEFT_ATI] = LEFT_ATI_ROTATION;
	ATIRotationAngle[RIGHT_ATI] = RIGHT_ATI_ROTATION;

	// Compute the transformations to put ATI forces in a common reference frame.
	// The local manipulandum reference frame should align with
	// the world reference frame when the manipulandum is held upright in the seated posture.
	SetQuaterniond( ftAlignmentQuaternion[0], ATIRotationAngle[0], kVector );
	SetQuaterniond( align, ATIRotationAngle[1], kVector );
	SetQuaterniond( flip, 180.0, iVector );
	MultiplyQuaternions( ftAlignmentQuaternion[1], flip, align );

	// Set a default filter constant.
	SetFilterConstant( 100.0 );

	// Initialize some instance variables used to hold the current state
	// when filtering certain vector values.
	CopyVector( filteredManipulandumPosition, zeroVector );
	CopyVector( filteredManipulandumRotations, zeroVector );
	CopyVector( filteredLoadForce, zeroVector );
	CopyVector( filteredAcceleration, zeroVector );
	for (int ati = 0; ati < N_FORCE_TRANSDUCERS; ati++ ) CopyVector( filteredCoP[ati], zeroVector );
	filteredGripForce = 0.0;

}

/***************************************************************************/

// Compute the center-of-pressure based on force and torque measurements.
// The normal force must be above threshold to compute a valid CoP.
// Returns the CoP in the vector pointed to by cop, and returns the distance of
//  of the CoP from 0,0 as a double. If CoP cannot be computed, returns -1.0.
double DexAnalogMixin::ComputeCoP( Vector3 &cop, Vector3 &force, Vector3 &torque, double threshold ) {
	// If there is enough normal force, compute the center of pressure.
	if ( fabs( force[X] ) > threshold ) {
		cop[Y] = - torque[Z] / force[X];
		cop[Z] = - torque[Y] / force[X];
		cop[X] = 0.0;
		// Return the distance from the center.
		return( sqrt( cop[Z] * cop[Z] + cop[Y] * cop[Y] ) );
	}
	else {
		// If there is not enough normal force, call it 'invisible'.
		cop[X] = cop[Y] = cop[Z] = MISSING_DOUBLE;
		// But signal that it's not a valid COP by returning a negative distance.
		return( -1 );
	}
}


/***************************************************************************/

// Returns the grip force from the 3D force measurements of the sensors under each fingertip.
double DexAnalogMixin::ComputeGripForce( Vector3 &force1, Vector3 &force2 ) {
	// Force readings have been transformed into a common reference frame.
	// Take the average of two opposing forces.
	// NB: Using same axis definitions as Dex code provided by Bert.
	return(  ( force2[X] - force1[X] ) / 2.0 );
}

// Computes the net force (load force) acting on the manipulandum, based on the 
// measured 3D force vectors from the two ATI sensors. Result is returned in the
// 3D vector 'load', while the magnitude of the load is returned as a double.
double DexAnalogMixin::ComputeLoadForce( Vector3 &load, Vector3 &force1, Vector3 &force2 ) {
	// Compute the net force on the object.
	// If the reference frames have been properly aligned,
	// the net force on the object is simply the sum of the forces 
	// applied to either side.
	load[X] = force1[X] + force2[X];
	load[Y] = force1[Y] + force2[Y];
	load[Z] = force1[Z] + force2[Z];
	// Return the magnitude of the net force on the object.
	return( VectorNorm( load ) );
}

// Same as the above, but the load force normal to the sensors is ignored.
double DexAnalogMixin::ComputePlanarLoadForce( Vector3 &load, Vector3 &force1, Vector3 &force2 ) {
	// Compute the net force perpendicular to the pinch axis.
	ComputeLoadForce( load, force1, force2 );
	// Ignore the normal component.
	// NB: Using same axis definitions as Dex code provided by Bert.
	load[X] = 0.0;
	// Return the magnitude of the net force on the object.
	return( VectorNorm( load ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// Take a vector or a scalar, recursively filter it and return the filtered value.
// Filtering is the same for all quantities in the instance.

// Set the filter constant.
// The larger the filterConstant value, the more the data is smoothed.
// A filterConstant of zero results in no filtering.
void DexAnalogMixin::SetFilterConstant( double filter_constant ) {
	filterConstant = filter_constant;
}

// Read the current filter constant.
double DexAnalogMixin::GetFilterConstant( void ) {
	return( filterConstant );
}

// Vectors are filtered 'in place', i.e. a reference to a vector containing the new
//  measurement is provided as an input to the method and the filtered vector value
//  is returned in the same vector. The magnitude of the filtered vector is returned 
//  as a scalar value.

double DexAnalogMixin::FilterLoadForce( Vector3 load_force ) {
	// Combine the new force sample with previous filtered value (recursive filtering).
	ScaleVector( filteredLoadForce, filteredLoadForce, filterConstant );
	AddVectors( filteredLoadForce, filteredLoadForce, load_force );
	ScaleVector( filteredLoadForce, filteredLoadForce, 1.0 / (1.0 + filterConstant ));
	// Return the filtered value in place.
	CopyVector( load_force, filteredLoadForce );
	return( VectorNorm( filteredLoadForce ) );
}

double DexAnalogMixin::FilterCoP( int which_ati, Vector3 center_of_pressure ) {
	// Combine the new force sample with previous filtered value (recursive filtering).
	ScaleVector( filteredCoP[which_ati], filteredCoP[which_ati], filterConstant );
	AddVectors( filteredCoP[which_ati], filteredCoP[which_ati], center_of_pressure );
	ScaleVector( filteredCoP[which_ati], filteredCoP[which_ati], 1.0 / (1.0 + filterConstant ));
	// Return the filtered value in place.
	CopyVector( center_of_pressure, filteredCoP[which_ati] );
	return( VectorNorm( filteredCoP[which_ati] ) );
}

double DexAnalogMixin::FilterManipulandumPosition( Vector3 position ) {
	// Combine the new position sample with previous filtered value (recursive filtering).
	ScaleVector( filteredManipulandumPosition, filteredManipulandumPosition, filterConstant );
	AddVectors( filteredManipulandumPosition, filteredManipulandumPosition, position );
	ScaleVector( filteredManipulandumPosition, filteredManipulandumPosition, 1.0 / (1.0 + filterConstant ));
	// Return the filtered value in place.
	CopyVector( position, filteredManipulandumPosition );
	return( VectorNorm( position ) );
}

double DexAnalogMixin::FilterManipulandumRotations( Vector3 rotations ) {
	// Combine the new rotations sample with previous filtered value (recursive filtering).
	ScaleVector( filteredManipulandumRotations, filteredManipulandumRotations, filterConstant );
	AddVectors( filteredManipulandumRotations, filteredManipulandumRotations, rotations );
	ScaleVector( filteredManipulandumRotations, filteredManipulandumRotations, 1.0 / (1.0 + filterConstant ));
	// Return the filtered value in place.
	CopyVector( rotations, filteredManipulandumRotations );
	return( VectorNorm( rotations ) );
}

double DexAnalogMixin::FilterAcceleration( Vector3 acceleration ) {
	// Combine the new position sample with previous filtered value (recursive filtering).
	ScaleVector( filteredAcceleration, filteredAcceleration, filterConstant );
	AddVectors( filteredAcceleration, filteredAcceleration, acceleration );
	ScaleVector( filteredAcceleration, filteredAcceleration, 1.0 / (1.0 + filterConstant ));
	// Return the filtered value in place.
	CopyVector( acceleration, filteredAcceleration );
	return( VectorNorm( acceleration ) );
}

// When filtering a scalar value, the input value is not changed in the calling routine (call by value).
// Rather, the filtered value is returned as the return value of the method.
double DexAnalogMixin::FilterGripForce( double grip_force ) {
	filteredGripForce = (grip_force + filterConstant * filteredGripForce) / (1.0 + filterConstant );
	return( filteredGripForce );
}

// The normal force of each ATI sensor is filtered separately. So the input to this method includes
//  both the newly measured normal force and a specification of which ATI sensor (0 or 1).
double DexAnalogMixin::FilterNormalForce( double normal_force, int ati ) {
	if ( ati >= N_FORCE_TRANSDUCERS || ati < 0 ) {
		// This should not happen, but check if the caller specifies a valid sensor ID.
		return( MISSING_DOUBLE );
	}
	else {
		filteredNormalForce[ati] = (normal_force + filterConstant * filteredNormalForce[ati]) / (1.0 + filterConstant );
		return( filteredNormalForce[ati] );
	}
}


