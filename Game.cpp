
#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)

#include "Game.h"
#include "Vertex.h"
#include "Input.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "WICTextureLoader.h"


// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// Helper macro for getting a float between min and max
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

// Helper macros for making texture and shader loading code more succinct
#define LoadTexture(file, srv) CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(file).c_str(), 0, srv.GetAddressOf())
#define LoadShader(type, file) new type(device.Get(), context.Get(), GetFullPathTo_Wide(file).c_str())


// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore(
		hInstance,			// The application's handle
		"DirectX Game",		// Text for the window's title bar
		1280,				// Width of the window's client area
		720,				// Height of the window's client area
		true)				// Show extra stats (fps) in title bar?
{
	camera = 0;

	// Seed random
	srand((unsigned int)time(0));

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif

}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	// ImGui clean up
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// Note: Since we're using smart pointers (ComPtr),
	// we don't need to explicitly clean up those DirectX objects
	// - If we weren't using smart pointers, we'd need
	//   to call Release() on each DirectX object

	// Clean up our other resources
	for (auto& m : meshes) delete m;
	for (auto& s : shaders) delete s; 
	for (auto& m : materials) delete m;
	for (auto& e : entities) delete e;

	// Delete any one-off objects
	delete renderer;
	delete sky;
	delete camera;
	delete arial;
	delete spriteBatch;

	// Delete singletons
	delete& Input::GetInstance();
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Initialize the input manager with the window's handle
	Input::GetInstance().Initialize(this->hWnd);

	// Asset loading and entity creation
	LoadAssetsAndCreateEntities();
	
	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set up lights initially
	lightCount = 64;
	GenerateLights();

	// Make our camera
	camera = new Camera(
		0, 0, -10,	// Position
		3.0f,		// Move speed
		1.0f,		// Mouse look
		this->width / (float)this->height); // Aspect ratio

	// Initialize ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	// Pick a style (uncomment one of these 3)
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();
	//ImGui::StyleColorsClassic();
	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device.Get(), context.Get());
}

