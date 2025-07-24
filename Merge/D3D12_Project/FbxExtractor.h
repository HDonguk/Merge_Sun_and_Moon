#pragma once
#include <fbxsdk.h>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <DirectXMath.h>
#include "SkinnedData.h"
#include "Info.h"

template<typename T> using ptr = T*;
using namespace DirectX;
using namespace std;


class FbxExtractor
{
public:
	FbxExtractor();
	~FbxExtractor();
	void ImportFbxFile(const string&, bool ,bool);
	void ExtractDataFromFbx();
	vector<Vertex>& GetVertices();
	//vector<pair<string, int>>& GetBoneHierarchy();
	vector<int>& GetBoneHierarchyIndex();
	vector<vector<pair<int, float>>>& GetControlPointsWeight();
	vector<XMFLOAT4X4>& GetOffsetMatrix();
	unordered_map<string, AnimationClip>& GetAnimation();
	void ResetAndClear();

private:
	void ConvertSceneAxisSystem(FbxAxisSystem::EUpVector, FbxAxisSystem::EFrontVector, FbxAxisSystem::ECoordSystem);
	//inline void TraverseNode(ptr<FbxNode>);
	inline void TraverseNodeForSkeleton(ptr<FbxNode>);
	inline void TraverseNodeForMesh(ptr<FbxNode>);
	inline void TraverseNodeForAnimation(ptr<FbxNode>, ptr<FbxTakeInfo>, FbxTime::EMode);
	void ExtractMeshData(ptr<FbxNode>);
	void ExtractBoneHierarchy(ptr<FbxNode>);
	void ExtractAnimationData(ptr<FbxNode>);
	void RemoveBadPolygons();
	bool ConvertScenePolygonsTriangulate();
	bool ConvertNodeAttributePolygonsTriangulate(ptr<FbxNode>);
	void CheckMeshDataState(ptr<FbxMesh>);
	void ProcessAnimationLayer(ptr<FbxNode>, ptr<FbxAnimLayer>);
	void ProcessCurves(ptr<FbxAnimCurve>);
	int FindBoneIndex(const string& name);
	void ExtractWeightAndOffsetMatrix(ptr<FbxMesh>);
	void NormalizeWeight();
	XMFLOAT4X4 ToXMFloat4x4(FbxAMatrix&);
	XMFLOAT3 ToXMFloat3(FbxVector4&);
	XMFLOAT4 ToXMFloat4(FbxVector4&);
	XMFLOAT4 ToXMFloat4(FbxQuaternion&);

	ptr<FbxManager> mFbxManager;
	ptr<FbxIOSettings> mFbxIOS;
	ptr<FbxImporter> mFbxImporter;
	ptr<FbxScene> mFbxScene;

	// ���̴� �Է� ����
	vector<vector<pair<int, float>>> mControlPointsWeight; // ������ ����ġ ���� 4���� �Ѿ� ���� ����ȭ�� �������
	vector<Vertex> mVertices;

	// ������� ������
	vector<int> mBoneHierarchyIndex; // �ܺη� ���޿�
	vector<string> mBoneHierarchyName; // Fbx ���ο��� ����ϱ� ����
	//vector<pair<string, int>> mBoneHierarchy;
	vector<XMFLOAT4X4> mOffsetMatrix;
	unordered_map<string, AnimationClip> mAnimations;

	// �÷��� ����
	bool mUV;
	bool mIsFirst;
	bool mBone;
	bool mNormalize;
	bool mOnlyAnimation;

	string mDirectory = "./Fbxs/";
};

