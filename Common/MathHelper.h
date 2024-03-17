#pragma once

#include <Windows.h>
#include <DirectXMath.h>
#include <cstdint>

class MathHelper
{
public:
	// Returns random float in [0, 1).
	static float RandF()
	{
		return (float)(rand()) / (float)RAND_MAX;
	}

	// Returns random float in [a, b).
	static float RandF(float a, float b)
	{
		return a + RandF()*(b-a);
	}

    static int Rand(int a, int b)
    {
        return a + rand() % ((b - a) + 1);
    }

	template<typename T>
	static T Min(const T& a, const T& b)
	{
		return a < b ? a : b;
	}

	template<typename T>
	static T Max(const T& a, const T& b)
	{
		return a > b ? a : b;
	}
	 
	template<typename T>
	static T Lerp(const T& a, const T& b, float t)
	{
		return a + (b-a)*t;
	}

	template<typename T>
	static T Clamp(const T& x, const T& low, const T& high)
	{
		return x < low ? low : (x > high ? high : x); 
	}

	// ���������Ͳ��� x �� y������һ���� [0, 2*PI] ��Χ�ڵļ�����Ƕ�ֵ��
	static float AngleFromXY(float x, float y);
	//����������ת��Ϊ�ѿ������꣨theta�Ǵ���Z�ᵽ������ƽ��ļнǣ���Χͨ���� [0, 2��)��
	// phi�Ǵ���X�ᵽ������Ӧ����ƽ���ϵ����ߵļнǣ���Χ�� [0, ��]��
	static DirectX::XMVECTOR SphericalToCartesian(float radius, float theta, float phi)
	{
		return DirectX::XMVectorSet(
			radius*sinf(phi)*cosf(theta),
			radius*cosf(phi),
			radius*sinf(phi)*sinf(theta),
			1.0f);
	}

    static DirectX::XMMATRIX InverseTranspose(DirectX::CXMMATRIX M)
	{
		//��ģ�;�����ת��ƽ�Ʊ任��Ϊ����ȷ�ؼ������Ч������Ҫ�Զ��㷨��Ҳ������Ӧ�ı任��
		// ��������ת�任��˵����ȷ�ı任��ʽ�Ƕ�ģ�͵�����ת�þ�����Է���������
		// ������ֱ��ʹ��ģ�͵������
		//����Ӱ��ᱻ�������ر�����ԷǾ������ŵ��������ֹ�������������ѹ����
		//��ת����ȷ��Ӧ���ڷ�������ʹ�䱣����任��ı��洹ֱ��
        DirectX::XMMATRIX A = M;
        A.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

        DirectX::XMVECTOR det = DirectX::XMMatrixDeterminant(A);
        return DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&det, A));
	}

    static DirectX::XMFLOAT4X4 Identity4x4()
    {
        static DirectX::XMFLOAT4X4 I(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);

        return I;
    }

    static DirectX::XMVECTOR RandUnitVec3();
    static DirectX::XMVECTOR RandHemisphereUnitVec3(DirectX::XMVECTOR n);

	static const float Infinity;
	static const float Pi;


};