// --------------------------------------------------------
// Load all assets and create materials, entities, etc.
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	// Load shaders using our succinct LoadShader() macro
	SimpleVertexShader* vertexShader	= LoadShader(SimpleVertexShader, L"VertexShader.cso");
	SimplePixelShader* pixelShader		= LoadShader(SimplePixelShader, L"PixelShader.cso");
	SimplePixelShader* pixelShaderPBR	= LoadShader(SimplePixelShader, L"PixelShaderPBR.cso");
	SimplePixelShader* solidColorPS		= LoadShader(SimplePixelShader, L"SolidColorPS.cso");
	
	SimpleVertexShader* skyVS = LoadShader(SimpleVertexShader, L"SkyVS.cso");
	SimplePixelShader* skyPS  = LoadShader(SimplePixelShader, L"SkyPS.cso");

	shaders.push_back(vertexShader);
	shaders.push_back(pixelShader);
	shaders.push_back(pixelShaderPBR);
	shaders.push_back(solidColorPS);
	shaders.push_back(skyVS);
	shaders.push_back(skyPS);

	// Set up the sprite batch and load the sprite font
	spriteBatch = new SpriteBatch(context.Get());
	arial = new SpriteFont(device.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/arial.spritefont").c_str());

	// Make the meshes
	Mesh* sphereMesh = new Mesh(GetFullPathTo("../../Assets/Models/sphere.obj").c_str(), device);
	Mesh* helixMesh = new Mesh(GetFullPathTo("../../Assets/Models/helix.obj").c_str(), device);
	Mesh* cubeMesh = new Mesh(GetFullPathTo("../../Assets/Models/cube.obj").c_str(), device);
	Mesh* coneMesh = new Mesh(GetFullPathTo("../../Assets/Models/cone.obj").c_str(), device);

	meshes.push_back(sphereMesh);
	meshes.push_back(helixMesh);
	meshes.push_back(cubeMesh);
	meshes.push_back(coneMesh);

	
	// Declare the textures we'll need
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cobbleA,  cobbleN,  cobbleR,  cobbleM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorA,  floorN,  floorR,  floorM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> paintA,  paintN,  paintR,  paintM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scratchedA,  scratchedN,  scratchedR,  scratchedM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bronzeA,  bronzeN,  bronzeR,  bronzeM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughA,  roughN,  roughR,  roughM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodA,  woodN,  woodR,  woodM;

	// Load the textures using our succinct LoadTexture() macro
	LoadTexture(L"../../Assets/Textures/cobblestone_albedo.png", cobbleA);
	LoadTexture(L"../../Assets/Textures/cobblestone_normals.png", cobbleN);
	LoadTexture(L"../../Assets/Textures/cobblestone_roughness.png", cobbleR);
	LoadTexture(L"../../Assets/Textures/cobblestone_metal.png", cobbleM);

	LoadTexture(L"../../Assets/Textures/floor_albedo.png", floorA);
	LoadTexture(L"../../Assets/Textures/floor_normals.png", floorN);
	LoadTexture(L"../../Assets/Textures/floor_roughness.png", floorR);
	LoadTexture(L"../../Assets/Textures/floor_metal.png", floorM);
	
	LoadTexture(L"../../Assets/Textures/paint_albedo.png", paintA);
	LoadTexture(L"../../Assets/Textures/paint_normals.png", paintN);
	LoadTexture(L"../../Assets/Textures/paint_roughness.png", paintR);
	LoadTexture(L"../../Assets/Textures/paint_metal.png", paintM);
	
	LoadTexture(L"../../Assets/Textures/scratched_albedo.png", scratchedA);
	LoadTexture(L"../../Assets/Textures/scratched_normals.png", scratchedN);
	LoadTexture(L"../../Assets/Textures/scratched_roughness.png", scratchedR);
	LoadTexture(L"../../Assets/Textures/scratched_metal.png", scratchedM);
	
	LoadTexture(L"../../Assets/Textures/bronze_albedo.png", bronzeA);
	LoadTexture(L"../../Assets/Textures/bronze_normals.png", bronzeN);
	LoadTexture(L"../../Assets/Textures/bronze_roughness.png", bronzeR);
	LoadTexture(L"../../Assets/Textures/bronze_metal.png", bronzeM);
	
	LoadTexture(L"../../Assets/Textures/rough_albedo.png", roughA);
	LoadTexture(L"../../Assets/Textures/rough_normals.png", roughN);
	LoadTexture(L"../../Assets/Textures/rough_roughness.png", roughR);
	LoadTexture(L"../../Assets/Textures/rough_metal.png", roughM);
	
	LoadTexture(L"../../Assets/Textures/wood_albedo.png", woodA);
	LoadTexture(L"../../Assets/Textures/wood_normals.png", woodN);
	LoadTexture(L"../../Assets/Textures/wood_roughness.png", woodR);
	LoadTexture(L"../../Assets/Textures/wood_metal.png", woodM);

	// Describe and create our sampler state
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampDesc.MaxAnisotropy = 16;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&sampDesc, samplerOptions.GetAddressOf());


	// Create the sky using a DDS cube map
	/*sky = new Sky(
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\SunnyCubeMap.dds").c_str(),
		cubeMesh,
		skyVS,
		skyPS,
		samplerOptions,
		device,
		context);*/

	// Create the sky using 6 images
	sky = new Sky(
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Night\\right.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Night\\left.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Night\\up.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Night\\down.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Night\\front.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Night\\back.png").c_str(),
		cubeMesh,
		skyVS,
		skyPS,
		samplerOptions,
		device,
		context);

	// Create basic materials
	Material* cobbleMat2x = new Material(vertexShader, pixelShader, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), cobbleA, cobbleN, cobbleR, cobbleM, samplerOptions);
	Material* floorMat = new Material(vertexShader, pixelShader, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), floorA, floorN, floorR, floorM, samplerOptions);
	Material* paintMat = new Material(vertexShader, pixelShader, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), paintA, paintN, paintR, paintM, samplerOptions);
	Material* scratchedMat = new Material(vertexShader, pixelShader, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), scratchedA, scratchedN, scratchedR, scratchedM, samplerOptions);
	Material* bronzeMat = new Material(vertexShader, pixelShader, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), bronzeA, bronzeN, bronzeR, bronzeM, samplerOptions);
	Material* roughMat = new Material(vertexShader, pixelShader, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), roughA, roughN, roughR, roughM, samplerOptions);
	Material* woodMat = new Material(vertexShader, pixelShader, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), woodA, woodN, woodR, woodM, samplerOptions);

	materials.push_back(cobbleMat2x);
	materials.push_back(floorMat);
	materials.push_back(paintMat);
	materials.push_back(scratchedMat);
	materials.push_back(bronzeMat);
	materials.push_back(roughMat);
	materials.push_back(woodMat);

	// Create PBR materials
	Material* cobbleMat2xPBR = new Material(vertexShader, pixelShaderPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), cobbleA, cobbleN, cobbleR, cobbleM, samplerOptions);
	Material* floorMatPBR = new Material(vertexShader, pixelShaderPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), floorA, floorN, floorR, floorM, samplerOptions);
	Material* paintMatPBR = new Material(vertexShader, pixelShaderPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), paintA, paintN, paintR, paintM, samplerOptions);
	Material* scratchedMatPBR = new Material(vertexShader, pixelShaderPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), scratchedA, scratchedN, scratchedR, scratchedM, samplerOptions);
	Material* bronzeMatPBR = new Material(vertexShader, pixelShaderPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), bronzeA, bronzeN, bronzeR, bronzeM, samplerOptions);
	Material* roughMatPBR = new Material(vertexShader, pixelShaderPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), roughA, roughN, roughR, roughM, samplerOptions);
	Material* woodMatPBR = new Material(vertexShader, pixelShaderPBR, XMFLOAT4(1, 1, 1, 1), 256.0f, XMFLOAT2(2, 2), woodA, woodN, woodR, woodM, samplerOptions);

	materials.push_back(cobbleMat2xPBR);
	materials.push_back(floorMatPBR);
	materials.push_back(paintMatPBR);
	materials.push_back(scratchedMatPBR);
	materials.push_back(bronzeMatPBR);
	materials.push_back(roughMatPBR);
	materials.push_back(woodMatPBR);



	// === Create the PBR entities =====================================
	GameEntity* cobSpherePBR = new GameEntity(sphereMesh, cobbleMat2xPBR);
	GameEntity* floorSpherePBR = new GameEntity(sphereMesh, floorMatPBR);
	GameEntity* paintSpherePBR = new GameEntity(sphereMesh, paintMatPBR);
	GameEntity* scratchSpherePBR = new GameEntity(sphereMesh, scratchedMatPBR);
	GameEntity* bronzeSpherePBR = new GameEntity(sphereMesh, bronzeMatPBR);
	GameEntity* roughSpherePBR = new GameEntity(sphereMesh, roughMatPBR);
	GameEntity* woodSpherePBR = new GameEntity(sphereMesh, woodMatPBR);

	entities.push_back(cobSpherePBR);
	entities.push_back(floorSpherePBR);
	entities.push_back(paintSpherePBR);
	entities.push_back(scratchSpherePBR);
	entities.push_back(bronzeSpherePBR);
	entities.push_back(roughSpherePBR);
	entities.push_back(woodSpherePBR);

	// Create the non-PBR entities ==============================
	GameEntity* cobSphere = new GameEntity(sphereMesh, cobbleMat2x);
	GameEntity* floorSphere = new GameEntity(sphereMesh, floorMat);
	GameEntity* paintSphere = new GameEntity(sphereMesh, paintMat);
	GameEntity* scratchSphere = new GameEntity(sphereMesh, scratchedMat);
	GameEntity* bronzeSphere = new GameEntity(sphereMesh, bronzeMat);
	GameEntity* roughSphere = new GameEntity(sphereMesh, roughMat);
	GameEntity* woodSphere = new GameEntity(sphereMesh, woodMat);

	entities.push_back(cobSphere);
	entities.push_back(floorSphere);
	entities.push_back(paintSphere);
	entities.push_back(scratchSphere);
	entities.push_back(bronzeSphere);
	entities.push_back(roughSphere);
	entities.push_back(woodSphere);

	// Set starting positions =====
	// PBR entities
	cobSpherePBR->GetTransform()->SetPosition(-6, 2, 0);
	floorSpherePBR->GetTransform()->SetPosition(1, 0, 0);

	paintSpherePBR->GetTransform()->SetPosition(-1, 0, 0);
	scratchSpherePBR->GetTransform()->SetPosition(0, 2, 0);
	bronzeSpherePBR->GetTransform()->SetPosition(1, 0, 0);

	roughSpherePBR->GetTransform()->SetPosition(-1, 0, 0);
	woodSpherePBR->GetTransform()->SetPosition(6, 2, 0);

	// Non-PBR entities
	cobSphere->GetTransform()->SetPosition(-6, -2, 0);

	// Create Child-Parent relationships
	for (int e = 0; e < entities.size(); e++) {
		switch (e)
		{
		case 0:
			entities[e]->GetTransform()->AddChild(entities[e + 1]->GetTransform());
			break;
		case 3:
			entities[e]->GetTransform()->AddChild(entities[e - 1]->GetTransform());
			entities[e]->GetTransform()->AddChild(entities[e + 1]->GetTransform());
			break;
		case 6:
			entities[e]->GetTransform()->AddChild(entities[e - 1]->GetTransform());
			break;
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
			entities[e]->GetTransform()->SetScale(2, 2, 2);
			entities[e]->GetTransform()->SetPosition(-6 + (e - 7) * 2, -2, 0);

			/*if(e > 7)
				entities[e]->GetTransform()->SetParent(entities[e - 1]->GetTransform());*/
			break;
		default:
			break;
		}

		// Scales any entities that are sole parents entities to 2x
		for (auto ge : entities)
		{
			if(ge->GetTransform()->GetChildCount() > 0 &&
				ge->GetTransform()->GetParent() == NULL)
				ge->GetTransform()->SetScale(2, 2, 2);
		}
	}

	// Save assets needed for drawing point lights
	// (Since these are just copies of the pointers,
	//  we won't need to directly delete them as 
	//  the original pointers will be cleaned up)
	lightMesh = sphereMesh;
	lightVS = vertexShader;
	lightPS = solidColorPS;

	// Create the renderer
	renderer = new Renderer(
		device,
		context,
		swapChain,
		backBufferRTV,
		depthStencilView,
		width,
		height,
		sky,
		entities,
		lights, 
		lightVS,
		lightPS);
}

