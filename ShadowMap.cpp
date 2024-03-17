#include "ShadowMap.h"
 
ShadowMap::ShadowMap(ID3D12Device* device, UINT width, UINT height)
{
	md3dDevice = device;

	mWidth = width;
	mHeight = height;

	mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	mScissorRect = { 0, 0, (int)width, (int)height };

	BuildResource();
}

UINT ShadowMap::Width()const
{
    return mWidth;
}

UINT ShadowMap::Height()const
{
    return mHeight;
}

ID3D12Resource*  ShadowMap::Resource()
{
	return mShadowMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ShadowMap::Srv()const
{
	return mhGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMap::Dsv()const
{
	return mhCpuDsv;
}

D3D12_VIEWPORT ShadowMap::Viewport()const
{
	return mViewport;
}

D3D12_RECT ShadowMap::ScissorRect()const
{
	return mScissorRect;
}

void ShadowMap::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
	                             CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
	                             CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
	// 保存对描述符的引用
	mhCpuSrv = hCpuSrv;
	mhGpuSrv = hGpuSrv;
    mhCpuDsv = hCpuDsv;

	//  创建描述符
	BuildDescriptors();
}

void ShadowMap::OnResize(UINT newWidth, UINT newHeight)
{
	if((mWidth != newWidth) || (mHeight != newHeight))
	{
		mWidth = newWidth;
		mHeight = newHeight;

		BuildResource();

		// 新资源，因此我们需要该资源的新描述符
		BuildDescriptors();
	}
}
 
void ShadowMap::BuildDescriptors()
{
    // 将 SRV 创建到资源，以便我们可以在着色器程序中对阴影贴图进行采样
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; 
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    srvDesc.Texture2D.PlaneSlice = 0;
    md3dDevice->CreateShaderResourceView(mShadowMap.Get(), &srvDesc, mhCpuSrv);

	// 创建 DSV 到资源，以便我们可以渲染到阴影贴图
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc; 
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.Texture2D.MipSlice = 0;
	md3dDevice->CreateDepthStencilView(mShadowMap.Get(), &dsvDesc, mhCpuDsv);
}

void ShadowMap::BuildResource()
{
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = mFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(&mShadowMap)));
}
/*
// 为阴影图构建PCF,返回阴影因子
float CalcShadowFactor(float4 shadowPosH)
{
	// 将顶点变换到NDC空间（如果是正交投影，则W=1）
	shadowPosH.xyz /= shadowPosH.w;

	// NDC空间中的深度值
	float depth = shadowPosH.z;

	// 读取ShadowMap的宽高及mip级数
	uint width, height, numMips;
	gShadowMap.GetDimensions(0, width, height, numMips);

	// 纹素尺寸
	float dx = 1.0f / (float)width;

	float percentLit = 0.0f;
	// 使用9核
	const float2 offsets[9] =
	{
		float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
	};

	// PCF（类似均值模糊算法）
	[unroll]
		// 执行9次4-tap PCF
		for (int i = 0; i < 9; ++i)
		{
			// 每个核都执行tap4 PCF计算
			percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
				shadowPosH.xy + offsets[i], depth).r;
		}
	// 将9次PCF取均值
	return percentLit / 9.0f;
	*/
