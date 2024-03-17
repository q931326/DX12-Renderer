#include "d3dApp.h"
#include <WindowsX.h>

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

D3DApp* D3DApp::mApp = nullptr;
D3DApp* D3DApp::GetApp()
{
    return mApp;
}

D3DApp::D3DApp(HINSTANCE hInstance)
:	mhAppInst(hInstance)
{
    // 只能构造一个 D3DApp
    assert(mApp == nullptr);
    mApp = this;
}

D3DApp::~D3DApp()
{
	if(md3dDevice != nullptr)
		FlushCommandQueue();
}

HINSTANCE D3DApp::AppInst()const
{
	return mhAppInst;
}

HWND D3DApp::MainWnd()const
{
	return mhMainWnd;
}

float D3DApp::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

bool D3DApp::Get4xMsaaState()const
{
    return m4xMsaaState;
}

void D3DApp::Set4xMsaaState(bool value)
{
    if(m4xMsaaState != value)
    {
        m4xMsaaState = value;

        // 使用新的多采样设置重新创建交换链和缓冲区
        CreateSwapChain();
        OnResize();
    }
}

int D3DApp::Run()
{
	MSG msg = {0};
 
	mTimer.Reset();

	while(msg.message != WM_QUIT)
	{
		// 如果有 Window 消息，请处理它们
		if(PeekMessage( &msg, 0, 0, 0, PM_REMOVE ))
		{
            TranslateMessage( &msg );
            DispatchMessage( &msg );
		}
		// 否则，做动画/游戏的东西
		else
        {	
			mTimer.Tick();

			if( !mAppPaused )
			{
				CalculateFrameStats();
				Update(mTimer);	
                Draw(mTimer);
			}
			else
			{
				Sleep(100);
			}
        }
    }

	return (int)msg.wParam;
}

bool D3DApp::Initialize()
{
	if(!InitMainWindow())
		return false;

	if(!InitDirect3D())
		return false;

    // 执行初始调整大小代码
    OnResize();

	return true;
}
 
void D3DApp::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;	//设置 NodeMask 为0意味着这个描述符堆可以在任何GPU节点上使用
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
        &rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));


    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
        &dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void D3DApp::OnResize()
{
	assert(md3dDevice);
	assert(mSwapChain);
    assert(mDirectCmdListAlloc);

	// 在更改任何资源之前刷新命令队列
	FlushCommandQueue();

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// 释放我们将重新创建的先前资源。
	for (int i = 0; i < SwapChainBufferCount; ++i)
		mSwapChainBuffer[i].Reset();
    mDepthStencilBuffer.Reset();
	
	// 调整交换链的大小
    ThrowIfFailed(mSwapChain->ResizeBuffers(
		SwapChainBufferCount, 
		mClientWidth, mClientHeight, 
		mBackBufferFormat, 
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));	//允许在运行时更改显示模式（如分辨率、刷新率等）

	mCurrBackBuffer = 0;
 
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
		md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

    // 深度模板视图
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = mClientWidth;
    depthStencilDesc.Height = mClientHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;

	// SSAO 需要 SRV 到深度缓冲区才能读取深度缓冲区。 因此，因为我们需要为同一资源创建两个视图：
	//1.SRV格式：DXGI_FORMAT_R24_UNORM_X8_TYPELESS 2.DSV 格式：DXGI_FORMAT_D24_UNORM_S8_UINT
	//我们需要创建具有无类型格式的深度缓冲区资源。
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

    depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = mDepthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;
    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

    //使用资源的格式创建整个资源的 mip 级别 0 的描述符。
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
    md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

    // 将资源从其初始状态转换为用作深度缓冲区
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	// 更新视口转换以覆盖工作区
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width    = static_cast<float>(mClientWidth);
	mScreenViewport.Height   = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

    mScissorRect = { 0, 0, mClientWidth, mClientHeight };
}
 
LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch( msg )
	{
	//	WM_ACTIVATE在窗口被激活或停用时发送。 当窗口被停用时，我们会暂停游戏，
	// 当它变为活动状态时，我们会取消暂停游戏。 
	case WM_ACTIVATE:
		if( LOWORD(wParam) == WA_INACTIVE )
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;

	// WM_SIZE在用户调整窗口大小时发送。
	case WM_SIZE:
		// 保存新的工作区尺寸。
		mClientWidth  = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if( md3dDevice )
		{
			if( wParam == SIZE_MINIMIZED )
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if( wParam == SIZE_MAXIMIZED )
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if( wParam == SIZE_RESTORED )
			{
				
				// 从最小化状态恢复
				if( mMinimized )
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}

				// 从最大化状态恢复
				else if( mMaximized )
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if( mResizing )
				{
					//如果用户正在拖动调整大小条，我们不会在此处调整缓冲区的大小，因为当用户不
					// 断拖动调整大小条时，会向窗口发送WM_SIZE消息流，并且对于拖动调整大小条收
					// 到的每条WM_SIZE消息调整大小是没有意义的（而且很慢）。 因此，相反，我们
					// 在用户完成调整窗口大小并释放调整大小栏后进行重置，这会发送一条
					// WM_EXITSIZEMOVE消息。
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					OnResize();
				}
			}
		}
		return 0;

	// 当用户抓取调整大小的条形图时，将发送WM_EXITSIZEMOVE。
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing  = true;
		mTimer.Stop();
		return 0;

	//WM_EXITSIZEMOVE在用户释放调整大小条时发送。
	//在这里，我们根据新的窗口尺寸重置所有内容。
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing  = false;
		mTimer.Start();
		OnResize();
		return 0;
 
	// WM_DESTROY在窗口被关闭时发送。
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	//当菜单处于活动状态并且用户按WM_MENUCHAR与任何助记键或加速键不对应的键。
	case WM_MENUCHAR:
        return MAKELRESULT(0, MNC_CLOSE);

	// 捕获此消息，以防止窗口变得太小
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200; 
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
    case WM_KEYUP:
        if(wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        else if((int)wParam == VK_F2)
            Set4xMsaaState(!m4xMsaaState);

        return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool D3DApp::InitMainWindow()
{
	WNDCLASS wc;
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = MainWndProc; 
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = mhAppInst;
	wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = L"MainWnd";

	if( !RegisterClass(&wc) )
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, mClientWidth, mClientHeight };
    AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width  = R.right - R.left;
	int height = R.bottom - R.top;

	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(), 
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0); 
	if( !mhMainWnd )
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	return true;
}

