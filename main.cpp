#include <iostream>

#include "glframework/core.h"
#include "glframework/shader.h"
#include <string>
#include <assert.h>//断言
#include "wrapper/checkError.h"
#include "application/Application.h"
#include "glframework/texture.h"

//引入相机+控制器
#include "application/camera/perspectiveCamera.h"
#include "application/camera/orthographicCamera.h"
#include "application/camera/trackBallCameraControl.h"
#include "application/camera/GameCameraControl.h"

#include "glframework/geometry.h"
#include "glframework/material/phongMaterial.h"
#include "glframework/material/whiteMaterial.h"
#include "glframework/material/depthMaterial.h"
#include "glframework/material/opacityMaskMaterial.h"
#include "glframework/material/screenMaterial.h"
#include "glframework/material/cubeMaterial.h"
#include "glframework/material/phongEnvMaterial.h"
#include "glframework/material/phongInstanceMaterial.h"
#include "glframework/material/grassInstanceMaterial.h"
#include "glframework/mesh/mesh.h"
#include "glframework/mesh/instancedMesh.h"
#include "glframework/renderer/renderer.h"
#include "glframework/light/pointLight.h"
#include "glframework/light/spotLight.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "glframework/scene.h"
#include "application/assimpLoader.h"
#include "application/assimpInstanceLoader.h"
#include "glframework/material/cloudMaterial.h"
#include "glframework/framebuffer/framebuffer.h"

Mesh* sunMesh = nullptr;
Renderer* renderer = nullptr;
Scene* sceneOffscreen = nullptr;//离屏渲染场景
Scene* sceneInscreen = nullptr;//贴屏渲染场景
Scene* scene = nullptr; //通用测试场景
CloudMaterial* cloudMaterial = nullptr;
Framebuffer* framebuffer = nullptr;

//这里先写死,后面需要单独进行管理
int WIDTH = 1600;
int HEIGHT = 1200;

GrassInstanceMaterial* grassMaterial = nullptr;//这里后面要通过imgui去调整,所以放到全局

// 灯光
std::vector<PointLight*> pointLights{};
SpotLight* spotLight = nullptr;
DirectionalLight* dirLight = nullptr;

//环境光
AmbientLight* ambLight = nullptr;

Camera* camera = nullptr;
CameraControl* cameraControl = nullptr;

glm::vec3 clearColor{};

//天气控制变量 (0.0 = 白天, 0.5 = 傍晚, 1.0 = 黑夜)
float timeOfDay = 0.0f;

// 辅助函数：线性插值 vec3
glm::vec3 lerpVec3(glm::vec3 a, glm::vec3 b, float t) {
	// t = clamp(t, 0.0f, 1.0f); // GLM会自动处理插值范围
	return a * (1.0f - t) + b * t;
}

// 辅助函数：线性插值 float
float lerpFloat(float a, float b, float t) {
	// t = clamp(t, 0.0f, 1.0f); // GLM会自动处理插值范围
	return a * (1.0f - t) + b * t;
}

