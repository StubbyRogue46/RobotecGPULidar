#include <cuda_runtime.h>
#include <math_constants.h>
#include <optix_device.h>

#include <math/Vector.hpp>
#include <math/Mat3x4f.hpp>
#include <cassert>

#include <gpu/RaytraceRequestContext.hpp>
#include <gpu/ShaderBindingTableTypes.h>


extern "C" static __constant__ RaytraceRequestContext ctx;

struct Vec3fPayload
{
	unsigned p0;
	unsigned p1;
	unsigned p2;
};

__forceinline__ __device__
Vec3fPayload encodePayloadVec3f(const Vec3f& src)
{
	Vec3fPayload dst;
	dst.p0 = *(reinterpret_cast<const unsigned*>(&src[0]));
	dst.p1 = *(reinterpret_cast<const unsigned*>(&src[1]));
	dst.p2 = *(reinterpret_cast<const unsigned*>(&src[2]));
	return dst;
}

__forceinline__ __device__
Vec3f decodePayloadVec3f(const Vec3fPayload& src)
{
	return Vec3f {
		*reinterpret_cast<const float*>(&src.p0),
		*reinterpret_cast<const float*>(&src.p1),
		*reinterpret_cast<const float*>(&src.p2),
	};
}


template<bool isFinite>
__forceinline__ __device__
void saveRayResult(Vec3f* xyz=nullptr)
{
	const int rayIdx = optixGetLaunchIndex().x;
	Vec3f origin = decodePayloadVec3f({
		.p0 = optixGetPayload_0(),
		.p1 = optixGetPayload_1(),
		.p2 = optixGetPayload_2()
	});

	if (ctx.xyz != nullptr) {
		// Return actual XYZ of the hit point or infinity vector with signs of the ray.
		ctx.xyz[rayIdx] = isFinite ? *xyz : ctx.rays[rayIdx] * Vec3f{CUDART_INF_F, CUDART_INF_F, CUDART_INF_F};
	}
	if (ctx.isHit != nullptr) {
		ctx.isHit[rayIdx] = isFinite;
	}
	if (ctx.rayIdx != nullptr) {
		ctx.rayIdx[rayIdx] = rayIdx;
	}
	if (ctx.ringIdx != nullptr && ctx.ringIds != nullptr) {
		ctx.ringIdx[rayIdx] = ctx.ringIds[rayIdx % ctx.ringIdsCount];
	}
	if (ctx.distanceIdx != nullptr) {
		ctx.distanceIdx[rayIdx] = isFinite
		                        ? sqrt(pow(
		                            (*xyz)[0] - origin[0], 2) +
		                            pow((*xyz)[1] - origin[1], 2) +
		                            pow((*xyz)[2] - origin[2], 2))
		                        : CUDART_INF_F;
	}
	if (ctx.intensityIdx != nullptr) {
		ctx.intensityIdx[rayIdx] = 100;
	}
}

extern "C" __global__ void __raygen__()
{
	Mat3x4f ray = ctx.rays[optixGetLaunchIndex().x];

	Vec3f origin = ray * Vec3f{0, 0, 0};
	Vec3f dir = ray * Vec3f{0, 0, 1} - origin;

	unsigned int flags = OPTIX_RAY_FLAG_DISABLE_ANYHIT;
	Vec3fPayload originPayload = encodePayloadVec3f(origin);
	optixTrace(ctx.scene, origin, dir, 0.0f, ctx.rayRange, 0.0f, OptixVisibilityMask(255), flags, 0, 1, 0,
	           originPayload.p0, originPayload.p1, originPayload.p2);
}

extern "C" __global__ void __closesthit__()
{
	const TriangleMeshSBTData& sbtData = *(const TriangleMeshSBTData*) optixGetSbtDataPointer();

	const int primID = optixGetPrimitiveIndex();
	assert(primID < sbtData.index_count);
	const Vec3i index = sbtData.index[primID];
	const float u = optixGetTriangleBarycentrics().x;
	const float v = optixGetTriangleBarycentrics().y;

	assert(index.x() < sbtData.vertex_count);
	assert(index.y() < sbtData.vertex_count);
	assert(index.z() < sbtData.vertex_count);
	const Vec3f& A = sbtData.vertex[index.x()];
	const Vec3f& B = sbtData.vertex[index.y()];
	const Vec3f& C = sbtData.vertex[index.z()];

	Vec3f hitObject = Vec3f((1 - u - v) * A + u * B + v * C);
	Vec3f hitWorld = optixTransformPointFromObjectToWorldSpace(hitObject);

	saveRayResult<true>(&hitWorld);
}

extern "C" __global__ void __miss__()
{
	saveRayResult<false>();
}

extern "C" __global__ void __anyhit__(){}