bool D3DApp::InitDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
{
	ComPtr<ID3D12Debug> debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
}
#endif

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

	// Try to create hardware device.
	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,             // default adapter
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&md3dDevice));

	// Fallback to WARP device.
	if(FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&md3dDevice)));
	}

	ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&mFence)));

	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//检查我们的后台缓冲区格式的 4X MSAA 质量支持。所有支持 Direct3D 11 的设备都支持所有呈现
	// 目标格式的 4X MSAA，因此我们只需要检查质量支持。

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

    m4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");
	
#ifdef _DEBUG
    LogAdapters();
#endif

	CreateCommandObjects();
    CreateSwapChain();
    CreateRtvAndDsvDescriptorHeaps();

	return true;
}

void D3DApp::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mDirectCmdListAlloc.Get(), // 关联的命令分配器
		nullptr,                   // 初始管道状态对象
		IID_PPV_ARGS(mCommandList.GetAddressOf())));

	//从关闭状态开始。 这是因为我们第一次引用命令列表时，我们会重置它，并且在调用 Reset 之前需要关闭它。
	mCommandList->Close();
}

void D3DApp::CreateSwapChain()
{
    // 释放我们将要重新创建的上一个交换链。
    mSwapChain.Reset();

    DXGI_SWAP_CHAIN_DESC sd;
    sd.BufferDesc.Width = mClientWidth;
    sd.BufferDesc.Height = mClientHeight;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = mBackBufferFormat;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = SwapChainBufferCount;
    sd.OutputWindow = mhMainWnd;
    sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	//注意：交换链使用队列来执行刷新。
    ThrowIfFailed(mdxgiFactory->CreateSwapChain(
		mCommandQueue.Get(),
		&sd, 
		mSwapChain.GetAddressOf()));
}

void D3DApp::FlushCommandQueue()
{
	// 将 fence 值向前推进以将命令标记到此围栏点
    mCurrentFence++;

    // 向命令队列添加一条指令以设置新的围栏点。 由于我们位于 GPU 时间轴上，因此在 GPU 完成处理
	// 此 Signal（） 之前的所有命令之前，不会设置新的围栏点。
    ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	// 等到 GPU 完成命令，直到此围栏点
    if(mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

        //当 GPU 击中当前围栏时触发事件
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

        // 等到 GPU 命中当前围栏事件被触发
		WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
	}
}

ID3D12Resource* D3DApp::CurrentBackBuffer()const
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView()const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurrBackBuffer,
		mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView()const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3DApp::CalculateFrameStats()
{
	// 代码计算每秒平均帧数，以及呈现一帧所需的平均时间。 这些统计信息将附加到窗口标题栏。
    
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// 计算一秒内的平均值。
	if( (mTimer.TotalTime() - timeElapsed) >= 1.0f )
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

        wstring fpsStr = to_wstring(fps);
        wstring mspfStr = to_wstring(mspf);

        wstring windowText = mMainWndCaption +
            L"    fps: " + fpsStr +
            L"   mspf: " + mspfStr;

        SetWindowText(mhMainWnd, windowText.c_str());
		
		// 重置为下一个平均值。
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

void D3DApp::LogAdapters()
{
    UINT i = 0;
    IDXGIAdapter* adapter = nullptr;
    std::vector<IDXGIAdapter*> adapterList;
    while(mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        std::wstring text = L"***Adapter: ";
        text += desc.Description;
        text += L"\n";

        OutputDebugString(text.c_str());

        adapterList.push_back(adapter);
        
        ++i;
    }

    for(size_t i = 0; i < adapterList.size(); ++i)
    {
        LogAdapterOutputs(adapterList[i]);
        ReleaseCom(adapterList[i]);
    }
}

void D3DApp::LogAdapterOutputs(IDXGIAdapter* adapter)
{
    UINT i = 0;
    IDXGIOutput* output = nullptr;
    while(adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_OUTPUT_DESC desc;
        output->GetDesc(&desc);
        
        std::wstring text = L"***Output: ";
        text += desc.DeviceName;
        text += L"\n";
        OutputDebugString(text.c_str());

        LogOutputDisplayModes(output, mBackBufferFormat);

        ReleaseCom(output);

        ++i;
    }
}

void D3DApp::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
    UINT count = 0;
    UINT flags = 0;

    // Call with nullptr to get list count.
    output->GetDisplayModeList(format, flags, &count, nullptr);

    std::vector<DXGI_MODE_DESC> modeList(count);
    output->GetDisplayModeList(format, flags, &count, &modeList[0]);

    for(auto& x : modeList)
    {
        UINT n = x.RefreshRate.Numerator;
        UINT d = x.RefreshRate.Denominator;
        std::wstring text =
            L"Width = " + std::to_wstring(x.Width) + L" " +
            L"Height = " + std::to_wstring(x.Height) + L" " +
            L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
            L"\n";

        ::OutputDebugString(text.c_str());
    }
}