// 更新环境光照、背景和材质参数
void updateEnvironment() {

	// -------------------------------------------------------------
	//  计算太阳位置与旋转
	// -------------------------------------------------------------
	// 定义太阳的轨道半径 (比地面稍大，确保在远处)
	float orbitRadius = 120.0f;

	// 将 timeOfDay (0.0~1.0) 映射为角度 (弧度制)
	// 0.0 (白天) = 90度 (正上方)
	// 0.5 (傍晚) = 0度 (地平线)
	// 1.0 (黑夜) = -90度 (正下方)
	float angleRad = glm::mix(glm::radians(90.0f), glm::radians(-90.0f), timeOfDay);

	// 计算太阳在 XY 平面上的位置 (模拟日落西山)
	// 假设 -X 方向是西方 (日落方向)
	float sunX = -cos(angleRad) * orbitRadius;
	float sunY = sin(angleRad) * orbitRadius;
	float sunZ = -30.0f; //稍微偏一点Z轴，不让它直直地切过头顶

	glm::vec3 sunPosition = glm::vec3(sunX, sunY, sunZ);

	// 更新太阳模型的位置
	if (sunMesh) {
		sunMesh->setPosition(sunPosition);
	}


	// 定义三个阶段的关键参数
	// -------------------------------------------------------------
	// 白天参数 (0.0)
	glm::vec3 dayClearColor = glm::vec3(0.5f, 0.8f, 1.0f);   // 蓝天
	glm::vec3 dayLightColor = glm::vec3(1.0f, 1.0f, 0.95f);  // 暖白光
	float dayLightIntensity = 0.7f;                          // 柔和主光强度
	float dayAmbIntensity = 0.35f;                           // 柔和环境光强度
	// 白天镜面强度：设为极低，消除镜面高光带来的“光移动”错觉
	float daySpecularIntensity = 0.03f;
	glm::vec3 daySunDir = glm::vec3(-0.5f, -1.0f, -0.5f);
	float dayGrassBrightness = 1.0f;
	float dayCloudLerp = 0.3f;

	// 傍晚参数 (0.5)
	glm::vec3 eveClearColor = glm::vec3(0.8f, 0.5f, 0.3f);
	glm::vec3 eveLightColor = glm::vec3(1.0f, 0.6f, 0.3f);
	float eveLightIntensity = 0.8f;
	float eveAmbIntensity = 0.4f;
	// 傍晚镜面强度：设为较低，保持柔和
	float eveSpecularIntensity = 0.05f;
	glm::vec3 eveSunDir = glm::vec3(-1.0f, -0.2f, -0.5f);
	float eveGrassBrightness = 0.7f;
	float eveCloudLerp = 0.6f;

	// 黑夜参数 (1.0)
	glm::vec3 nightClearColor = glm::vec3(0.02f, 0.02f, 0.1f);
	glm::vec3 nightLightColor = glm::vec3(0.2f, 0.3f, 0.6f);
	float nightLightIntensity = 0.1f;
	float nightAmbIntensity = 0.0f;
	// 黑夜镜面强度：几乎为零
	float nightSpecularIntensity = 0.0f;
	glm::vec3 nightSunDir = glm::vec3(0.5f, -1.0f, 0.5f);
	float nightGrassBrightness = 0.01f;
	float nightCloudLerp = 0.0f;

	// 2. 根据 timeOfDay 进行插值计算
	// -------------------------------------------------------------
	glm::vec3 currentClearColor, currentLightColor, currentSunDir;
	float currentLightIntensity, currentAmbIntensity, currentGrassBrightness, currentCloudLerp, currentSpecularIntensity;

	if (timeOfDay <= 0.5f) {
		// [阶段1] 白天 -> 傍晚 (0.0 ~ 0.5)
		float t = timeOfDay / 0.5f;
		currentClearColor = lerpVec3(dayClearColor, eveClearColor, t);
		currentLightColor = lerpVec3(dayLightColor, eveLightColor, t);
		currentSunDir = glm::normalize(lerpVec3(daySunDir, eveSunDir, t));
		currentLightIntensity = lerpFloat(dayLightIntensity, eveLightIntensity, t);
		currentAmbIntensity = lerpFloat(dayAmbIntensity, eveAmbIntensity, t);
		currentGrassBrightness = lerpFloat(dayGrassBrightness, eveGrassBrightness, t);
		currentCloudLerp = lerpFloat(dayCloudLerp, eveCloudLerp, t);
		// 镜面强度
		currentSpecularIntensity = lerpFloat(daySpecularIntensity, eveSpecularIntensity, t);
	}
	else {
		// [阶段2] 傍晚 -> 黑夜 (0.5 ~ 1.0)
		float t = (timeOfDay - 0.5f) / 0.5f;
		currentClearColor = lerpVec3(eveClearColor, nightClearColor, t);
		currentLightColor = lerpVec3(eveLightColor, nightLightColor, t);
		currentSunDir = glm::normalize(lerpVec3(eveSunDir, nightSunDir, t));
		currentLightIntensity = lerpFloat(eveLightIntensity, nightLightIntensity, t);
		currentAmbIntensity = lerpFloat(eveAmbIntensity, nightAmbIntensity, t);
		currentGrassBrightness = lerpFloat(eveGrassBrightness, nightGrassBrightness, t);
		currentCloudLerp = lerpFloat(eveCloudLerp, nightCloudLerp, t);
		//  镜面强度
		currentSpecularIntensity = lerpFloat(eveSpecularIntensity, nightSpecularIntensity, t);
	}

	// 3. 应用参数
	// -------------------------------------------------------------
	clearColor = currentClearColor;

	// 设置主灯光 (太阳/月亮)
	if (dirLight) {
		dirLight->mColor = currentLightColor;
		dirLight->mIntensity = currentLightIntensity;
		//dirLight->mDirection = currentSunDir;
		//  设置镜面强度
		//dirLight->mSpecularIntensity = currentSpecularIntensity;
		dirLight->mDirection = glm::normalize(-sunPosition);

		dirLight->mSpecularIntensity = currentSpecularIntensity;
	}

	// 设置环境光
	if (ambLight) {
		//ambLight->mColor = glm::vec3(currentAmbIntensity);
		glm::vec3 skyTint = glm::mix(glm::vec3(1.0f), currentClearColor, 0.5f);
		ambLight->mColor = skyTint * currentAmbIntensity;
	}

	// 设置草地材质参数
	if (grassMaterial) {
		grassMaterial->mBrightness = currentGrassBrightness;
		grassMaterial->mCloudLerp = currentCloudLerp;
	}
}
#pragma region 事件回调函数
void OnResize(int width, int height) {
	GL_CALL(glViewport(0, 0, width, height));
	std::cout << "OnResize" << std::endl;
}