// --------------------------------------------------------
// Generates the lights in the scene: 3 directional lights
// and many random point lights.
// --------------------------------------------------------
void Game::GenerateLights()
{
	// Reset
	lights.clear();

	// Setup directional lights
	Light dir1 = {};
	dir1.Type = LIGHT_TYPE_DIRECTIONAL;
	dir1.Direction = XMFLOAT3(1, -1, 1);
	dir1.Color = XMFLOAT3(0.8f, 0.8f, 0.8f);
	dir1.Intensity = 1.0f;

	Light dir2 = {};
	dir2.Type = LIGHT_TYPE_DIRECTIONAL;
	dir2.Direction = XMFLOAT3(-1, -0.25f, 0);
	dir2.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir2.Intensity = 1.0f;

	Light dir3 = {};
	dir3.Type = LIGHT_TYPE_DIRECTIONAL;
	dir3.Direction = XMFLOAT3(0, -1, 1);
	dir3.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir3.Intensity = 1.0f;

	// Add light to the list
	lights.push_back(dir1);
	lights.push_back(dir2);
	lights.push_back(dir3);

	// Create the rest of the lights
	while (lights.size() < lightCount)
	{
		Light point = {};
		point.Type = LIGHT_TYPE_POINT;
		point.Position = XMFLOAT3(RandomRange(-10.0f, 10.0f), RandomRange(-5.0f, 5.0f), RandomRange(-10.0f, 10.0f));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = RandomRange(5.0f, 10.0f);
		point.Intensity = RandomRange(0.1f, 3.0f);

		// Add to the list
		lights.push_back(point);
	}

}

// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Update our projection matrix to match the new aspect ratio
	if (camera)
		camera->UpdateProjectionMatrix(this->width / (float)this->height);

	// Update renderer
	renderer->PostResize(width, height, backBufferRTV, depthStencilView);
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	GUISetup(deltaTime);

	// Update the camera
	camera->Update(deltaTime);

	// Check individual input
	Input& input = Input::GetInstance();
	if (input.KeyDown(VK_ESCAPE)) Quit();
	if (input.KeyPress(VK_TAB)) GenerateLights();

	for (int e = 0; e < entities.size(); e++) {
		switch (e)
		{
		// The left and rightmost PBR entities rotate
		// with their child (the adjacent entity)
		case 0:
		case 6:
			entities[e]->GetTransform()->Rotate(0.0f, sinf(deltaTime), 0.0f);
			break;
		// The center PBR entity rotates with its 2 adjacent children entities
		case 3:
			entities[e]->GetTransform()->Rotate(0.0f, 0.0f, sinf(deltaTime));
			break;
		// Each entity on the bottom row moves back and forth down and to the right
		// Moving its child (the entity to the right) along with it
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
			//entities[e]->GetTransform()->MoveRelative(sinf(totalTime) / 50, -sinf(totalTime) / 50, 0.0f);
			break;
		default:
			break;
		}
	}

	CreateGUI();
}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	renderer->Render(camera, lightMesh, arial, spriteBatch);
}

