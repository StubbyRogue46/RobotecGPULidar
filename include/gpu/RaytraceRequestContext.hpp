#pragma once

#include <optix.h>
#include <RGLFields.hpp>

struct RaytraceRequestContext
{
	// Input
	const Mat3x4f* rays;
	size_t rayCount;

	Mat3x4f rayOriginToWorld;
	float rayRange;

	const int* ringIds;
	size_t ringIdsCount;

	OptixTraversableHandle scene;

	// Output
	Field<XYZ_F32>::type* xyz;
	Field<IS_HIT_I32>::type* isHit;
	Field<RAY_IDX_U32>::type* rayIdx;
	Field<RING_ID_U16>::type* ringIdx;
	Field<DISTANCE_F32>::type* distanceIdx;
	Field<INTENSITY_F32>::type* intensityIdx;
};