void OnKey(int key, int action, int mods) {
	cameraControl->onKey(key, action, mods);
}

//鼠标按下/抬起
void OnMouse(int button, int action, int mods) {
	double x, y;
	glApp->getCursorPosition(&x, &y);
	cameraControl->onMouse(button, action, x, y);
}

//鼠标移动
void OnCursor(double xpos, double ypos) {
	cameraControl->onCursor(xpos, ypos);
}

//鼠标滚轮
void OnScroll(double offset) {
	cameraControl->onScroll(offset);
}


void prepareCamera() {
	float size = 10.0f;
	//camera = new OrthographicCamera(-size, size, size, -size, size, -size);
	camera = new PerspectiveCamera(
		60.0f,
		(float)glApp->getWidth() / (float)glApp->getHeight(),
		0.1f,
		200.0f // 增加远裁剪距离，以包含太阳的 120.0f 轨道
	);

	cameraControl = new GameCameraControl();
	cameraControl->setCamera(camera);
	cameraControl->setSensitivity(0.4f);
}

void setModelBlend(Object* obj, bool blend, float opacity) {
	if (obj->getType() == ObjectType::Mesh)
	{
		Mesh* mesh = (Mesh*)obj;
		Material* mat = mesh->mMaterial;
		mat->mBlend = blend;
		mat->mOpacity = opacity;
		mat->mDepthWrite = false;
	}
	auto children = obj->getChildren();
	for (int i = 0; i < children.size(); i++)
	{
		setModelBlend(children[i], blend, opacity);
	}
}

void setInstanceMatrix(Object* obj, int index, glm::mat4 matrix) {
	if (obj->getType() == ObjectType::InstancedMesh)
	{
		InstancedMesh* im = (InstancedMesh*)obj;
		im->mInstanceMatrices[index] = matrix;
	}
	auto children = obj->getChildren();
	for (int i = 0; i < children.size(); i++)
	{
		setInstanceMatrix(children[i], index, matrix);
	}
}

void updateInstanceMatrix(Object* obj) {
	if (obj->getType() == ObjectType::InstancedMesh)
	{
		InstancedMesh* im = (InstancedMesh*)obj;
		im->updateMatrices();
	}
	auto children = obj->getChildren();
	for (int i = 0; i < children.size(); i++)
	{
		updateInstanceMatrix(children[i]);
	}
} 

void setInstanceMaterial(Object* obj, Material* material) {
	if (obj->getType() == ObjectType::InstancedMesh)
	{
		InstancedMesh* im = (InstancedMesh*)obj;
		im->mMaterial = material;
	}
	auto children = obj->getChildren();
	for (int i = 0; i < children.size(); i++)
	{
		setInstanceMaterial(children[i], material);
	}
}