void Game::GUISetup(float deltaTime)
{
	Input& input = Input::GetInstance();
	// Reset input manager's gui state so we don’t
	// taint our own input (you’ll uncomment later)
	input.SetGuiKeyboardCapture(false);
	input.SetGuiMouseCapture(false);
	// 
	// Set io info
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = deltaTime;
	io.DisplaySize.x = (float)this->width;
	io.DisplaySize.y = (float)this->height;
	io.KeyCtrl = input.KeyDown(VK_CONTROL);
	io.KeyShift = input.KeyDown(VK_SHIFT);
	io.KeyAlt = input.KeyDown(VK_MENU);
	io.MousePos.x = (float)input.GetMouseX();
	io.MousePos.y = (float)input.GetMouseY();
	io.MouseDown[0] = input.MouseLeftDown();
	io.MouseDown[1] = input.MouseRightDown();
	io.MouseDown[2] = input.MouseMiddleDown();
	io.MouseWheel=  input.GetMouseWheel();
	input.GetKeyArray(io.KeysDown, 256);
	
	// Reset the frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui:: NewFrame();
	// Determine new input capture (you’ll uncomment later)
	input.SetGuiKeyboardCapture(io.WantCaptureKeyboard);
	input.SetGuiMouseCapture(io.WantCaptureMouse);
	// Show the demo window
	ImGui:: ShowDemoWindow();
}

void Game::CreateGUI() 
{
	ImGuiIO& io = ImGui::GetIO();

	// Stats Window
	ImGui::Begin("Stats");
	ImGui::Text("FPS: %i", (int)io.Framerate);
	ImGui::Text("Window Width: %i", this->width);
	ImGui::Text("Window Height: %i", this->height);
	float aspectRatio = this->width / (float)this->height;
	ImGui::Text("Aspect Ratio: %f", aspectRatio);
	ImGui::Text("Number of Entities: %i", entities.size());
	ImGui::Text("Number of Lights: %i", lightCount);
	ImGui::End();

	// Entities Window
	ImGui::Begin("Elements");
	int geIndex = 0;
	if (ImGui::CollapsingHeader("Entities")) {
		for (auto ge : entities) {
			DisplayEntityInfo(ge, geIndex);
			geIndex++;
		}
	}
	if (ImGui::CollapsingHeader("Lights")) {
		for (int i = 0; i < lightCount; i++) {
			DisplayLightInfo(i);
		}
	}
	ImGui::End();
}

