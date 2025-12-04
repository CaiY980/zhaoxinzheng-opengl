#version 460 core
out vec4 FragColor;

in vec3 vWorldPos;

uniform vec3 cameraPosition;
uniform vec3 lightDirection;
uniform vec3 ambientColor;
uniform vec3 lightColor;
uniform float time;
uniform vec2 winSize; // 屏幕分辨率，用于计算深度

// 云的参数
uniform float cloudDensityThreshold; // 0.1
uniform float cloudAbsorption;       // 0.5

// 简单的哈希函数用于生成噪声
float hash(float n) { return fract(sin(n) * 753.5453123); }

// 3D 值噪声
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

// 分形布朗运动 (FBM) - 叠加多层噪声以获得云的絮状感
float fbm(vec3 p) {
    float f = 0.0;
    f += 0.5000 * noise(p); p = p * 2.02;
    f += 0.2500 * noise(p); p = p * 2.03;
    f += 0.1250 * noise(p); p = p * 2.01;
    f += 0.0625 * noise(p);
    return f;
}

// 场景距离场 (这里定义云的形状)
float map(vec3 p) {
    // 让云随时间移动
    vec3 q = p - vec3(0.0, 0.0, time * 2.0);
    
    // 基础噪声
    float f = fbm(q * 0.1); // 0.1 是缩放系数，越小云越大
    
    float dist = length(p.xz);
    float circularMask = smoothstep(45.0, 20.0, dist);
    // 可以在这里限制云的高度范围 (例如只在 y=10 到 y=20 之间)
    float heightMask = smoothstep(10.0, 20.0, p.y) * smoothstep(30.0, 20.0, p.y);
    

    // 4. 应用阈值和所有遮罩
    // 注意：这里乘上了 boundaryMask
    float density = max(0.0, f - cloudDensityThreshold) * heightMask * circularMask;
    
    return density;
}

void main()
{
    vec3 ro = cameraPosition;
    vec3 rd = normalize(vWorldPos - cameraPosition);

    // 步进参数
    float stepSize = 0.5; // 步长，越小越精细但越卡
    int maxSteps = 64;    // 最大步数
    float maxDist = 100.0; // 最远渲染距离

    float t = 0.0;
    
    // 如果相机在包围盒外，应该先计算射线与包围盒的交点作为起点 t
    // 这里为了简化，假设相机就在云层下方或内部，直接从相机处开始步进
    // 或者从一定距离开始
    t = max(0.0, (10.0 - ro.y) / rd.y); // 简单的优化：直接跳到云层底部 y=10

    vec4 sum = vec4(0.0); // 累积颜色和透明度

    for(int i = 0; i < maxSteps; i++) {
        vec3 p = ro + t * rd;
        
        // 如果超出范围或不透明度已满，停止
        if(sum.a > 0.99 || t > maxDist || p.y > 35.0) break;

        float density = map(p);

        if(density > 0.001) {
            // 简单的光照计算 (向着光源方向采样一次密度作为阴影)
            float lightDiff = 0.0;
            // 这里的 stepSize * 2.0 是为了向光源方向偏移一点
            // 真正的体积光影需要再做一个内部循环，这里用简化的
            float lightDensity = map(p + lightDirection * 1.0);
            float shadow = exp(-lightDensity * 5.0); 
            
            //vec3 col = lightColor * shadow * 1.2; // 1.2是亮度增强
            // 1. 主光照 (太阳光 * 阴影)
            vec3 diffuse = lightColor * shadow * 1.5;
            
            // 2. 环境光 (在阴影处补光，让它不那么黑)
            // 这里的 0.6 是环境光强度系数，你可以调整
            vec3 ambient = ambientColor * 0.8; 
            
            // 3. 最终颜色是两者的叠加
            vec3 col = diffuse + ambient;

           
           

            // 累积
            float alpha = 1.0 - exp(-density * stepSize * cloudAbsorption);
            sum.rgb += col * alpha * (1.0 - sum.a);
            sum.a += alpha;
        }

        t += stepSize;
    }

    // 简单的雾化融合（可选）
    //sum.rgb = mix(sum.rgb, vec3(0.6, 0.7, 0.8), 1.0 - exp(-0.001 * t * t));

    if(sum.a < 0.01) discard;

    FragColor = sum;
}