void prepare() {
	renderer = new Renderer();
	scene = new Scene();




	srand(glfwGetTime());

	/*auto boxGeo = Geometry::createBox(1.0f);
	auto boxMat = new CubeMaterial();
	boxMat->mDiffuse = new Texture("assets/textures/box2.png", 0);
	auto boxMesh = new Mesh(boxGeo, boxMat);
	scene->addChild(boxMesh);*/

	// --- 体积云初始化 ---
	// 创建一个巨大的盒子覆盖天空，例如 100x20x100
	auto cloudGeo = Geometry::createBox(1.0f); // 基础是1x1x1
	auto cloudMesh = new Mesh(cloudGeo, new CloudMaterial());

	// 缩放成扁平的大盒子
	cloudMesh->setScale(glm::vec3(100.0f, 20.0f, 100.0f));
	// 移动到半空中，例如 Y=25
	cloudMesh->setPosition(glm::vec3(0.0f, 25.0f, 0.0f));

	// 获取并保存材质指针以便 imgui 调节
	cloudMaterial = (CloudMaterial*)cloudMesh->mMaterial;
	cloudMaterial->mBlend = true; // 云是半透明的
	// 这一点很重要：因为我们在盒子内部看，需要渲染背面或者关闭剔除
	// 简单粗暴的方法是关闭剔除，或者在 Renderer::setFaceCullingState 里处理
	cloudMaterial->mFaceCulling = false;

	scene->addChild(cloudMesh);


	float groundSize = 20.0f; // 设定一个足够大的地面尺寸，覆盖草地视野
	auto groundGeo = Geometry::createPlane(groundSize, groundSize, 5.0f, 5.0f);

	// 使用 PhongMaterial，假设地面颜色为深绿色/棕色
	auto groundMat = new PhongMaterial();

	groundMat->mDiffuse = new Texture("assets/textures/grass.jpg", 0); // 暂用草地底纹

	auto groundMesh = new Mesh(groundGeo, groundMat);

	// Geometry::createPlane 默认在 XY 平面，法线朝 Z+。
	// 需要将其绕 X 轴旋转 -90 度，使其位于 XZ 平面（地平面），法线朝 Y+。
	//groundMesh->setRotation(glm::vec3(-90.0f, 0.0f, 0.0f));
	groundMesh->setAngleX(-90.0f);
	// 将地面略微下移，避免与草地的根部发生 Z-Fighting
	groundMesh->setPosition(glm::vec3(0.0f, -0.01f, 0.0f));

	// 土地不需要高光（泥土不反光）
	groundMat->mShiness = 0.5f;
	// 地面是双面的，可以禁用背面剔除（如果需要从地下看）
	groundMat->mFaceCulling = false;

	scene->addChild(groundMesh);
	
	
	int rNum = 50;
	int cNum = 50;
	float spacing = 0.4f; // 间距 0.4m

	auto grassModel = AssimpInstanceLoader::load("assets/fbx/grassNew.obj", rNum * cNum);
	glm::mat4 translate;
	glm::mat4 rotate;
	glm::mat4 scaleMat;
	glm::mat4 transform;

	srand(glfwGetTime());//给定随机种子

	for (int r = 0; r < rNum; r++)
	{
		for (int c = 0; c < cNum; c++)
		{
			// 中心化位置 [-range, range]
			float baseX = (r - rNum / 2.0f) * spacing;
			float baseZ = (c - cNum / 2.0f) * spacing;

			// 随机偏移 (Jitter)
			float jitterX = (rand() % 100 / 100.0f - 0.5f) * spacing;
			float jitterZ = (rand() % 100 / 100.0f - 0.5f) * spacing;

			float x = baseX + jitterX;
			float z = baseZ + jitterZ;
			float y = 0.0f; // 确保草地根部在 Y=0

			// 构建变换矩阵
			translate = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));

			// 随机旋转
			rotate = glm::rotate(glm::radians((float)(rand() % 360)), glm::vec3(0.0, 1.0, 0.0));

			// 随机缩放 (0.8 ~ 1.2倍)
			float s = 0.8f + (rand() % 50 / 100.0f);
			scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(s, s, s));

			transform = translate * rotate * scaleMat;
			setInstanceMatrix(grassModel, r * cNum + c, transform);
		}
	}
	updateInstanceMatrix(grassModel); // 必须调用，上传数据

	grassMaterial = new GrassInstanceMaterial();

	grassMaterial->mDiffuse = new Texture("assets/textures/GRASS.png", 0);
	grassMaterial->mOpacityMask = new Texture("assets/textures/grassMask.png", 1);
	grassMaterial->mCloudMask = new Texture("assets/textures/CLOUD.PNG", 2);
	grassMaterial->mBlend = true;
	grassMaterial->mDepthWrite = false;
	setInstanceMaterial(grassModel, grassMaterial);
	scene->addChild(grassModel);


	//auto house = AssimpLoader::load("assets/fbx/house.fbx");
	//house->setScale(glm::vec3(0.5f));
	//house->setPosition(glm::vec3(rNum * 0.2f / 2.0f, 0.4f, cNum * 0.2f / 2.0f));
	//scene->addChild(house);

	float sunRadius = 4.0f; // 太阳的大小
	auto sunGeo = Geometry::createSphere(sunRadius);

	// 使用 WhiteMaterial，让太阳始终显示为纯白色（不受光照影响，自发光效果）
	auto sunMat = new WhiteMaterial();

	sunMesh = new Mesh(sunGeo, sunMat);
	scene->addChild(sunMesh);










	//方向光
	dirLight = new DirectionalLight();
	dirLight->mDirection = glm::vec3(-1.0f);
	dirLight->mSpecularIntensity = 0.1f;
	
	//环境光
	ambLight = new AmbientLight();
	ambLight->mColor = glm::vec3(0.1f);
}

