#ifndef CAMERA_H
#define CAMERA_H

#include "d3dUtil.h"

class Camera
{
public:

	Camera();
	~Camera();

	DirectX::XMVECTOR GetPosition()const;
	DirectX::XMFLOAT3 GetPosition3f()const;
	void SetPosition(float x, float y, float z);
	void SetPosition(const DirectX::XMFLOAT3& v);
	
	// 获取相机的方向
	DirectX::XMVECTOR GetRight()const;
	DirectX::XMFLOAT3 GetRight3f()const;
	DirectX::XMVECTOR GetUp()const;
	DirectX::XMFLOAT3 GetUp3f()const;
	DirectX::XMVECTOR GetLook()const;
	DirectX::XMFLOAT3 GetLook3f()const;

	// 获取视锥体属性。
	float GetNearZ()const;
	float GetFarZ()const;
	float GetAspect()const;
	float GetFovY()const;
	float GetFovX()const;

	// 获取视图空间坐标中的近平面和远平面尺寸。
	float GetNearWindowWidth()const;
	float GetNearWindowHeight()const;
	float GetFarWindowWidth()const;
	float GetFarWindowHeight()const;
	
	// 设置视锥体。
	void SetLens(float fovY, float aspect, float zn, float zf);

	// 通过 LookAt 参数定义相机空间。
	void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
	void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);

	DirectX::XMMATRIX GetView()const;
	DirectX::XMMATRIX GetProj()const;

	DirectX::XMFLOAT4X4 GetView4x4f()const;
	DirectX::XMFLOAT4X4 GetProj4x4f()const;

	// 扫射/将相机移动一段距离 d。
	void Strafe(float d);
	void Walk(float d);

	// 旋转
	void Pitch(float angle);
	void RotateY(float angle);

	//修改相机位置/方向后，调用以重建视图矩阵。
	void UpdateViewMatrix();

private:

	// 具有相对于世界空间的坐标的相机坐标系
	DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

	//缓存视锥体属性
	float mNearZ = 0.0f;
	float mFarZ = 0.0f;
	float mAspect = 0.0f;
	float mFovY = 0.0f;
	float mNearWindowHeight = 0.0f;
	float mFarWindowHeight = 0.0f;

	bool mViewDirty = true;

	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();
};

#endif // CAMERA_H