void Game::DisplayEntityInfo(GameEntity* ge, int index)
{
	std::string iStr = std::to_string(index); 
	std::string node = "Entity " + iStr; 
	if (ImGui::TreeNode(node.c_str())) {
		// Game Entity Position, Rotation, and Scale structure
		// 
		// "Header" name (Position, Rotation, or Scale)
		// Get the current values from the ge
		// Create the slider ID that is ge specific
		// Create a float[4] for the slider using the current ge values
		// Create the slider
		// Set the new values of the ge
		//

		// Position
		ImGui::Text("Position: ");
		XMFLOAT3 gePos = ge->GetTransform()->GetPosition();
		std::string gePosSliderName = "##GE" + iStr + "Pos";
		float gePosSliderValues[4] = { gePos.x, gePos.y, gePos.z, 0.44f };
		ImGui::SliderFloat3(gePosSliderName.c_str(), gePosSliderValues, -10.0f, 10.0f);
		ge->GetTransform()->SetPosition(gePosSliderValues[0], gePosSliderValues[1], gePosSliderValues[2]);

		// Rotation
		ImGui::Text("Rotation: ");
		XMFLOAT3 geRot = ge->GetTransform()->GetPitchYawRoll();
		std::string geRotSliderName = "##GE" + iStr + "Rot";
		float geRotSliderValues[4] = { geRot.x, geRot.y, geRot.z, 0.44f };
		ImGui::SliderFloat3(geRotSliderName.c_str(), geRotSliderValues, -6.28, 6.28);
		ge->GetTransform()->SetRotation(geRotSliderValues[0], geRotSliderValues[1], geRotSliderValues[2]);

		// Scale
		ImGui::Text("Scale: ");
		XMFLOAT3 geScale = ge->GetTransform()->GetScale();
		std::string geScaleSliderName = "##GE" + iStr + "Scale";
		float geScaleSliderValues[4] = { geScale.x, geScale.y, geScale.z, 0.44f };
		ImGui::SliderFloat3(geScaleSliderName.c_str(), geScaleSliderValues, 0.1, 5.0f);
		ge->GetTransform()->SetScale(geScaleSliderValues[0], geScaleSliderValues[1], geScaleSliderValues[2]);
		
		// Children
		std::string childCount = std::to_string(ge->GetTransform()->GetChildCount());
		std::string childCountNode = childCount;
		if(ge->GetTransform()->GetChildCount() != 1)
			childCountNode += " children";
		else
			childCountNode += " child";
		ImGui::Text(childCountNode.c_str());
		ImGui::TreePop();
	}
}

