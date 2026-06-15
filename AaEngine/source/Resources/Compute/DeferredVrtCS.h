#pragma once

#include "Resources/Compute/ComputeShader.h"
#include "Utils/MathUtils.h"

class DeferredVrtGenerateRaysCS : public ComputeShader
{
public:
	struct DispatchParams
	{
		Matrix InvViewProjectionMatrix;
		Vector3 CameraPosition;
		float Time;
		XMUINT2 ViewportSize;
		UINT TexIdNormal;
		UINT TexIdDepth;
		UINT RayIndex;
		UINT RayCount;
		UINT OutputQueueIndex;
		UINT Padding0;
	};

	void dispatch(ID3D12GraphicsCommandList* commandList, const DispatchParams& params, ID3D12Resource* outputRays, ID3D12Resource* rayResults, ID3D12Resource* queueState, ID3D12Resource* dispatchArgs);
};

class DeferredVrtTraceRayCS : public ComputeShader
{
public:
	struct DispatchParams
	{
		Matrix InvViewProjectionMatrix;
		Vector3 CameraPosition;
		float Time;
		XMUINT2 ViewportSize;
		UINT TexIdNormal;
		UINT TexIdDepth;
		UINT CascadeIndex;
		UINT InputQueueIndex;
		UINT OutputQueueIndex;
		UINT IsLastCascade;
	};

	void dispatchIndirect(ID3D12GraphicsCommandList* commandList, ID3D12CommandSignature* commandSignature, const DispatchParams& params, D3D12_GPU_VIRTUAL_ADDRESS voxelInfo, ID3D12Resource* inputRays, ID3D12Resource* outputRays, ID3D12Resource* rayResults, ID3D12Resource* queueState, ID3D12Resource* outputDispatchArgs, ID3D12Resource* indirectDispatchArgs);
};

class DeferredVrtCoarseTraceRayCS : public ComputeShader
{
public:
	struct DispatchParams
	{
		Matrix InvViewProjectionMatrix;
		Vector3 CameraPosition;
		float Time;
		XMUINT2 ViewportSize;
		UINT TexIdNormal;
		UINT TexIdDepth;
		UINT CascadeIndex;
		UINT InputQueueIndex;
		UINT HitQueueIndex;
		UINT BypassQueueIndex;
		UINT IsLastCascade;
		UINT CoarseMip;
		UINT Padding0;
		UINT Padding1;
	};

	void dispatchIndirect(ID3D12GraphicsCommandList* commandList, ID3D12CommandSignature* commandSignature, const DispatchParams& params, D3D12_GPU_VIRTUAL_ADDRESS voxelInfo, ID3D12Resource* inputRays, ID3D12Resource* hitRays, ID3D12Resource* bypassRays, ID3D12Resource* rayResults, ID3D12Resource* queueState, ID3D12Resource* hitDispatchArgs, ID3D12Resource* bypassDispatchArgs, ID3D12Resource* indirectDispatchArgs);
};

class DeferredVrtCollectRaysCS : public ComputeShader
{
public:
	struct DispatchParams
	{
		XMUINT2 ViewportSize;
		UINT ResIdOutput;
		UINT Padding0;
	};

	void dispatch(ID3D12GraphicsCommandList* commandList, const DispatchParams& params, ID3D12Resource* rayResults);
};

class DeferredVrtResetQueueCS : public ComputeShader
{
public:
	struct DispatchParams
	{
		UINT QueueIndex;
		UINT Padding0;
		UINT Padding1;
		UINT Padding2;
	};

	void dispatch(ID3D12GraphicsCommandList* commandList, UINT queueIndex, ID3D12Resource* queueState, ID3D12Resource* dispatchArgs);
};