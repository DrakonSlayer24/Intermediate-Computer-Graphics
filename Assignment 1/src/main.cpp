//Just a simple handler for simple initialization stuffs
#include "Utilities/BackendHandler.h"
#include "Utilities/Util.h"

#include <filesystem>
#include <json.hpp>
#include <fstream>

#include <Texture2D.h>
#include <Texture2DData.h>
#include <MeshBuilder.h>
#include <MeshFactory.h>
#include <NotObjLoader.h>
#include <ObjLoader.h>
#include <VertexTypes.h>
#include <ShaderMaterial.h>
#include <RendererComponent.h>
#include <TextureCubeMap.h>
#include <TextureCubeMapData.h>

#include <Timing.h>
#include <GameObjectTag.h>
#include <InputHelpers.h>

#include <IBehaviour.h>
#include <CameraControlBehaviour.h>
#include <FollowPathBehaviour.h>
#include <SimpleMoveBehaviour.h>

//Variables
GLfloat PosTimer;
GLfloat PosMaxTime = 1.5f;
GLfloat t = 0.0f;
GLfloat CatT = 0.0f;
GLfloat CatTimer;
GLfloat CatMaxTime = 10.0f;
int CatmullLoop = 0;
int CatmullX;
glm::vec3 p0(20, 0, 0.3), p1(0, 20, 0.3), p2(-20, 0, 0.3), p3(0, -20, 0.3);
GLfloat Count = 0;
GLfloat Turn = -90;

template<typename T>
T Catmull(const T& p0, const T& p1, const T& p2, const T& p3, float t)
{
	return 0.5f * (2.f * p1 + t * (-p0 + p2)
		+ t * t * (2.f * p0 - 5.f * p1 + 4.f * p2 - p3)
		+ t * t * t * (-p0 + 3.f * p1 - 3.f * p2 + p3));
}

void RenderVAO(
	const Shader::sptr& shader,
	const VertexArrayObject::sptr& vao,
	const glm::mat4& viewProjection,
	const Transform& transform)
{
	shader->SetUniformMatrix("u_ModelViewProjection", viewProjection * transform.LocalTransform());
	shader->SetUniformMatrix("u_Model", transform.LocalTransform());
	shader->SetUniformMatrix("u_NormalMatrix", transform.NormalMatrix());
	vao->Render();
}

