#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// ============================================================
// Push Constants — mixed types, tight packing
// ============================================================
layout(push_constant) uniform PushData {
    mat4   mvp;
    vec4   tint;
    vec2   uvOffset;
    float  time;
    int    instanceBase;
    ivec2  gridSize;
    float  pad0;              // explicit padding to test offset calc
    uint   flags;
} push;

// ============================================================
// Set 0: Deeply nested structs with mixed-size matrices
// ============================================================
struct BoneWeight {
    float  weight;
    uint   index;
};

struct SkinCluster {
    BoneWeight  weights[4];
    mat4        bindPose;
    mat3x4      dualQuat;       // 3×4 non-square matrix
};

struct SubMeshInfo {
    uint          vertexOffset;
    uint          indexOffset;
    uint          materialID;
    float         lodBias;
    SkinCluster   skin;
    mat2          uvRotation;    // 2×2
    vec4          boundingSphere;
};

struct MeshNode {
    mat4          transform;
    mat4x3        normalMatrix;  // 4×3 non-square
    SubMeshInfo   submeshes[3];
    float         morphWeights[8];
    int           parentIndex;
    uint          childCount;
    vec2          _padding;
};

layout(set = 0, binding = 0, std430) readonly buffer SceneGraph {
    uint        nodeCount;
    float       globalScale;
    vec2        _hdr0;
    MeshNode    nodes[];         // runtime array of nested structs
} sceneGraph;

// ============================================================
// Set 0: Array of uniform buffers with nested structs
// ============================================================
struct LightAttenuation {
    float constant;
    float linear;
    float quadratic;
    float cutoff;
};

struct ShadowCascade {
    mat4   viewProj;
    float  splitDepth;
    float  bias;
    vec2   texelSize;
};

struct LightData {
    vec4              position;
    vec4              direction;
    vec4              color;
    mat4x2            cookieTransform;  // 4×2
    LightAttenuation  atten;
    ShadowCascade     cascades[4];
    uint              typeFlags;
    float             intensity;
    vec2              spotAngles;
};

layout(set = 0, binding = 1, std140) uniform LightBlock {
    LightData  lights[16];
    uint       activeLightCount;
    float      ambientIntensity;
    vec2       _pad;
    mat3       envRotation;      // 3×3
} lighting;

// ============================================================
// Set 1: Material system — arrays of structs, nested arrays
// ============================================================
struct TextureRef {
    uint   textureIndex;
    uint   samplerIndex;
    vec2   tilingScale;
    vec2   tilingOffset;
    float  mipBias;
    float  blendFactor;
};

struct MaterialLayer {
    TextureRef  albedo;
    TextureRef  normal;
    TextureRef  roughMetalOcc;    // packed R/M/O
    vec4        baseColor;
    float       metallicFactor;
    float       roughnessFactor;
    float       emissiveStrength;
    float       alphaClip;
    mat2x3      texCoordTransform; // 2×3
    uint        blendMode;
    uint        shadingModel;
    vec2        _reserved;
};

struct MaterialDescriptor {
    MaterialLayer  layers[4];
    float          ior;
    float          subsurfaceRadius;
    float          clearcoatFactor;
    float          clearcoatRoughness;
    vec4           sheenColor;
    uvec4          featureFlags;
    mat3           tangentBasis;    // per-material override
};

layout(set = 1, binding = 0, std430) readonly buffer MaterialBuffer {
    MaterialDescriptor materials[];  // runtime array
} materialBuf;

// ============================================================
// Set 1: Indirect draw args + instance data
// ============================================================
struct PerInstance {
    mat4   model;
    mat4   prevModel;
    mat3x4 cofactorMatrix;     // 3×4 for normal transform
    uvec4  packed;             // materialID, meshID, flags, userData
    vec4   customParams[2];
};

layout(set = 1, binding = 1, std430) readonly buffer InstanceBuffer {
    PerInstance instances[];    // runtime array
} instanceBuf;

// ============================================================
// Set 2: Fixed-size arrays + combined image samplers
// ============================================================
struct ParticleState {
    vec4   positionLife;
    vec4   velocityMass;
    mat2   spin;               // 2×2 rotation
    vec2   size;
    vec2   _p;
};

layout(set = 2, binding = 0, std430) buffer ParticlePool {
    uint           aliveCount;
    uint           deadCount;
    uvec2          _hdr;
    ParticleState  particles[4096];  // fixed-size large array
} particlePool;

layout(set = 2, binding = 1) uniform sampler2D textures2D[64];       // array of combined image samplers
layout(set = 2, binding = 2) uniform samplerCube textureCubes[8];    // array of cube samplers
layout(set = 2, binding = 3) uniform sampler3D   volumeTextures[4];  // array of 3D samplers

// ============================================================
// Set 3: Misc — uniform texel buffer, storage images, etc.
// ============================================================
struct JointPose {
    vec4  rotation;   // quaternion
    vec4  translation;
    vec4  scale;
};

struct AnimClip {
    JointPose  joints[128];
    float      timestamp;
    float      duration;
    uint       loopMode;
    uint       _pad;
};

layout(set = 3, binding = 0, std430) readonly buffer AnimationBank {
    AnimClip clips[];           // runtime array of struct with big inner array
} animBank;

layout(set = 3, binding = 1, std140) uniform GlobalUniforms {
    mat4   view;
    mat4   proj;
    mat4   viewProj;
    mat4   invView;
    mat4   invProj;
    mat4   prevViewProj;
    vec4   cameraPos;
    vec4   cameraDir;
    vec4   viewport;           // x, y, w, h
    vec4   nearFarFov;         // near, far, fovY, aspect
    uvec4  frameInfo;          // frameIndex, totalFrames, flags, rngSeed
    mat2x4 jitterSequence;    // 2×4 for TAA jitter
} globals;

