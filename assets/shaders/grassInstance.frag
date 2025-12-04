#version 460 core
out vec4 FragColor;

in vec2 uv;
in vec3 normal;
in vec3 worldPosition;
in vec2 worldXZ;

uniform sampler2D sampler;	//diffuse贴图采样器
uniform sampler2D opacityMask;//透明蒙版
uniform sampler2D cloudMask;
uniform float cloudSpeed;
uniform float time;
uniform vec3 windDirection;
uniform float cloudLerp;

uniform vec3 ambientColor;

//相机世界位置
uniform vec3 cameraPosition;


uniform float shiness;

//透明度
uniform float opacity;

//草地贴图特性
uniform float uvScale;
uniform float brightness;

uniform vec3 cloudWhiteColor;
uniform vec3 cloudBlackColor;
uniform float cloudUVScale;

struct DirectionalLight{
	vec3 direction;
	vec3 color;
	float specularIntensity;
};

struct PointLight{
	vec3 position;
	vec3 color;
	float specularIntensity;

	float k2;
	float k1;
	float kc;
};

struct SpotLight{
	vec3 position;
	vec3 targetDirection;
	vec3 color;
	float outerLine;
	float innerLine;
	float specularIntensity;
};

uniform DirectionalLight directionalLight;

//计算漫反射光照
vec3 calculateDiffuse(vec3 lightColor, vec3 objectColor, vec3 lightDir, vec3 normal){
	float diffuse = clamp(dot(-lightDir, normal), 0.0,1.0);
	vec3 diffuseColor = lightColor * diffuse * objectColor;

	return diffuseColor;
}

//计算镜面反射光照
vec3 calculateSpecular(vec3 lightColor, vec3 lightDir, vec3 normal, vec3 viewDir, float intensity){
	//1 防止背面光效果
	float dotResult = dot(-lightDir, normal);
	float flag = step(0.0, dotResult);
	vec3 lightReflect = normalize(reflect(lightDir,normal));

	//2 jisuan specular
	float specular = max(dot(lightReflect,-viewDir), 0.0);

	//3 控制光斑大小
	specular = pow(specular, shiness);

	//4 计算最终颜色
	vec3 specularColor = lightColor * specular * flag * intensity;

	return specularColor;
}

vec3 calculateSpotLight(SpotLight light, vec3 normal, vec3 viewDir){
	//计算光照的通用数据
	vec3 objectColor  = texture(sampler, uv).xyz;
	vec3 lightDir = normalize(worldPosition - light.position);
	vec3 targetDir = normalize(light.targetDirection);

	//计算spotlight的照射范围
	float cGamma = dot(lightDir, targetDir);
	float intensity =clamp( (cGamma - light.outerLine) / (light.innerLine - light.outerLine), 0.0, 1.0);

	//1 计算diffuse
	vec3 diffuseColor = calculateDiffuse(light.color,objectColor, lightDir,normal);

	//2 计算specular
	vec3 specularColor = calculateSpecular(light.color, lightDir,normal, viewDir,light.specularIntensity); 

	return (diffuseColor + specularColor)*intensity;
}

vec3 calculateDirectionalLight(vec3 objectColor,  DirectionalLight light, vec3 normal ,vec3 viewDir){
	//计算光照的通用数据
	vec3 lightDir = normalize(light.direction);

	//1 计算diffuse
	vec3 diffuseColor = calculateDiffuse(light.color,objectColor, lightDir,normal);

	//2 计算specular
	vec3 specularColor = calculateSpecular(light.color, lightDir,normal, viewDir,light.specularIntensity); 

	return diffuseColor + specularColor;
}

vec3 calculatePointLight(vec3 objectColor, PointLight light, vec3 normal ,vec3 viewDir){
	//计算光照的通用数据
	vec3 lightDir = normalize(worldPosition - light.position);

	//计算衰减
	float dist = length(worldPosition - light.position);
	float attenuation = 1.0 / (light.k2 * dist * dist + light.k1 * dist + light.kc);

	//1 计算diffuse
	vec3 diffuseColor = calculateDiffuse(light.color,objectColor, lightDir,normal);

	//2 计算specular
	vec3 specularColor = calculateSpecular(light.color, lightDir,normal, viewDir,light.specularIntensity); 

	return (diffuseColor + specularColor)*attenuation;
}



// ==========================================
// 复制自 cloud.frag 的噪声函数，用于计算阴影
// ==========================================
float hash(float n) { return fract(sin(n) * 753.5453123); }

float noise(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    float n = p.x + p.y * 157.0 + 113.0 * p.z;
    return mix(mix(mix(hash(n + 0.0), hash(n + 1.0), f.x),
                   mix(hash(n + 157.0), hash(n + 158.0), f.x), f.y),
               mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
                   mix(hash(n + 270.0), hash(n + 271.0), f.x), f.y), f.z);
}