void Game::DisplayLightInfo(int index)
{
	std::string iStr = std::to_string(index);
	std::string node = "Light " + iStr;
	if (ImGui::TreeNode(node.c_str())) {
		// Zero'd variables to (possibly) be used later in the switch statement
		XMFLOAT3 lightDir = XMFLOAT3();
		XMFLOAT3 lightPos = XMFLOAT3();
		XMFLOAT3 newDir = XMFLOAT3();
		XMFLOAT3 newPos = XMFLOAT3();
		float range = 0.0f;

		// Slider names & base values for position and direction
		std::string lightDirSliderName = "##Light" + iStr + "Dir";
		std::string lightPosSliderName = "##Light" + iStr + "Pos";
		float lightDirSliderValues[4] = { 0.0f, 0.0f, 0.0f, 0.44f };
		float lightPosSliderValues[4] = { 0.0f, 0.0f, 0.0f, 0.44f };
		// Slider name & bounds for range
		std::string lightRangeSliderName = "##Light" + iStr + "Range";
		float f_five = 5.0f;
		float f_ten = 10.0f;

		// Light Color
		XMFLOAT3 color = lights[index].Color;
		std::string lightColorSliderName = "##Light" + iStr + "Color";
		ImGui::ColorEdit4(lightColorSliderName.c_str(), &color.x);
		lights[index].Color = color;

		// Light Type
		int lightType = lights[index].Type;

		switch (lightType)
		{
		// Light Position or Direction or both structure
		// 
		// "Header" name (Position or Direction)
		// Get the current values from the light
		// Set new values for the float[4] for the slider using the light's (pos/dir) values
		// Create the slider
		// Set the values of the light
		//
		case 0:
			ImGui::Text("Type: Directional");

			// Direction
			ImGui::Text("Direction: ");
			lightDir = lights[index].Direction;
			lightDirSliderValues[0] = lightDir.x;
			lightDirSliderValues[1] = lightDir.y;
			lightDirSliderValues[2] = lightDir.z;
			ImGui::SliderFloat3(lightDirSliderName.c_str(), lightDirSliderValues, -1.0f, 1.0f);
			newDir = XMFLOAT3(lightDirSliderValues[0], lightDirSliderValues[1], lightDirSliderValues[2]);
			lights[index].Direction = newDir;

			break;
		case 1:
			ImGui::Text("Type: Point");

			// Position
			ImGui::Text("Position: ");
			lightPos = lights[index].Position;
			lightPosSliderValues[0] = lightPos.x;
			lightPosSliderValues[1] = lightPos.y;
			lightPosSliderValues[2] = lightPos.z;
			ImGui::SliderFloat3(lightPosSliderName.c_str(), lightPosSliderValues, -10.0f, 10.0f);
			newPos = XMFLOAT3(lightPosSliderValues[0], lightPosSliderValues[1], lightPosSliderValues[2]);
			lights[index].Position = newPos;

			// Range
			ImGui::Text("Range: ");
			range = lights[index].Range;

			// Slider creation
			ImGui::SliderScalar(lightRangeSliderName.c_str(), ImGuiDataType_Float, &range, &f_five, &f_ten);
			lights[index].Range = range;

			break;
		case 2:
			ImGui::Text("Type: Spot");

			// Direction
			ImGui::Text("Direction: ");
			lightDir = lights[index].Direction;
			lightDirSliderValues[0] = lightDir.x;
			lightDirSliderValues[1] = lightDir.y;
			lightDirSliderValues[2] = lightDir.z;
			ImGui::SliderFloat3(lightDirSliderName.c_str(), lightDirSliderValues, -1.0f, 1.0f);
			newDir = XMFLOAT3(lightDirSliderValues[0], lightDirSliderValues[1], lightDirSliderValues[2]);
			lights[index].Direction = newDir;

			// Position
			ImGui::Text("Position: ");
			lightPos = lights[index].Position;
			lightPosSliderValues[0] = lightPos.x;
			lightPosSliderValues[1] = lightPos.y;
			lightPosSliderValues[2] = lightPos.z;
			ImGui::SliderFloat3(lightPosSliderName.c_str(), lightPosSliderValues, -10.0f, 10.0f);
			newPos = XMFLOAT3(lightPosSliderValues[0], lightPosSliderValues[1], lightPosSliderValues[2]);
			lights[index].Position = newPos;
			break;
		}
		
		// Intensity
		ImGui::Text("Intensity: ");
		float intensity = lights[index].Intensity;

		// Slider Name & Bounds
		std::string lightIntensitySliderName = "##Light" + iStr + "Intensity";
		float f_tenth = 0.1f;
		float f_three = 3.0f;

		// Slider creation
		ImGui::SliderScalar(lightIntensitySliderName.c_str(), ImGuiDataType_Float, &intensity, &f_tenth, &f_three);
		lights[index].Intensity = intensity;

		ImGui::TreePop();
	}
}