layout(set = 3, binding = 2, r32f) uniform image2D  heightmapRW;        // storage image
layout(set = 3, binding = 3)       uniform sampler   immutableSamplers[3]; // sampler array

// ============================================================
// Vertex inputs
// ============================================================
layout(location = 0) in vec3  inPosition;
layout(location = 1) in vec3  inNormal;
layout(location = 2) in vec4  inTangent;
layout(location = 3) in vec2  inUV0;
layout(location = 4) in vec2  inUV1;
layout(location = 5) in uvec4 inBoneIDs;
layout(location = 6) in vec4  inBoneWeights;
layout(location = 7) in uint  inInstanceID;

// ============================================================
// Outputs to fragment
// ============================================================
layout(location = 0) out vec3  fragWorldPos;
layout(location = 1) out vec3  fragNormal;
layout(location = 2) out vec2  fragUV;
layout(location = 3) out flat uint fragMaterialID;

// ============================================================
// Main — just enough logic to reference everything so nothing
// gets stripped by the compiler.
// ============================================================
void main() {
    uint iid = inInstanceID + push.instanceBase;
    PerInstance inst = instanceBuf.instances[iid];
    uint matID = inst.packed.x;
    uint meshID = inst.packed.y;

    // Touch the scene graph (runtime array of nested structs)
    MeshNode node = sceneGraph.nodes[meshID];
    float morphSum = 0.0;
    for (int i = 0; i < 8; i++) {
        morphSum += node.morphWeights[i];
    }

    // Touch skinning data inside nested struct
    SkinCluster skin = node.submeshes[0].skin;
    mat4 skinMatrix = mat4(0.0);
    for (int b = 0; b < 4; b++) {
        if (skin.weights[b].index == inBoneIDs[b]) {
            skinMatrix += skin.weights[b].weight * skin.bindPose;
        }
    }

    // Touch lighting UBO (nested cascades, non-square cookie matrix)
    float shadowFactor = 1.0;
    for (uint li = 0; li < lighting.activeLightCount && li < 16; li++) {
        LightData ld = lighting.lights[li];
        vec4 shadowCoord = ld.cascades[0].viewProj * vec4(inPosition, 1.0);
        vec2 cookieUV = ld.cookieTransform * vec4(inPosition.xy, 0.0, 1.0);
        shadowFactor *= ld.atten.quadratic;
    }

    // Touch materials (runtime array, nested layers, non-square matrices)
    MaterialDescriptor mat = materialBuf.materials[matID];
    vec3 tcoord = mat.layers[0].texCoordTransform * inUV0;
    float roughness = mat.layers[0].roughnessFactor * mat.clearcoatRoughness;

    // Touch particle pool (fixed-size array inside buffer)
    if (particlePool.aliveCount > 0u) {
        ParticleState ps = particlePool.particles[0];
        morphSum += ps.positionLife.w;
        vec2 spun = ps.spin * ps.size;
    }

    // Touch array-of-samplers (forces descriptor indexing)
    vec4 texSample = texture(textures2D[matID % 64], inUV0 + push.uvOffset);
    vec4 cubeSample = texture(textureCubes[0], inNormal);
    vec4 volSample = texture(volumeTextures[0], inPosition * 0.01);

    // Touch animation bank (runtime array, big inner array)
    JointPose jp = animBank.clips[0].joints[inBoneIDs.x];
    vec4 animOffset = jp.translation * jp.scale;

    // Touch global UBO (many matrices, non-square jitter)
    vec4 jitter = globals.jitterSequence * push.uvOffset;

    // Touch storage image
    vec4 heightVal = imageLoad(heightmapRW, ivec2(push.gridSize));

    // Touch immutable sampler array (just to keep it alive)
    // (can't actually sample without a texture, but the descriptor stays)

    // Touch non-square matrices from various places
    vec3 envNormal = lighting.envRotation * inNormal;
    vec3 tangentN  = mat.tangentBasis * inNormal;
    vec4 cofN      = inst.cofactorMatrix * inNormal;
    vec3 nodeN     = (node.normalMatrix * vec4(inNormal, 0.0));

    // Use the push constant mat2 from submesh
    vec2 rotUV = node.submeshes[0].uvRotation * inUV0;

    // Final position
    vec4 worldPos = inst.model * (skinMatrix * vec4(inPosition, 1.0) + animOffset);
    worldPos.xyz += morphSum * 0.0001;
    worldPos.xyz += texSample.xyz * 0.0001;
    worldPos.xyz += cubeSample.xyz * 0.0001;
    worldPos.xyz += volSample.xyz * 0.0001;
    worldPos.xyz += heightVal.xyz * 0.0001;
    worldPos.xyz += tcoord * 0.0001;
    worldPos.xy  += rotUV * 0.0001;
    worldPos.xyz += envNormal * 0.0001;
    worldPos.xyz += tangentN * 0.0001;
    worldPos.xyz += cofN.xyz * 0.0001;
    worldPos.xyz += nodeN * 0.0001;
    worldPos.xyz += jitter.xyz * 0.0001;
    worldPos.x   += roughness * 0.0001;
    worldPos.x   += shadowFactor * 0.0001;

    fragWorldPos  = worldPos.xyz;
    fragNormal    = normalize((inst.model * vec4(inNormal, 0.0)).xyz);
    fragUV        = inUV0 + push.uvOffset;
    fragMaterialID = matID;

    gl_Position = push.mvp * worldPos;
}
