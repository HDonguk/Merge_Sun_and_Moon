#pragma once
#include "stdafx.h"
#include "FbxExtractor.h"
#include "Info.h"

struct TerrainData {
	int terrainWidth;
	int terrainHeight;
	int terrainScale;
};

class ResourceManager
{
public:
	ResourceManager();
	~ResourceManager();
	void LoadFbx(const string& fileName, bool onlyAnimation, bool zUp);
	void CreatePlane(const string& name, float size, float wrap);
	void CreateTerrain(const string& name, int maxheight, int scale, int maxUV);
	vector<Vertex>& GetVertexBuffer();
	vector<uint32_t>& GetIndexBuffer();
	SubMeshData& GetSubMeshData(string name);
	SkinnedData& GetAnimationData(string name);
	TerrainData& GetTerrainData();
private:
	unique_ptr<FbxExtractor> mFbxExtractor;
	vector<Vertex> mVertexBuffer;
	vector<uint32_t> mIndexBuffer;
	unordered_map<string, SubMeshData> mSubMeshData;
	unordered_map<string, SkinnedData> mAnimData;
	TerrainData mTerrainData;
};