float fbm(vec3 p) {
    float f = 0.0;
    f += 0.5000 * noise(p); p = p * 2.02;
    f += 0.2500 * noise(p); p = p * 2.03;
    f += 0.1250 * noise(p); p = p * 2.01;
    f += 0.0625 * noise(p);
    return f;
}

// 对应 cloud.frag 中的 map 函数，但我们只需要算密度
// 这里为了性能，我们可以简化一点，只采样核心密度
float getCloudDensity(vec3 p, float time) {
    vec3 q = p - vec3(0.0, 0.0, time * 2.0);
    float f = fbm(q * 0.1); 
    
    // 这里硬编码一些与 cloud.frag 保持一致的参数
    // 如果你在 main.cpp 里改了参数，这里也要对应调整
    // 假设云层中心在 Y=25，范围是 Y=15~35
    float heightMask = smoothstep(15.0, 20.0, p.y) * smoothstep(35.0, 30.0, p.y);
    
    // 边缘淡出 (与之前设置的圆形遮罩保持一致)
    float dist = length(p.xz);
    float circularMask = smoothstep(45.0, 20.0, dist);

    float cloudDensityThreshold = 0.4; // 对应 CloudMaterial 的默认值
    
    return max(0.0, f - cloudDensityThreshold) * heightMask * circularMask;
}



void main()
{
	vec2 worldXZ = worldXZ;//将世界坐标的位置作为采样uv
	vec2 worldUV = worldXZ / uvScale;
	vec3 objectColor  = texture(sampler, worldUV).xyz * brightness;
	vec3 result = vec3(0.0,0.0,0.0);

	//计算光照的通用数据
	vec3 normalN = normalize(normal);
	vec3 viewDir = normalize(worldPosition - cameraPosition);

	result += calculateDirectionalLight(objectColor, directionalLight,normalN, viewDir);

	//环境光计算
	float alpha =  texture(opacityMask, uv).r;//采用透明蒙版的透明度

	vec3 ambientColor = objectColor * ambientColor;

	vec3 grassColor = result + ambientColor;

	vec3 windDirN = normalize(windDirection);
	vec2 cloudUV = worldXZ/ cloudUVScale;
	cloudUV = cloudUV + time * cloudSpeed * windDirN.xz;//云的uv方向要与风的方向保持一致
	float cloudMask = texture(cloudMask, cloudUV).r;
	vec3 cloudColor = mix(cloudBlackColor, cloudWhiteColor, cloudMask);


	// 1. 获取光线方向 (指向太阳)
    vec3 lightDir = normalize(-directionalLight.direction);
    
    // 2. 射线求交：计算从当前草地像素位置 (vWorldPos) 沿着光线方向，到达云层高度 (Y=25) 的位置
    // 公式: P_cloud = P_grass + t * L
    // 我们要找 P_cloud.y = 25.0
    float cloudHeight = 25.0; 
    
    // 如果太阳在地平线以下或平行，就不计算阴影
    float shadowFactor = 1.0; // 1.0 表示无阴影 (全亮)
    
    if (lightDir.y > 0.01) {
        float t = (cloudHeight - worldPosition.y) / lightDir.y;
        vec3 cloudPos = worldPosition + t * lightDir;
        
        // 3. 在云层位置采样密度
        // 这里的 time 需要从 uniform 传进来，或者暂时用 osg_FrameTime 之类的
        float density = getCloudDensity(cloudPos, time); // 确保你有 uniform float time;
        
        // 4. 根据密度计算阴影强度 (Beer's Law 简化版)
        // 密度越大，shadowFactor 越小 (越黑)
        // 乘一个系数 (例如 3.0) 来控制阴影的浓淡
        shadowFactor = exp(-density * 3.0);
    }
    
    // 5. 混合云的阴影颜色
    // 当 shadowFactor 为 1 时，显示原色；为 0 时，显示阴影色 (cloudBlackColor)
    // 这一步通常是混合 Diffuse 颜色，或者直接乘在最终光照上
    // 这里我们用你的 mix 逻辑：
    
    // 注意：原来的 mix(black, white, mask) 逻辑里，mask=1 是白(无云/亮)，mask=0 是黑(有云)
    // 现在的 shadowFactor: 1.0 是亮，0.0 是黑
    vec3 cloudShadowColor = mix(cloudBlackColor, cloudWhiteColor, shadowFactor);


	vec3 finalColor = mix(grassColor, cloudShadowColor, cloudLerp);
	

	FragColor = vec4(finalColor,alpha * opacity);
}