#include <fstream>
#include "ResourceManager.h"

ResourceManager::ResourceManager() : mFbxExtractor{ nullptr }, mVertexBuffer{}
{
	mFbxExtractor = make_unique<FbxExtractor>();
}

ResourceManager::~ResourceManager()
{
}

void ResourceManager::LoadFbx(const string& fileName, bool onlyAnimation, bool zUp)
{
	mFbxExtractor->ImportFbxFile(fileName, onlyAnimation, zUp);
	mFbxExtractor->ExtractDataFromFbx();

	if (onlyAnimation == false) {
		vector<Vertex>& vertexData = mFbxExtractor->GetVertices();
		SubMeshData subData{};
		subData.vertexCountPerInstance = vertexData.size();
		subData.startVertexLocation = mVertexBuffer.size();
		subData.startIndexLocation = -1;
		mSubMeshData[fileName] = subData;
		mVertexBuffer.insert(mVertexBuffer.end(), vertexData.begin(), vertexData.end());
	}

	SkinnedData animData;
	animData.Set(mFbxExtractor->GetBoneHierarchyIndex(), mFbxExtractor->GetOffsetMatrix(), mFbxExtractor->GetAnimation());
	mAnimData.emplace(fileName, animData);

	mFbxExtractor->ResetAndClear();
}

void ResourceManager::CreatePlane(const string& name, float size, float wrap)
{
	vector<Vertex> vertexData;

	// 평면
	{
		vertexData.push_back(Vertex{ { 0.0f, 0.0f, size},{0.0f,1.0f,0.0f},{0.0f,0.0f} });
		vertexData.push_back(Vertex{ {size, 0.0f, size},{0.0f,1.0f,0.0f},{wrap,0.0f} });
		vertexData.push_back(Vertex{ {size, 0.0f, 0.0f},{0.0f,1.0f,0.0f},{wrap,wrap} });
		vertexData.push_back(Vertex{ {0.0f, 0.0f, size},{0.0f,1.0f,0.0f},{0.0f,0.0f} });
		vertexData.push_back(Vertex{ {size, 0.0f, 0.0f},{0.0f,1.0f,0.0f},{wrap,wrap} });
		vertexData.push_back(Vertex{ {0.0f, 0.0f, 0.0f},{0.0f,1.0f,0.0f},{0.0f,wrap} });
	}

	SubMeshData subData{};
	subData.vertexCountPerInstance = vertexData.size();
	subData.startVertexLocation = mVertexBuffer.size();
	subData.startIndexLocation = -1;
	mSubMeshData[name] = subData;

	mVertexBuffer.insert(mVertexBuffer.end(), vertexData.begin(), vertexData.end());
}

void ResourceManager::CreateTerrain(const string& name, int maxHeight , int scale, int maxUV)
{
	ifstream in{ name };
	if (!in) throw;

	in.seekg(0, ios::end);
	int fileSize = in.tellg();
	in.seekg(0, ios::beg);

	vector<uint8_t> heightMap(fileSize);
	in.read(reinterpret_cast<char*>(heightMap.data()), fileSize);


	int width = sqrt(fileSize);
	int height = sqrt(fileSize);

	mTerrainData.terrainWidth = width;
	mTerrainData.terrainHeight = height;
	mTerrainData.terrainScale = scale;

	float down{ 0.4f };
	vector<float> heightData(width * height);
	for (int z = 0; z < height; ++z) {
		for (int x = 0; x < width; ++x) {
			heightData[z * width + x] = (heightMap[(height - 1 - z) * width + x] / 255.f - down) * maxHeight; // (height - 1 - z)는 왼쪽 아래를 원점(원래 원점은 왼쪽 위)으로 하기 위함이다.
			//heightData[z * width + x] = heightMap[z * width + x] / 255.f * maxHeight; // (height - 1 - z) 의 의미는 왼쪽 아래를 원점으로 하기 위함이다.

		}
	}

	vector<Vertex> vertices(width * height);
	for (int z = 0; z < height; ++z) {
		for (int x = 0; x < width; ++x) {

			int scaledX = x * scale;
			int scaledZ = z * scale;
			// position
			vertices[z * width + x].position.x = scaledX;
			vertices[z * width + x].position.y = heightData[z * width + x];
			vertices[z * width + x].position.z = scaledZ;
			// position.
			
			// normal
			XMVECTOR left = XMVectorZero();
			if (x == 0)
				left = { (float)scaledX, heightData[z * width + x], (float)scaledZ };
			else
				left = { (float)scaledX - scale, heightData[z * width + x - 1], (float)scaledZ };

			XMVECTOR right = XMVectorZero();
			if (x == width - 1)
				right = { (float)scaledX, heightData[z * width + x],(float)scaledZ };
			else
				right = { (float)scaledX + scale, heightData[z * width + x + 1], (float)scaledZ };

			XMVECTOR up = XMVectorZero();
			if (z == height - 1)
				up = { (float)scaledX, heightData[z * width + x], (float)scaledZ };
			else
				up = { (float)scaledX, heightData[(z + 1) * width + x], (float)scaledZ + scale };

			XMVECTOR down = XMVectorZero();
			if (z == 0)
				down = { (float)scaledX, heightData[z * width + x], (float)scaledZ };
			else
				down = { (float)scaledX, heightData[(z - 1) * width + x], (float)scaledZ - scale };

			XMVECTOR normal = XMVector3Normalize(XMVector3Cross(up - down, right - left));			
			XMStoreFloat3(&vertices[z * width + x].normal, normal);
			// normal.
			
			// uv
			vertices[z * width + x].uv.x = ((float)x / (width - 1)) * maxUV;
			vertices[z * width + x].uv.y = (1 - (float)z / (height - 1)) * maxUV;
			// uv.
		}
	}

	vector<uint32_t> indices((width - 1) * (height - 1) * 2 * 3);
	int i{ 0 };
	for (int z = 0; z < height - 1; ++z) {
		for (int x = 0; x < width - 1; ++x) {
			int topLeft = (z + 1) * width + x;
			int topRight = topLeft + 1;
			int bottomLeft = z * width + x;
			int bottomRight = bottomLeft + 1;

			indices[i++] = bottomLeft;
			indices[i++] = topLeft;
			indices[i++] = topRight;

			indices[i++] = bottomLeft;
			indices[i++] = topRight;
			indices[i++] = bottomRight;
		}
	}

	SubMeshData subData{};
	subData.vertexCountPerInstance = vertices.size();
	subData.indexCountPerInstance = indices.size();
	subData.startVertexLocation = mVertexBuffer.size();
	subData.startIndexLocation = mIndexBuffer.size();
	subData.baseVertexLocation = mVertexBuffer.size();
	mSubMeshData[name] = subData;

	mVertexBuffer.insert(mVertexBuffer.end(), vertices.begin(), vertices.end());
	//OutputDebugStringA(string{ "##### " + (to_string(mVertexBuffer.size()) + "\n") }.c_str());

	mIndexBuffer.insert(mIndexBuffer.end(), indices.begin(), indices.end());
}

vector<Vertex>& ResourceManager::GetVertexBuffer()
{
	return mVertexBuffer;
}

vector<uint32_t>& ResourceManager::GetIndexBuffer()
{
	return mIndexBuffer;
}

SubMeshData& ResourceManager::GetSubMeshData(string name)
{
	return mSubMeshData.at(name);
}

SkinnedData& ResourceManager::GetAnimationData(string name)
{
	return mAnimData.at(name);
}

TerrainData& ResourceManager::GetTerrainData()
{
	return mTerrainData;
}
