#pragma once
#include <wrl\client.h>
#include <d3d11.h>
#include <SpriteFont.h>

#include "Camera.h"
#include "Sky.h"
#include "GameEntity.h"
#include "Lights.h"

class Renderer
{
public:
	Renderer(
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain,
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV,
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV,
		unsigned int windowWidth,
		unsigned int windowHeight,
		Sky* sky,
		const std::vector<GameEntity*>& entities,
		const std::vector<Light>& lights, 
		SimpleVertexShader* lightVS,
		SimplePixelShader* lightPS);
	void PostResize(
		unsigned int windowWidth,
		unsigned int windowHeight,
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV,
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV);
	void Render(
		Camera* camera, 
		Mesh* lightMesh, 
		DirectX::SpriteFont* arial, 
		DirectX::SpriteBatch* spriteBatch);
private:
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV;
	unsigned int windowWidth;
	unsigned int windowHeight;
	Sky* sky;
	const std::vector<GameEntity*>& entities;
	const std::vector<Light>& lights;
	SimpleVertexShader* lightVS;
	SimplePixelShader* lightPS;

	void DrawPointLights(
		Camera* camera, 
		Mesh* lightMesh);
	void DrawUI(
		DirectX::SpriteFont* arial, 
		DirectX::SpriteBatch* spriteBatch);
};

