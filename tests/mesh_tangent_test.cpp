/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
#include <render/mesh.h>
#include <render/logger.h>

#include <vector>
#include <iostream>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

bool CheckOrthonormalBasis(const Vector3& n, const Vector3& t, const Vector3& b, float epsilon = 1e-3f) {
    float nLen = n.norm();
    float tLen = t.norm();
    float bLen = b.norm();

    if (std::abs(nLen - 1.0f) > epsilon || std::abs(tLen - 1.0f) > epsilon || std::abs(bLen - 1.0f) > epsilon) {
        std::cerr << "Vector normalization failed: |N|=" << nLen << " |T|=" << tLen << " |B|=" << bLen << '\n';
        return false;
    }

    if (std::abs(n.dot(t)) > epsilon || std::abs(n.dot(b)) > epsilon) {
        std::cerr << "Orthogonality check failed: N·T=" << n.dot(t) << " N·B=" << n.dot(b) << '\n';
        return false;
    }

    float handedness = n.cross(t).dot(b);
    if (std::abs(std::abs(handedness) - 1.0f) > epsilon) {
        std::cerr << "Handedness magnitude check failed: |cross(N,T)·B|=" << handedness << '\n';
        return false;
    }

    return true;
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    Logger::GetInstance().SetLogToFile(false);
    Logger::GetInstance().SetLogToConsole(false);

    std::vector<Vertex> vertices(4);
    std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

    vertices[0].position = Vector3(0.0f, 0.0f, 0.0f);
    vertices[1].position = Vector3(1.0f, 0.0f, 0.0f);
    vertices[2].position = Vector3(1.0f, 0.0f, 1.0f);
    vertices[3].position = Vector3(0.0f, 0.0f, 1.0f);

    vertices[0].texCoord = Vector2(0.0f, 0.0f);
    vertices[1].texCoord = Vector2(1.0f, 0.0f);
    vertices[2].texCoord = Vector2(1.0f, 1.0f);
    vertices[3].texCoord = Vector2(0.0f, 1.0f);

    for (auto& v : vertices) {
        v.normal = Vector3(0.0f, 1.0f, 0.0f);
    }

    Mesh mesh;
    mesh.SetData(vertices, indices);
    mesh.RecalculateTangents();

    bool allOk = true;
    mesh.AccessVertices([&](const std::vector<Vertex>& data) {
        for (size_t i = 0; i < data.size(); ++i) {
            const auto& v = data[i];
            if (!CheckOrthonormalBasis(v.normal, v.tangent, v.bitangent)) {
                std::cerr << "Vertex " << i << " failed tangent basis check." << std::endl;
                allOk = false;
            }
        }
    });

    if (!allOk) {
        std::cerr << "Mesh tangent space validation failed." << std::endl;
        return 1;
    }

    std::cout << "Mesh tangent space validation passed." << std::endl;
    return 0;
}