int main() {
	int frameIx = 0;
	float fpsBuffer[128];
	float minFps, maxFps, avgFps;
	int selectedVao = 0; // select cube by default
	std::vector<GameObject> controllables;

	BackendHandler::InitAll();

	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(BackendHandler::GlDebugMessage, nullptr);

	// Enable texturing
	glEnable(GL_TEXTURE_2D);

	// Push another scope so most memory should be freed *before* we exit the app
	{
		#pragma region Shader and ImGui

		// Load our shaders
		Shader::sptr shader = Shader::Create();
		shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		shader->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured.glsl", GL_FRAGMENT_SHADER);
		shader->Link();

		glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 5.0f);
		glm::vec3 lightCol = glm::vec3(0.9f, 0.85f, 0.5f);
		bool DiffuseToggle = false;
		bool AmbientToggle = false;
		bool SpecularToggle = false;
		bool Option1 = false;
		bool Option2 = false;
		bool Option3 = false;
		bool Option4 = false;
		bool Option5 = false;
		float     lightAmbientPow = 0.1f;
		float     lightSpecularPow = 1.0f;
		glm::vec3 ambientCol = glm::vec3(1.0f);
		float     ambientPow = 0.5f;
		float     lightLinearFalloff = 0.09f;
		float     lightQuadraticFalloff = 0.032f;
		float CustomEffect = 0.0f;
		GLfloat Timer = 0;
		
		// These are our application / scene level uniforms that don't necessarily update
		// every frame
		shader->SetUniform("u_LightPos", lightPos);
		shader->SetUniform("u_LightCol", lightCol);
		shader->SetUniform("u_DiffuseToggle", (int)DiffuseToggle);
		shader->SetUniform("u_AmbientToggle", (int)AmbientToggle);
		shader->SetUniform("u_SpecularToggle", (int)SpecularToggle);
		shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
		shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
		shader->SetUniform("u_AmbientCol", ambientCol);
		shader->SetUniform("u_AmbientStrength", ambientPow);
		shader->SetUniform("u_LightAttenuationConstant", 1.0f);
		shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
		shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
		shader->SetUniform("u_Option1", (int)Option1);
		shader->SetUniform("u_Option2", (int)Option2);
		shader->SetUniform("u_Option3", (int)Option3);
		shader->SetUniform("u_Option4", (int)Option4);
		shader->SetUniform("u_Option5", (int)Option5);
		shader->SetUniform("u_CustomEffect", CustomEffect);
		
		// We'll add some ImGui controls to control our shader
		BackendHandler::imGuiCallbacks.push_back([&]() {
			if (ImGui::CollapsingHeader("Lighting Toggle Buttons"))
			{
				if (ImGui::Checkbox("Diffuse", &DiffuseToggle))
				{
					Option1 = false;
					Option2 = false;
					Option3 = false;
					Option4 = false;
					Option5 = false;
					shader->SetUniform("u_DiffuseToggle", (int)DiffuseToggle);
				}
				if (ImGui::Checkbox("Ambient", &AmbientToggle))
				{
					Option1 = false;
					Option2 = false;
					Option3 = false;
					Option4 = false;
					Option5 = false;
					shader->SetUniform("u_AmbientToggle", (int)AmbientToggle);
				}
				if (ImGui::Checkbox("Specular", &SpecularToggle))
				{
					Option1 = false;
					Option2 = false;
					Option3 = false;
					Option4 = false;
					Option5 = false;
					shader->SetUniform("u_SpecularToggle", (int)SpecularToggle);
				}
			}

			if (ImGui::CollapsingHeader("Debug Toggle Buttons"))
			{
				if (ImGui::Checkbox("No Lighting", &Option1))
				{
					Option2 = false;
					Option3 = false;
					Option4 = false;
					Option5 = false;
					DiffuseToggle = false;
					AmbientToggle = false;
					SpecularToggle = false;
					shader->SetUniform("u_Option1", (int)Option1);
				}
				if (ImGui::Checkbox("Ambient Only", &Option2))
				{
					Option1 = false;
					Option3 = false;
					Option4 = false;
					Option5 = false;
					DiffuseToggle = false;
					AmbientToggle = false;
					SpecularToggle = false;
					shader->SetUniform("u_Option2", (int)Option2);
				}
				if (ImGui::Checkbox("Specular Only", &Option3))
				{
					Option1 = false;
					Option2 = false;
					Option4 = false;
					Option5 = false;
					DiffuseToggle = false;
					AmbientToggle = false;
					SpecularToggle = false;
					shader->SetUniform("u_Option3", (int)Option3);
				}
				if (ImGui::Checkbox("Ambient + Specular", &Option4))
				{
					Option1 = false;
					Option2 = false;
					Option3 = false;
					Option5 = false;
					DiffuseToggle = false;
					AmbientToggle = false;
					SpecularToggle = false;
					shader->SetUniform("u_Option4", (int)Option4);
				}
				if (ImGui::Checkbox("Ambient + Specular + Custom", &Option5))
				{
					Option1 = false;
					Option2 = false;
					Option3 = false;
					Option4 = false;
					DiffuseToggle = false;
					AmbientToggle = false;
					SpecularToggle = false;
					shader->SetUniform("u_Option5", (int)Option5);
					shader->SetUniform("u_CustomEffect", CustomEffect);
				}
			}
			/*
			if (ImGui::CollapsingHeader("Scene Level Lighting Settings"))
			{
				if (ImGui::ColorPicker3("Ambient Color", glm::value_ptr(ambientCol))) {
					shader->SetUniform("u_AmbientCol", ambientCol);
				}
				if (ImGui::SliderFloat("Fixed Ambient Power", &ambientPow, 0.01f, 1.0f)) {
					shader->SetUniform("u_AmbientStrength", ambientPow);
				}
			}
			if (ImGui::CollapsingHeader("Light Level Lighting Settings"))
			{
				if (ImGui::DragFloat3("Light Pos", glm::value_ptr(lightPos), 0.01f, -10.0f, 10.0f)) {
					shader->SetUniform("u_LightPos", lightPos);
				}
				if (ImGui::ColorPicker3("Light Col", glm::value_ptr(lightCol))) {
					shader->SetUniform("u_LightCol", lightCol);
				}
				if (ImGui::SliderFloat("Light Ambient Power", &lightAmbientPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				}
				if (ImGui::SliderFloat("Light Specular Power", &lightSpecularPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				if (ImGui::DragFloat("Light Linear Falloff", &lightLinearFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
				}
				if (ImGui::DragFloat("Light Quadratic Falloff", &lightQuadraticFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
				}
			}

			auto name = controllables[selectedVao].get<GameObjectTag>().Name;
			ImGui::Text(name.c_str());
			auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
			ImGui::Checkbox("Relative Rotation", &behaviour->Relative);

			ImGui::Text("Q/E -> Yaw\nLeft/Right -> Roll\nUp/Down -> Pitch\nY -> Toggle Mode");

			minFps = FLT_MAX;
			maxFps = 0;
			avgFps = 0;
			for (int ix = 0; ix < 128; ix++) {
				if (fpsBuffer[ix] < minFps) { minFps = fpsBuffer[ix]; }
				if (fpsBuffer[ix] > maxFps) { maxFps = fpsBuffer[ix]; }
				avgFps += fpsBuffer[ix];
			}
			ImGui::PlotLines("FPS", fpsBuffer, 128);
			ImGui::Text("MIN: %f MAX: %f AVG: %f", minFps, maxFps, avgFps / 128.0f);
			*/
			});
			
		//std::cout << DiffuseToggle;
		#pragma endregion 

		// GL states
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LEQUAL); // New 

		#pragma region TEXTURE LOADING

		// Load some textures from files
		Texture2D::sptr stone = Texture2D::LoadFromFile("images/Stone_001_Diffuse.png");
		Texture2D::sptr stoneSpec = Texture2D::LoadFromFile("images/Stone_001_Specular.png");
		Texture2D::sptr grass = Texture2D::LoadFromFile("images/StoneBrick.jpg");
		Texture2D::sptr noSpec = Texture2D::LoadFromFile("images/grassSpec.png");
		Texture2D::sptr box = Texture2D::LoadFromFile("images/box.bmp");
		Texture2D::sptr boxSpec = Texture2D::LoadFromFile("images/box-reflections.bmp");
		Texture2D::sptr simpleFlora = Texture2D::LoadFromFile("images/SimpleFlora.png");
		Texture2D::sptr black = Texture2D::LoadFromFile("images/black.png");
		Texture2D::sptr stoneTex = Texture2D::LoadFromFile("images/stone.jpg");

		// Load the cube map
		//TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/sample.jpg");
		//TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/ToonSky.jpg"); 
		//TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/ocean.jpg");
		TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/cave.jpg");
		
		// Creating an empty texture
		Texture2DDescription desc = Texture2DDescription();  
		desc.Width = 1;
		desc.Height = 1;
		desc.Format = InternalFormat::RGB8;
		Texture2D::sptr texture2 = Texture2D::Create(desc);
		// Clear it with a white colour
		texture2->Clear();

		#pragma endregion

		///////////////////////////////////// Scene Generation //////////////////////////////////////////////////
		#pragma region Scene Generation
		
		// We need to tell our scene system what extra component types we want to support
		GameScene::RegisterComponentType<RendererComponent>();
		GameScene::RegisterComponentType<BehaviourBinding>();
		GameScene::RegisterComponentType<Camera>();

		// Create a scene, and set it to be the active scene in the application
		GameScene::sptr scene = GameScene::Create("test");
		Application::Instance().ActiveScene = scene;

		// We can create a group ahead of time to make iterating on the group faster
		entt::basic_group<entt::entity, entt::exclude_t<>, entt::get_t<Transform>, RendererComponent> renderGroup =
			scene->Registry().group<RendererComponent>(entt::get_t<Transform>());

		// Create a material and set some properties for it
		ShaderMaterial::sptr stoneMat = ShaderMaterial::Create();  
		stoneMat->Shader = shader;
		stoneMat->Set("s_Diffuse", stone);
		stoneMat->Set("s_Specular", stoneSpec);
		stoneMat->Set("u_Shininess", 2.0f);
		stoneMat->Set("u_TextureMix", 0.0f); 

		ShaderMaterial::sptr stone2Mat = ShaderMaterial::Create();
		stone2Mat->Shader = shader;
		stone2Mat->Set("s_Diffuse", stoneTex);
		stone2Mat->Set("s_Specular", stoneSpec);
		stone2Mat->Set("u_Shininess", 2.0f);
		stone2Mat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr grassMat = ShaderMaterial::Create();
		grassMat->Shader = shader;
		grassMat->Set("s_Diffuse", grass);
		grassMat->Set("s_Specular", noSpec);
		grassMat->Set("u_Shininess", 2.0f);
		grassMat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr boxMat = ShaderMaterial::Create();
		boxMat->Shader = shader;
		boxMat->Set("s_Diffuse", box);
		boxMat->Set("s_Specular", boxSpec);
		boxMat->Set("u_Shininess", 8.0f);
		boxMat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr simpleFloraMat = ShaderMaterial::Create();
		simpleFloraMat->Shader = shader;
		simpleFloraMat->Set("s_Diffuse", simpleFlora);
		simpleFloraMat->Set("s_Specular", noSpec);
		simpleFloraMat->Set("u_Shininess", 8.0f);
		simpleFloraMat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr blackTex = ShaderMaterial::Create();
		blackTex->Shader = shader;
		blackTex->Set("s_Diffuse", black);
		blackTex->Set("s_Specular", noSpec);
		blackTex->Set("u_Shininess", 2.0f);
		blackTex->Set("u_TextureMix", 0.0f);

		//Ground vao
		GameObject ground = scene->CreateEntity("Ground"); 
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/plane.obj");
			ground.emplace<RendererComponent>().SetMesh(vao).SetMaterial(grassMat);
			ground.get<Transform>().SetLocalScale(2.0f, 2.0f, 2.0f);
		}

		//Bug vao
		GameObject bug = scene->CreateEntity("bug");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/bug.obj");
			bug.emplace<RendererComponent>().SetMesh(vao).SetMaterial(blackTex);
			bug.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.2f);
			bug.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(bug);
		}

		//Rocks vao
		VertexArrayObject::sptr vao1 = ObjLoader::LoadFromFile("models/rock.obj");
		GameObject rock = scene->CreateEntity("rock");
		{
			rock.emplace<RendererComponent>().SetMesh(vao1).SetMaterial(stone2Mat);
			//rock.get<Transform>().SetLocalScale(2.0f, 2.0f, 0.2f);
			rock.get<Transform>().SetLocalPosition(5.0f, -8.0f, -0.3f).SetLocalRotation(90.0f, 0.0f, 0.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(rock);
		}

		// Create an object to be our camera
		GameObject cameraObject = scene->CreateEntity("Camera");
		{
			cameraObject.get<Transform>().SetLocalPosition(0, 3, 3).LookAt(glm::vec3(0, 0, 0));

			// We'll make our camera a component of the camera object
			Camera& camera = cameraObject.emplace<Camera>();// Camera::Create();
			camera.SetPosition(glm::vec3(0, 3, 3));
			camera.SetUp(glm::vec3(0, 0, 1));
			camera.LookAt(glm::vec3(0));
			camera.SetFovDegrees(90.0f); // Set an initial FOV
			camera.SetOrthoHeight(3.0f);
			BehaviourBinding::Bind<CameraControlBehaviour>(cameraObject);
		}

		Framebuffer* testBuffer;
		GameObject framebufferObject = scene->CreateEntity("Basic Buffer");
		{
			int width, height;
			glfwGetWindowSize(BackendHandler::window, &width, &height);

			testBuffer = &framebufferObject.emplace<Framebuffer>();
			testBuffer->AddDepthTarget();
			testBuffer->AddColorTarget(GL_RGBA8);
			testBuffer->Init(width, height);
		}
		#pragma endregion 
		//////////////////////////////////////////////////////////////////////////////////////////

		/////////////////////////////////// SKYBOX ///////////////////////////////////////////////
		{
			// Load our shaders
			Shader::sptr skybox = std::make_shared<Shader>();
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.vert.glsl", GL_VERTEX_SHADER);
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.frag.glsl", GL_FRAGMENT_SHADER);
			skybox->Link();

			ShaderMaterial::sptr skyboxMat = ShaderMaterial::Create();
			skyboxMat->Shader = skybox;  
			skyboxMat->Set("s_Environment", environmentMap);
			skyboxMat->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0))));
			skyboxMat->RenderLayer = 100;

			MeshBuilder<VertexPosNormTexCol> mesh;
			MeshFactory::AddIcoSphere(mesh, glm::vec3(0.0f), 1.0f);
			MeshFactory::InvertFaces(mesh);
			VertexArrayObject::sptr meshVao = mesh.Bake();
			
			GameObject skyboxObj = scene->CreateEntity("skybox");  
			skyboxObj.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			skyboxObj.get_or_emplace<RendererComponent>().SetMesh(meshVao).SetMaterial(skyboxMat);
		}
		////////////////////////////////////////////////////////////////////////////////////////


		// We'll use a vector to store all our key press events for now (this should probably be a behaviour eventually)
		std::vector<KeyPressWatcher> keyToggles;
		{
			// This is an example of a key press handling helper. Look at InputHelpers.h an .cpp to see
			// how this is implemented. Note that the ampersand here is capturing the variables within
			// the scope. If you wanted to do some method on the class, your best bet would be to give it a method and
			// use std::bind
			keyToggles.emplace_back(GLFW_KEY_T, [&]() { cameraObject.get<Camera>().ToggleOrtho(); });

			controllables.push_back(bug);

			keyToggles.emplace_back(GLFW_KEY_KP_ADD, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao++;
				if (selectedVao >= controllables.size())
					selectedVao = 0;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});
			keyToggles.emplace_back(GLFW_KEY_KP_SUBTRACT, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao--;
				if (selectedVao < 0)
					selectedVao = controllables.size() - 1;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});

			keyToggles.emplace_back(GLFW_KEY_Y, [&]() {
				auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
				behaviour->Relative = !behaviour->Relative;
				});
		}

		// Initialize our timing instance and grab a reference for our use
		Timing& time = Timing::Instance();
		time.LastFrame = glfwGetTime();

		///// Game loop /////
		while (!glfwWindowShouldClose(BackendHandler::window)) {
			glfwPollEvents();

			// Update the timing
			time.CurrentFrame = glfwGetTime();
			time.DeltaTime = static_cast<float>(time.CurrentFrame - time.LastFrame);

			Timer = Timer + 0.01;
			CustomEffect = sin(Timer);
			shader->SetUniform("u_CustomEffect", CustomEffect);

			if (Timer >= 180)
			{
				Timer = 0;
			}

			time.DeltaTime = time.DeltaTime > 1.0f ? 1.0f : time.DeltaTime;
			CatTimer += time.DeltaTime;

			if (CatTimer >= CatMaxTime)
			{
				CatTimer = 0.0f;
				CatmullLoop = CatmullLoop + 1;
				if (CatmullLoop >= 4)
					CatmullLoop = 0;
			}

			t = PosTimer / PosMaxTime;
			CatT = CatTimer / CatMaxTime;

			t = length(p1 - p0);

			// Update our FPS tracker data
			fpsBuffer[frameIx] = 1.0f / time.DeltaTime;
			frameIx++;
			if (frameIx >= 128)
				frameIx = 0;

			// We'll make sure our UI isn't focused before we start handling input for our game
			if (!ImGui::IsAnyWindowFocused()) {
				// We need to poll our key watchers so they can do their logic with the GLFW state
				// Note that since we want to make sure we don't copy our key handlers, we need a const
				// reference!
				for (const KeyPressWatcher& watcher : keyToggles) {
					watcher.Poll(BackendHandler::window);
				}
			}

			// Iterate over all the behaviour binding components
			scene->Registry().view<BehaviourBinding>().each([&](entt::entity entity, BehaviourBinding& binding) {
				// Iterate over all the behaviour scripts attached to the entity, and update them in sequence (if enabled)
				for (const auto& behaviour : binding.Behaviours) {
					if (behaviour->Enabled) {
						behaviour->Update(entt::handle(scene->Registry(), entity));
					}
				}
			});

			// Clear the screen
			testBuffer->Clear();

			glClearColor(0.08f, 0.17f, 0.31f, 1.0f);
			glEnable(GL_DEPTH_TEST);
			glClearDepth(1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Update all world matrices for this frame
			scene->Registry().view<Transform>().each([](entt::entity entity, Transform& t) {
				t.UpdateWorldMatrix();
			});

			//Bug Catmull Circle
			if (CatmullLoop == 0)
			{
				bug.get<Transform>().SetLocalPosition(Catmull(p0, p1, p2, p3, CatT));
			}
			else if (CatmullLoop == 1)
			{
				bug.get<Transform>().SetLocalPosition(Catmull(p1, p2, p3, p0, CatT));
			}
			else if (CatmullLoop == 2)
			{
				bug.get<Transform>().SetLocalPosition(Catmull(p2, p3, p0, p1, CatT));
			}
			else if (CatmullLoop == 3)
			{
				bug.get<Transform>().SetLocalPosition(Catmull(p3, p0, p1, p2, CatT));
			}

			bug.get<Transform>().SetLocalRotation(90.0f, 0.0f, Turn);
			Turn = Turn + 0.15;
			
			// Grab out camera info from the camera object
			Transform& camTransform = cameraObject.get<Transform>();
			glm::mat4 view = glm::inverse(camTransform.LocalTransform());
			glm::mat4 projection = cameraObject.get<Camera>().GetProjection();
			glm::mat4 viewProjection = projection * view;
						
			// Sort the renderers by shader and material, we will go for a minimizing context switches approach here,
			// but you could for instance sort front to back to optimize for fill rate if you have intensive fragment shaders
			renderGroup.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
				// Sort by render layer first, higher numbers get drawn last
				if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
				if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

				// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
				if (l.Material->Shader < r.Material->Shader) return true;
				if (l.Material->Shader > r.Material->Shader) return false;

				// Sort by material pointer last (so we can minimize switching between materials)
				if (l.Material < r.Material) return true;
				if (l.Material > r.Material) return false;
				
				return false;
			});

			// Start by assuming no shader or material is applied
			Shader::sptr current = nullptr;
			ShaderMaterial::sptr currentMat = nullptr;

			testBuffer->Bind();

			// Iterate over the render group components and draw them
			renderGroup.each( [&](entt::entity e, RendererComponent& renderer, Transform& transform) {
				// If the shader has changed, set up it's uniforms
				if (current != renderer.Material->Shader) {
					current = renderer.Material->Shader;
					current->Bind();
					BackendHandler::SetupShaderForFrame(current, view, projection);
				}
				// If the material has changed, apply it
				if (currentMat != renderer.Material) {
					currentMat = renderer.Material;
					currentMat->Apply();
				}
				// Render the mesh
				if (renderer.Mesh == vao1)
				{
					
					rock.get<Transform>().SetLocalPosition(18.0f, 14.0f, -0.5f).SetLocalRotation(90.0f, 0.0f, 0.0f); 
					RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
					rock.get<Transform>().SetLocalPosition(-6.0f, 12.0f, -0.2f).SetLocalRotation(90.0f, 0.0f, 0.0f);
					RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
					rock.get<Transform>().SetLocalPosition(8.0f, -22.0f, -0.1f).SetLocalRotation(90.0f, 0.0f, 0.0f); 
					RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
					rock.get<Transform>().SetLocalPosition(-2.0f, 28.0f, -0.0f).SetLocalRotation(90.0f, 0.0f, 0.0f); 
					RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
					rock.get<Transform>().SetLocalPosition(-21.0f, -18.0f, -0.3f).SetLocalRotation(90.0f, 0.0f, 0.0f);
					RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
					rock.get<Transform>().SetLocalPosition(23.0f, 6.0f, -0.2f).SetLocalRotation(90.0f, 0.0f, 0.0f);
					RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
				}
				else
				{
					RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
				}
			});

			testBuffer->Unbind();

			testBuffer->DrawToBackbuffer();

			// Draw our ImGui content
			BackendHandler::RenderImGui();

			scene->Poll();
			glfwSwapBuffers(BackendHandler::window);
			time.LastFrame = time.CurrentFrame;
		}

		// Nullify scene so that we can release references
		Application::Instance().ActiveScene = nullptr;
		BackendHandler::ShutdownImGui();
	}	

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}