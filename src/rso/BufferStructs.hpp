#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <vector>

#include <glm/ext/scalar_constants.hpp>
#include <glm/mat4x4.hpp>
#include <vulkan/vulkan_raii.hpp>

constexpr uint32_t MAX_AGENTS = 512;

struct alignas(16) AgentData
{
    glm::mat4 camera;
    float data[4];
};
static_assert(sizeof(AgentData) == 80, "AgentData must be 80 bytes (5x vec4) to match GPU shared.slang layout");
static_assert(alignof(AgentData) == 16, "AgentData must be 16-byte aligned for GPU");

struct alignas(16) UBO
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 model;
    uint32_t vertexCount;
    uint32_t agentCount;
    float timeStep;
    uint32_t _padding0;
    AgentData agents[MAX_AGENTS];
};
static_assert(offsetof(UBO, agents) == 208, "UBO::agents offset must be 208 bytes to match GPU shared.slang layout");
static_assert(sizeof(UBO) == 208 + MAX_AGENTS * sizeof(AgentData), "UBO total size must match GPU shared.slang layout");

struct alignas(16) VertexCoords
{
    float values[4];
    VertexCoords() = default;
    explicit VertexCoords(const std::array<float, 4>& input)
        { std::copy(input.begin(), input.end(), values); }
};
static_assert(sizeof(VertexCoords) == 16, "VertexCoords must be 16 bytes (1x vec4)");

struct alignas(16) VertexData
{
    float values[4];
    VertexData() = default;
    explicit VertexData(const std::array<float, 4>& input)
        { std::copy(input.begin(), input.end(), values); }
};
static_assert(sizeof(VertexData) == 16, "VertexData must be 16 bytes (1x vec4)");

struct Mesh
{
    std::vector<VertexCoords> vertices;
    std::vector<VertexData> vertexData;

    Mesh() = default;

    static constexpr double kEarthRadiusKm = 6378.135;

    static inline bool isMeshPointInFrance(const glm::vec3& pos)
    {
        // 1. Latitude: Angle from the XZ equatorial plane
        // Returns radians in the range [-pi/2, pi/2]
        const double latRad = std::asin(pos.y);

        // 2. Longitude: Angle around the Y polar axis
        // Based on your specific rotation matrix, -Z is East and +X is the Prime Meridian
        // Returns radians in the range [-pi, pi]
        const double lonRad = std::atan2(-pos.z, pos.x);

        // 3. Convert to degrees
        constexpr double radToDeg = 180.0 / glm::pi<double>();
        const double lat = latRad * radToDeg;
        const double lon = lonRad * radToDeg;

        // 4. Evaluate against the Metropolitan France BBOX
        return (lon >= -5.5 && lon <= 9.6) && 
               (lat >= 41.3 && lat <= 51.1);
    }

    // Checks whether a normalized 3D mesh point lies inside a local surface rectangle
    // centered at (centerLatDeg, centerLonDeg) with dimensions widthKm (East-West) x heightKm (North-South).
    static inline bool isMeshPointInGeoRect(
        double centerLatDeg,
        double centerLonDeg,
        double widthKm,
        double heightKm,
        const glm::dvec3& pos)
    {
        constexpr double degToRad = glm::pi<double>() / 180.0;
        const double lat0 = centerLatDeg * degToRad;
        const double lon0 = centerLonDeg * degToRad;

        const double cosLat0 = std::cos(lat0);
        const double sinLat0 = std::sin(lat0);
        const double cosLon0 = std::cos(lon0);
        const double sinLon0 = std::sin(lon0);

        // 1. Center unit vector r0 adhering to your frame:
        //    pos.x = cos(lat) * cos(lon)
        //    pos.y = sin(lat)
        //    pos.z = -cos(lat) * sin(lon)
        const glm::dvec3 r0(
            cosLat0 * cosLon0,
            sinLat0,
            -cosLat0 * sinLon0
        );

        // Must be on the forward-facing hemisphere relative to the center
        const double cosAngle = glm::dot(pos, r0);
        if (cosAngle <= 0.0) { return false; }

        // 2. Unit tangent vector pointing East: d/d(lon) normalized
        //    d(pos.x)/d(lon) = -cos(lat) * sin(lon)
        //    d(pos.y)/d(lon) = 0
        //    d(pos.z)/d(lon) = -cos(lat) * cos(lon)
        const glm::dvec3 eastDir(-sinLon0, 0.0, -cosLon0);

        // 3. Unit tangent vector pointing North: d/d(lat) normalized
        //    d(pos.x)/d(lat) = -sin(lat) * cos(lon)
        //    d(pos.y)/d(lat) = cos(lat)
        //    d(pos.z)/d(lat) = sin(lat) * sin(lon)
        const glm::dvec3 northDir(
            -sinLat0 * cosLon0,
            cosLat0,
            sinLat0 * sinLon0
        );

        // 4. Central projection (Gnomonic) onto the tangent plane at distance R_E
        //    r_plane = pos / (pos . r0)
        //    This preserves great-circle geodesics as straight lines
        const glm::dvec3 diff = (pos / cosAngle) - r0;

        // 5. Calculate the east and north displacements in kilometers
        // Mean volumetric Earth radius in kilometers (WGS-84 / IUGG standard)
        const double deltaEastKm  = kEarthRadiusKm * glm::dot(diff, eastDir);
        const double deltaNorthKm = kEarthRadiusKm * glm::dot(diff, northDir);

        const double halfWidth  = widthKm * 0.5;
        const double halfHeight = heightKm * 0.5;

        return (std::abs(deltaEastKm) <= halfWidth) && (std::abs(deltaNorthKm) <= halfHeight);
    }

