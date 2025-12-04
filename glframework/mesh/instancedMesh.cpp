#include "instancedMesh.h"

InstancedMesh::InstancedMesh(
	Geometry* geometry, 
	Material* material, 
	unsigned int instanceCount):Mesh(geometry, material) {  //调用父类的有参构造函数
	mType = ObjectType::InstancedMesh;
	mInstanceCount = instanceCount;
	//mInstanceMatrices = new glm::mat4[instanceCount];//这个时候这是个空数组，先占位，后面会单独灌数据
	mInstanceMatrices.resize(instanceCount);//预留空间

	glGenBuffers(1, &mMatrixVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mMatrixVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * mInstanceCount, mInstanceMatrices.data(), GL_DYNAMIC_DRAW); //支持动态改变
	glBindVertexArray(mGeometry->getVao());
	glBindBuffer(GL_ARRAY_BUFFER, mMatrixVbo);
	for (int i = 0; i < 4; i++)
	{
		glEnableVertexAttribArray(4 + i); // 开启 Shader 中的 location 4, 5, 6, 7  分别对应矩阵的第 1, 2, 3, 4 列
		glVertexAttribPointer(
			4 + i,              // index: 属性位置
			4,                  // size: 每个属性包含 4 个 float (即 vec4)
			GL_FLOAT,           // type
			GL_FALSE,           // normalized
			sizeof(glm::mat4),  // stride: 步长。读取下一个实例的对应列需要跳过整个矩阵的大小 (64字节)
			(void*)(sizeof(float) * i * 4) // pointer: 偏移量。当前列在矩阵内部的起始偏移
		);
		/*
		默认情况下（Divisor = 0），OpenGL 每处理一个顶点，属性指针就前进一步。
		设置 Divisor = 1 后，OpenGL 变成每处理一个实例（Instance），属性指针才前进一步。
		*/
		glVertexAttribDivisor(4 + i, 1);//逐实例更新
	}
	glBindVertexArray(0);
}


InstancedMesh::InstancedMesh(
	Geometry* geometry,
	Material* material,
	unsigned int instanceCount, glm::mat4 T0, glm::mat4 T1, glm::mat4 T2) :Mesh(geometry, material) {  //调用父类的有参构造函数
	mType = ObjectType::InstancedMesh;
	mInstanceCount = instanceCount;
	//mInstanceMatrices = new glm::mat4[instanceCount];//这个时候这是个空数组，先占位，后面会单独灌数据
	mInstanceMatrices.resize(instanceCount);
	mInstanceMatrices[0] = T0;
	mInstanceMatrices[1] = T1;
	mInstanceMatrices[2] = T2;

	glGenBuffers(1, &mMatrixVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mMatrixVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * mInstanceCount, mInstanceMatrices.data(), GL_DYNAMIC_DRAW); //支持动态改变
	glBindVertexArray(mGeometry->getVao());
	glBindBuffer(GL_ARRAY_BUFFER, mMatrixVbo);
	for (int i = 0; i < 4; i++)
	{
		glEnableVertexAttribArray(3 + i);
		glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(float) * i * 4));
		glVertexAttribDivisor(3 + i, 1);//逐实例更新
	}
	glBindVertexArray(0);
}



// 将 CPU 端修改过的矩阵数组 (mInstanceMatrices) 同步更新到 GPU 显存 (VBO) 中
void InstancedMesh::updateMatrices() {
	glBindBuffer(GL_ARRAY_BUFFER, mMatrixVbo);
	//如果使用glBufferData进行数据更新，会导致重新分配显存空间
	//glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * mInstanceCount, mInstanceMatrices, GL_DYNAMIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::mat4) * mInstanceCount, mInstanceMatrices.data());//这个才是专门用来更新VBO数据的函数
	glBindVertexArray(0);
}

// 对所有的实例矩阵进行排序，确保渲染顺序是 从远到近 (Back-to-Front)
void InstancedMesh::sortMatrices(glm::mat4 viewMatrix) {
	std::sort(
		mInstanceMatrices.begin(),
		mInstanceMatrices.end(),
		[viewMatrix](const glm::mat4& a, const glm::mat4& b) {
			//1 计算A的相机系的Z
			auto modelMatrixA = a;
			auto worldPositionA = modelMatrixA * glm::vec4(0.0, 0.0, 0.0, 1.0);//所有的面片最开始都是在原点的
			//乘上他的模型变换矩阵就是世界空间的坐标
			auto cameraPositionA = viewMatrix * worldPositionA;

			//2 计算B的相机系的Z
			auto modelMatrixB = b;
			auto worldPositionB = modelMatrixB * glm::vec4(0.0, 0.0, 0.0, 1.0);//所有的面片最开始都是在原点的
			auto cameraPositionB = viewMatrix * worldPositionB;

			return cameraPositionA.z < cameraPositionB.z;
		});
}

InstancedMesh::~InstancedMesh() {

}	