void initIMGUI() {
	ImGui::CreateContext(); //创建imgui上下文
	ImGui::StyleColorsDark(); //选择一个主题

	// 设置ImGui与GLFW和OpenGL的绑定
	ImGui_ImplGlfw_InitForOpenGL(glApp->getWindow(), true);
	ImGui_ImplOpenGL3_Init("#version 460");
}

void renderIMGUI() {
	//1 开启当前的IMGUI渲染
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	//2 决定当前的GUI上面有哪些控件，从上到下
	ImGui::Begin("GrassMaterialEditor");
	ImGui::Text("GrassColor");
	ImGui::SliderFloat("UVScale", &grassMaterial->mUVScale, 0.0f, 100.0f);
	ImGui::InputFloat("Brightness", &grassMaterial->mBrightness);
	ImGui::Text("Wind");
	ImGui::InputFloat("WindScale", &grassMaterial->mWindScale);
	ImGui::InputFloat("PhaseScale", &grassMaterial->mPhaseScale);
	ImGui::ColorEdit3("WindDirection", (float*)&grassMaterial->mWindDirection);
	ImGui::Text("Cloud");
	ImGui::ColorEdit3("CloudWhiteColor", (float*)&grassMaterial->mCloudWhiteColor);
	ImGui::ColorEdit3("CloudBlackColor", (float*)&grassMaterial->mCloudBlackColor);
	ImGui::SliderFloat("CloudUVScale", &grassMaterial->mCloudUVScale, 0.0f, 100.0f);
	ImGui::InputFloat("CloudSpeed", &grassMaterial->mCloudSpeed);
	ImGui::SliderFloat("CloudLerp", &grassMaterial->mCloudLerp, 0.0f, 1.0f);
	ImGui::Text("Light");
	ImGui::InputFloat("Intensity", &dirLight->mIntensity);

	ImGui::Text("Time Cycle");
	// 0.0是白天，0.5是傍晚，1.0是黑夜
	if (ImGui::SliderFloat("Time of Day", &timeOfDay, 0.0f, 1.0f, "%.2f")) {
		// 如果滑块拖动了，这帧就会更新，或者放到主循环每帧都更新也可以
	}
	// 显示当前状态文字，增加交互体验
	if (timeOfDay < 0.25f) ImGui::Text("Status: Day");
	else if (timeOfDay < 0.75f) ImGui::Text("Status: Evening/Sunset");
	else ImGui::Text("Status: Night");
	ImGui::Text("Volumetric Cloud");
	if (cloudMaterial) {
		ImGui::SliderFloat("Cloud Threshold", &cloudMaterial->mDensityThreshold, 0.0f, 1.0f);
		ImGui::SliderFloat("Cloud Absorption", &cloudMaterial->mAbsorption, 0.0f, 2.0f);
	}
	ImGui::End();
	
	//3 执行UI渲染
	ImGui::Render();
	//获取当前窗体的宽高
	int display_w, display_h;
	glfwGetFramebufferSize(glApp->getWindow(), &display_w, &display_h);
	//重置视口大小
	glViewport(0, 0, display_w, display_h);

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}




int main() {
	if (!glApp->init(WIDTH, HEIGHT)) {
		return -1;
	}

	glApp->setResizeCallback(OnResize);
	glApp->setKeyBoardCallback(OnKey);
	glApp->setMouseCallback(OnMouse);
	glApp->setCursorCallback(OnCursor);
	glApp->setScrollCallback(OnScroll);

	//设置opengl视口以及清理颜色
	GL_CALL(glViewport(0, 0, WIDTH, HEIGHT));
	//GL_CALL(glClearColor(0.2f, 0.3f, 0.3f, 1.0f));
	GL_CALL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));

	prepareCamera();
	//注意顺序问题!!!! 否则colorattachment为nullptr
	//prepareFBO();
	prepare();
	initIMGUI();

	while (glApp->update()) {
		cameraControl->update(); 
		
		renderer->setClearColor(clearColor);
		//pass01 将box渲染到colorAttachmengt上,也就是新的fbo上
		//renderer->render(sceneOffscreen, camera, dirLight, ambLight, framebuffer->mFBO);//这里采用我们自己的fbo
		
		//pass02 将colorAttachment作为纹理,绘制到整个屏幕上
		renderer->render(scene, camera, dirLight, ambLight);//这里是默认的fbo
		
		updateEnvironment();
		
		


		renderIMGUI();
	}

	glApp->destroy();

	return 0;
}