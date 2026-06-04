#pragma once
#include "MeshData.h"

#include <d3d11.h>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <cstdint>

namespace Graphics
{

    // ---- MeshLoader -------------------------------------------------------------
    // .obj ファイルを解析して MeshData を生成する。
    // DirectXTK や外部ライブラリに依存しない自前実装。
    //
    // 対応フォーマット:
    //   v  (頂点座標)
    //   vn (法線)
    //   vt (UV)
    //   f  (面: 三角形・クワッド両対応)
    //   o / g (オブジェクト / グループ → SubMesh 分割)
    //   usemtl (マテリアル名 → SubMesh 分割トリガー)
    //   mtllib / # → スキップ
    //
    // 制限:
    //   - 法線・UV が存在しない場合はゼロ埋め
    //   - テクスチャ読み込みは別途 TextureRegistry で行う
    // ----------------------------------------------------------------------------
    class MeshLoader
    {
    public:
        // .obj を読み込んで MeshData を返す
        static MeshData LoadObj(ID3D11Device* device, const wchar_t* path)
        {
            // wchar_t → string 変換
            std::wstring ws(path);
            std::string filepath(ws.begin(), ws.end());

            std::ifstream file(filepath);
            if (!file.is_open())
            {
                throw std::runtime_error("MeshLoader: ファイルを開けません: " + filepath);
            }

            // ---- パース用作業バッファ -------------------------------------------
            std::vector<DirectX::SimpleMath::Vector3> positions;
            std::vector<DirectX::SimpleMath::Vector3> normals;
            std::vector<DirectX::SimpleMath::Vector2> uvs;

            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            // 頂点の重複排除マップ: "pi/ti/ni" → インデックス
            std::unordered_map<std::string, uint32_t> vertexCache;

            std::vector<SubMesh> subMeshes;
            uint32_t subMeshStart = 0;

            // ---- 行単位でパース ------------------------------------------------
            std::string line;
            while (std::getline(file, line))
            {
                if (line.empty() || line[0] == '#')
                    continue;

                std::istringstream ss(line);
                std::string token;
                ss >> token;

                if (token == "v")
                {
                    DirectX::SimpleMath::Vector3 p;
                    ss >> p.x >> p.y >> p.z;
                    positions.push_back(p);
                }
                else if (token == "vn")
                {
                    DirectX::SimpleMath::Vector3 n;
                    ss >> n.x >> n.y >> n.z;
                    normals.push_back(n);
                }
                else if (token == "vt")
                {
                    DirectX::SimpleMath::Vector2 uv;
                    ss >> uv.x >> uv.y;
                    uv.y = 1.f - uv.y; // OBJ は Y 上向き・DX は Y 下向き
                    uvs.push_back(uv);
                }
                else if (token == "f")
                {
                    // 面: 最初の頂点を基準に三角形ファン分割（クワッド対応）
                    std::vector<uint32_t> faceIndices;
                    std::string vertStr;
                    while (ss >> vertStr)
                    {
                        faceIndices.push_back(ParseVertex(vertStr, positions, normals, uvs, vertices, vertexCache));
                    }
                    // 三角形ファン: 0-1-2, 0-2-3, ...
                    for (std::size_t i = 1; i + 1 < faceIndices.size(); ++i)
                    {
                        indices.push_back(faceIndices[0]);
                        indices.push_back(faceIndices[i]);
                        indices.push_back(faceIndices[i + 1]);
                    }
                }
                else if (token == "usemtl" || token == "o" || token == "g")
                {
                    // 新しいサブメッシュを開始
                    const uint32_t currentCount = static_cast<uint32_t>(indices.size());
                    if (currentCount > subMeshStart)
                    {
                        SubMesh sm;
                        sm.indexOffset = subMeshStart;
                        sm.indexCount = currentCount - subMeshStart;
                        subMeshes.push_back(sm);
                        subMeshStart = currentCount;
                    }
                }
            }

            // 残りの面を最後の SubMesh として追加
            const uint32_t totalCount = static_cast<uint32_t>(indices.size());
            if (totalCount > subMeshStart)
            {
                SubMesh sm;
                sm.indexOffset = subMeshStart;
                sm.indexCount = totalCount - subMeshStart;
                subMeshes.push_back(sm);
            }
            if (subMeshes.empty())
            {
                SubMesh sm;
                sm.indexOffset = 0;
                sm.indexCount = totalCount;
                subMeshes.push_back(sm);
            }

            // ---- GPU バッファ生成 -----------------------------------------------
            return BuildMeshData(device, vertices, indices, subMeshes);
        }

    private:
        // "pi/ti/ni" 形式の頂点文字列をパースしてインデックスを返す
        static uint32_t ParseVertex(const std::string& vertStr,
                                    const std::vector<DirectX::SimpleMath::Vector3>& positions,
                                    const std::vector<DirectX::SimpleMath::Vector3>& normals,
                                    const std::vector<DirectX::SimpleMath::Vector2>& uvs, std::vector<Vertex>& vertices,
                                    std::unordered_map<std::string, uint32_t>& cache)
        {
            auto it = cache.find(vertStr);
            if (it != cache.end())
                return it->second;

            Vertex v = {};
            int pi = 0, ti = 0, ni = 0;
            char sep;

            std::istringstream ss(vertStr);
            ss >> pi;
            if (ss.peek() == '/')
            {
                ss >> sep;
                if (ss.peek() != '/')
                    ss >> ti;
                if (ss.peek() == '/')
                {
                    ss >> sep >> ni;
                }
            }

            // OBJ は 1-origin・負値は末尾から
            auto resolve = [](int i, int size) -> int { return (i > 0) ? i - 1 : size + i; };

            if (pi != 0 && !positions.empty())
                v.position = positions[resolve(pi, static_cast<int>(positions.size()))];
            if (ti != 0 && !uvs.empty())
                v.texCoord = uvs[resolve(ti, static_cast<int>(uvs.size()))];
            if (ni != 0 && !normals.empty())
                v.normal = normals[resolve(ni, static_cast<int>(normals.size()))];

            uint32_t index = static_cast<uint32_t>(vertices.size());
            vertices.push_back(v);
            cache[vertStr] = index;
            return index;
        }

        // 頂点・インデックス配列から MeshData を生成
        static MeshData BuildMeshData(ID3D11Device* device, const std::vector<Vertex>& vertices,
                                      const std::vector<uint32_t>& indices, const std::vector<SubMesh>& subMeshes)
        {
            MeshData mesh;
            mesh.indexCount = static_cast<UINT>(indices.size());
            mesh.subMeshes = subMeshes;

            // ---- 頂点バッファ（静的）-------------------------------------------
            {
                D3D11_BUFFER_DESC desc = {};
                desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertices.size());
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

                D3D11_SUBRESOURCE_DATA data = {};
                data.pSysMem = vertices.data();
                device->CreateBuffer(&desc, &data, mesh.vertexBuffer.GetAddressOf());
            }

            // ---- インデックスバッファ（静的）------------------------------------
            {
                D3D11_BUFFER_DESC desc = {};
                desc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * indices.size());
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

                D3D11_SUBRESOURCE_DATA data = {};
                data.pSysMem = indices.data();
                device->CreateBuffer(&desc, &data, mesh.indexBuffer.GetAddressOf());
            }

            // ---- インスタンスバッファ（動的）------------------------------------
            mesh.instanceBuffer = MeshData::CreateInstanceBuffer(device);

            return mesh;
        }
    };

} // namespace Graphics