    // Helper to calculate the exact solid angle of a rectangle on the z=1 face
    static inline double solidAngle(double x, double y)
        { return std::atan((x * y) / std::sqrt(x * x + y * y + 1.0)); }

    // Populates a point-cloud cubesphere. 
    // 'resolution' is the number of cells per dimension on a single face.
    inline void populateMesh(uint32_t resolution)
    {
        const uint32_t cellsPerFace = resolution * resolution;
        const uint32_t totalCells = 6 * cellsPerFace;
        const size_t vertexBytes = static_cast<size_t>(totalCells) * sizeof(VertexCoords);
        const size_t vertexDataBytes = static_cast<size_t>(totalCells) * sizeof(VertexData);
        const size_t totalBytes = vertexBytes + vertexDataBytes;
        const double totalMB = static_cast<double>(totalBytes) / (1024.0 * 1024.0);
        const double totalGB = static_cast<double>(totalBytes) / (1024.0 * 1024.0 * 1024.0);
        double totalSurfaceArea = 0.0;

        std::printf("[Mesh] total cells: %u\n", totalCells);
        if (totalBytes < (1ull << 30))
            { std::printf("[Mesh] total required memory: %.2f MB\n", totalMB); }
        else
            { std::printf("[Mesh] total required memory: %.2f GB\n", totalGB); }
        
        // 4 * resolution * cellSize == 2 * pi * kEarthRadiusKm
        const double maxCellSizeKm = (glm::pi<double>() * kEarthRadiusKm) / (2.0 * static_cast<double>(resolution));
        const double maxCellSizeM  = maxCellSizeKm * 1000.0;

        std::printf("[Mesh] total cells: %u\n", totalCells);
        std::printf("[Mesh] maximum cell size: %.3f km (%.1f m)\n", maxCellSizeKm, maxCellSizeM);

        vertices.clear(); vertices.reserve(totalCells);
        vertexData.clear(); vertexData.reserve(totalCells);

        const double invRes = 1.0 / static_cast<double>(resolution);
        const double pi_over_4 = glm::pi<double>() / 4.0;

        for (int face = 0; face < 6; ++face)
        {
            for (uint32_t i = 0; i < resolution; ++i)
            {
                for (uint32_t j = 0; j < resolution; ++j)
                {
                    // 1. Grid coordinates mapped to [-1.0, 1.0]
                    const double u0 = -1.0 + 2.0 * (static_cast<double>(i) * invRes);
                    const double u1 = -1.0 + 2.0 * (static_cast<double>(i + 1) * invRes);
                    const double v0 = -1.0 + 2.0 * (static_cast<double>(j) * invRes);
                    const double v1 = -1.0 + 2.0 * (static_cast<double>(j + 1) * invRes);

                    const double uCenter = (u0 + u1) * 0.5;
                    const double vCenter = (v0 + v1) * 0.5;

                    // 2. Equi-angular Warp (minimizes area dispersion)
                    const double x0 = std::tan(u0 * pi_over_4);
                    const double x1 = std::tan(u1 * pi_over_4);
                    const double y0 = std::tan(v0 * pi_over_4);
                    const double y1 = std::tan(v1 * pi_over_4);

                    const double xCenter = std::tan(uCenter * pi_over_4);
                    const double yCenter = std::tan(vCenter * pi_over_4);

                    // 3. Exact surface area calculation (Solid Angle of the cell)
                    const double area = solidAngle(x1, y1) - solidAngle(x0, y1) - solidAngle(x1, y0) + solidAngle(x0, y0);

                    // 4. Map the 2D face coordinates to 3D cube coordinates based on the face index
                    glm::vec3 pos;
                    switch (face)
                    {
                        case 0: pos = glm::vec3( 1.0f, -yCenter, -xCenter); break; // +X
                        case 1: pos = glm::vec3(-1.0f, -yCenter,  xCenter); break; // -X
                        case 2: pos = glm::vec3( xCenter,  1.0f,  yCenter); break; // +Y
                        case 3: pos = glm::vec3( xCenter, -1.0f, -yCenter); break; // -Y
                        case 4: pos = glm::vec3( xCenter, -yCenter,  1.0f); break; // +Z
                        case 5: pos = glm::vec3(-xCenter, -yCenter, -1.0f); break; // -Z
                    }

                    // 5. Normalize to project onto the sphere
                    pos = glm::normalize(pos);

                    // 6. Define payload parameters
                    constexpr float alpha = 1.0f;
                    constexpr float epsilon = 0.0f; //0.0004f;
                    constexpr float v = 1.0f;
                    const float s = static_cast<float>(area);
                    constexpr float w = 1.0f;

                    // 7. Add vertex to GPU arrays
                    //if (isMeshPointInFrance(pos) && !isMeshPointInGeoRect(48.8566, 2.3522, 500.0, 500.0, pos))
                    if ( isMeshPointInGeoRect(50.4500, 30.5233, 300.0, 300.0, pos)  // Kiyv 300x300
                      || isMeshPointInGeoRect(31.5075, 34.4597, 100.0, 300.0, pos)  // Gaza 100x300
                      || isMeshPointInGeoRect(12.6392, 8.0025, 200.0, 200.0, pos) ) // Mali, Bamako 200x200
                    {
                        vertices.emplace_back(std::array<float, 4>{
                            static_cast<float>(pos.x),
                            static_cast<float>(pos.y),
                            static_cast<float>(pos.z),
                            alpha
                        });
                        vertexData.emplace_back(std::array<float, 4>{ epsilon, v, s, w });
                        totalSurfaceArea += area;
                    }
                }
            }
            std::printf("[Mesh] generated face %d/6\n", face + 1);
        }
        std::printf("[Mesh] total generated surface area: %.10f (ideal: %.10f)\n", 
            totalSurfaceArea, 4.0 * glm::pi<double>());
    }
};

static constexpr std::array<vk::VertexInputBindingDescription, 1> kVertexCoordsBindingDescriptions {{
    {
        .binding = 0,
        .stride = sizeof(VertexCoords),
        .inputRate = vk::VertexInputRate::eVertex,
    }
}};

static constexpr std::array<vk::VertexInputAttributeDescription, 1> kVertexCoordsAttributeDescriptions {{
    {
        .location = 0,
        .binding = 0,
        .format = vk::Format::eR32G32B32A32Sfloat,
        .offset = static_cast<uint32_t>(offsetof(VertexCoords, values)),
    }
}};

static constexpr std::array<vk::VertexInputBindingDescription, 1> kVertexDataBindingDescriptions {{
    {
        .binding = 1,
        .stride = sizeof(VertexData),
        .inputRate = vk::VertexInputRate::eVertex,
    }
}};

static constexpr std::array<vk::VertexInputAttributeDescription, 1> kVertexDataAttributeDescriptions {{
    {
        .location = 1,
        .binding = 1,
        .format = vk::Format::eR32G32B32A32Sfloat,
        .offset = static_cast<uint32_t>(offsetof(VertexData, values)),
    }
}};
