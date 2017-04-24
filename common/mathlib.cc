/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

#include <common/cmdlib.hh>
#include <common/mathlib.hh>
#include <common/polylib.hh>
#include <assert.h>

#include <tuple>
#include <map>

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/closest_point.hpp>

using namespace polylib;

const vec3_t vec3_origin = { 0, 0, 0 };

qboolean
VectorCompare(const vec3_t v1, const vec3_t v2)
{
    int i;

    for (i = 0; i < 3; i++)
        if (fabs(v1[i] - v2[i]) > EQUAL_EPSILON)
            return false;

    return true;
}

void
CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross)
{
    cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
    cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
    cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

/*
 * VecStr - handy shortcut for printf, not thread safe, obviously
 */
const char *
VecStr(const vec3_t vec)
{
    static char buffers[8][20];
    static int current = 0;
    char *buf;

    buf = buffers[current++ & 7];
    q_snprintf(buf, sizeof(buffers[0]), "%i %i %i",
             (int)vec[0], (int)vec[1], (int)vec[2]);

    return buf;
}

const char *
VecStrf(const vec3_t vec)
{
    static char buffers[8][20];
    static int current = 0;
    char *buf;

    buf = buffers[current++ & 7];
    q_snprintf(buf, sizeof(buffers[0]), "%.2f %.2f %.2f",
             vec[0], vec[1], vec[2]);

    return buf;
}

// from http://mathworld.wolfram.com/SpherePointPicking.html
// eqns 6,7,8
void
UniformPointOnSphere(vec3_t dir, float u1, float u2)
{
    Q_assert(u1 >= 0 && u1 <= 1);
    Q_assert(u2 >= 0 && u2 <= 1);
    
    const vec_t theta = u1 * 2.0 * Q_PI;
    const vec_t u = (2.0 * u2) - 1.0;
    
    const vec_t s = sqrt(1.0 - (u * u));
    dir[0] = s * cos(theta);
    dir[1] = s * sin(theta);
    dir[2] = u;
    
    for (int i=0; i<3; i++) {
        Q_assert(dir[i] >= -1.001);
        Q_assert(dir[i] <=  1.001);
    }
}

void
RandomDir(vec3_t dir)
{
    UniformPointOnSphere(dir, Random(), Random());
}

glm::vec3 CosineWeightedHemisphereSample(float u1, float u2)
{
    Q_assert(u1 >= 0.0f && u1 <= 1.0f);
    Q_assert(u2 >= 0.0f && u2 <= 1.0f);
    
    // Generate a uniform sample on the unit disk
    // http://mathworld.wolfram.com/DiskPointPicking.html
    const float sqrt_u1 = sqrt(u1);
    const float theta = 2.0f * Q_PI * u2;
    
    const float x = sqrt_u1 * cos(theta);
    const float y = sqrt_u1 * sin(theta);
    
    // Project it up onto the sphere (calculate z)
    //
    // We know sqrt(x^2 + y^2 + z^2) = 1
    // so      x^2 + y^2 + z^2 = 1
    //         z = sqrt(1 - x^2 - y^2)
    
    const float temp = 1.0f - x*x - y*y;
    const float z = sqrt(qmax(0.0f, temp));
    
    return glm::vec3(x, y, z);
}

glm::vec3 vec_from_mangle(const glm::vec3 &m)
{
    const glm::vec3 mRadians = m * static_cast<float>(Q_PI / 180.0f);
    const glm::mat3x3 rotations = RotateAboutZ(mRadians[0]) * RotateAboutY(-mRadians[1]);
    const glm::vec3 v = rotations * glm::vec3(1,0,0);
    return v;
}

glm::vec3 mangle_from_vec(const glm::vec3 &v)
{
    const glm::vec3 up(0, 0, 1);
    const glm::vec3 east(1, 0, 0);
    const glm::vec3 north(0, 1, 0);
    
    // get rotation about Z axis
    float x = glm::dot(east, v);
    float y = glm::dot(north, v);
    float theta = atan2f(y, x);
    
    // get angle away from Z axis
    float cosangleFromUp = glm::dot(up, v);
    cosangleFromUp = qmin(qmax(-1.0f, cosangleFromUp), 1.0f);
    float radiansFromUp = acosf(cosangleFromUp);
    
    const glm::vec3 mangle = glm::vec3(theta, -(radiansFromUp - Q_PI/2.0), 0) * static_cast<float>(180.0f / Q_PI);
    return mangle;
}

glm::mat3x3 RotateAboutX(float t)
{
    //https://en.wikipedia.org/wiki/Rotation_matrix#Examples

    const float cost = cos(t);
    const float sint = sin(t);
    
    return glm::mat3x3 {
        1, 0, 0, //col0
        0, cost, sint, // col1
        0, -sint, cost // col1
    };
}

glm::mat3x3 RotateAboutY(float t)
{
    const float cost = cos(t);
    const float sint = sin(t);
    
    return glm::mat3x3{
        cost, 0, -sint, // col0
        0, 1, 0, // col1
        sint, 0, cost //col2
    };
}

glm::mat3x3 RotateAboutZ(float t)
{
    const float cost = cos(t);
    const float sint = sin(t);
    
    return glm::mat3x3{
        cost, sint, 0, // col0
        -sint, cost, 0, // col1
        0, 0, 1 //col2
    };
}

// Returns a 3x3 matrix that rotates (0,0,1) to the given surface normal.
glm::mat3x3 RotateFromUpToSurfaceNormal(const glm::vec3 &surfaceNormal)
{
    const glm::vec3 up(0, 0, 1);
    const glm::vec3 east(1, 0, 0);
    const glm::vec3 north(0, 1, 0);
    
    // get rotation about Z axis
    float x = glm::dot(east, surfaceNormal);
    float y = glm::dot(north, surfaceNormal);
    float theta = atan2f(y, x);
    
    // get angle away from Z axis
    float cosangleFromUp = glm::dot(up, surfaceNormal);
    cosangleFromUp = qmin(qmax(-1.0f, cosangleFromUp), 1.0f);
    float radiansFromUp = acosf(cosangleFromUp);
    
    const glm::mat3x3 rotations = RotateAboutZ(theta) * RotateAboutY(radiansFromUp);
    return rotations;
}

bool AABBsDisjoint(const vec3_t minsA, const vec3_t maxsA,
                   const vec3_t minsB, const vec3_t maxsB)
{
    for (int i=0; i<3; i++) {
        if (maxsA[i] < (minsB[i] - EQUAL_EPSILON)) return true;
        if (minsA[i] > (maxsB[i] + EQUAL_EPSILON)) return true;
    }
    return false;
}

void AABB_Init(vec3_t mins, vec3_t maxs, const vec3_t pt) {
    VectorCopy(pt, mins);
    VectorCopy(pt, maxs);
}

void AABB_Expand(vec3_t mins, vec3_t maxs, const vec3_t pt) {
    for (int i=0; i<3; i++) {
        mins[i] = qmin(mins[i], pt[i]);
        maxs[i] = qmax(maxs[i], pt[i]);
    }
}

void AABB_Size(const vec3_t mins, const vec3_t maxs, vec3_t size_out) {
    for (int i=0; i<3; i++) {
        size_out[i] = maxs[i] - mins[i];
    }
}

void AABB_Grow(vec3_t mins, vec3_t maxs, const vec3_t size) {
    for (int i=0; i<3; i++) {
        mins[i] -= size[i];
        maxs[i] += size[i];
    }
}

glm::vec3 Barycentric_FromPoint(const glm::vec3 &p, const tri_t &tri)
{
    using std::get;
    
    const glm::vec3 v0 = get<1>(tri) - get<0>(tri);
    const glm::vec3 v1 = get<2>(tri) - get<0>(tri);
    const glm::vec3 v2 =           p - get<0>(tri);
    float d00 = glm::dot(v0, v0);
    float d01 = glm::dot(v0, v1);
    float d11 = glm::dot(v1, v1);
    float d20 = glm::dot(v2, v0);
    float d21 = glm::dot(v2, v1);
    float invDenom = (d00 * d11 - d01 * d01);
    invDenom = 1.0/invDenom;
    
    glm::vec3 res;
    res[1] = (d11 * d20 - d01 * d21) * invDenom;
    res[2] = (d00 * d21 - d01 * d20) * invDenom;
    res[0] = 1.0f - res[1] - res[2];
    return res;
}

// from global illumination total compendium p. 12
glm::vec3 Barycentric_Random(const float r1, const float r2)
{
    glm::vec3 res;
    res.x = 1.0f - sqrtf(r1);
    res.y = r2 * sqrtf(r1);
    res.z = 1.0f - res.x - res.y;
    return res;
}

/// Evaluates the given barycentric coord for the given triangle
glm::vec3 Barycentric_ToPoint(const glm::vec3 &bary,
                              const tri_t &tri)
{
    using std::get;
    
    const glm::vec3 pt = \
          (bary.x * get<0>(tri))
        + (bary.y * get<1>(tri))
        + (bary.z * get<2>(tri));
    
    return pt;
}


vec_t
TriangleArea(const vec3_t v0, const vec3_t v1, const vec3_t v2)
{
    vec3_t edge0, edge1, cross;
    VectorSubtract(v2, v0, edge0);
    VectorSubtract(v1, v0, edge1);
    CrossProduct(edge0, edge1, cross);
    
    return VectorLength(cross) * 0.5;
}

static std::vector<float>
NormalizePDF(const std::vector<float> &pdf)
{
    float pdfSum = 0.0f;
    for (float val : pdf) {
        pdfSum += val;
    }
    
    std::vector<float> normalizedPdf;
    for (float val : pdf) {
        normalizedPdf.push_back(val / pdfSum);
    }
    return normalizedPdf;
}

std::vector<float> MakeCDF(const std::vector<float> &pdf)
{
    const std::vector<float> normzliedPdf = NormalizePDF(pdf);
    std::vector<float> cdf;
    float cdfSum = 0.0f;
    for (float val : normzliedPdf) {
        cdfSum += val;
        cdf.push_back(cdfSum);
    }
    return cdf;
}

int SampleCDF(const std::vector<float> &cdf, float sample)
{
    const size_t size = cdf.size();
    for (size_t i=0; i<size; i++) {
        float cdfVal = cdf.at(i);
        if (sample <= cdfVal) {
            return i;
        }
    }
    Q_assert_unreachable();
    return 0;
}

static float Gaussian1D(float width, float x, float alpha)
{
    if (fabs(x) > width)
        return 0.0f;
    
    return expf(-alpha * x * x) - expf(-alpha * width * width);
}

float Filter_Gaussian(float width, float height, float x, float y)
{
    const float alpha = 0.5;
    return Gaussian1D(width, x, alpha)
        * Gaussian1D(height, y, alpha);
}

// from https://en.wikipedia.org/wiki/Lanczos_resampling
static float Lanczos1D(float x, float a)
{
    if (x == 0)
        return 1;
    
    if (x < -a || x >= a)
        return 0;
    
    float lanczos = (a * sinf(Q_PI * x) * sinf(Q_PI * x / a)) / (Q_PI * Q_PI * x * x);
    return lanczos;
}

// from https://en.wikipedia.org/wiki/Lanczos_resampling#Multidimensional_interpolation
float Lanczos2D(float x, float y, float a)
{
    float dist = sqrtf((x*x) + (y*y));
    float lanczos = Lanczos1D(dist, a);
    return lanczos;
}

using namespace glm;
using namespace std;

glm::vec3 GLM_FaceNormal(std::vector<glm::vec3> points)
{
    const int N = static_cast<int>(points.size());
    float maxArea = -FLT_MAX;
    int bestI = -1;
    
    const vec3 p0 = points[0];
    
    for (int i=2; i<N; i++) {
        const vec3 p1 = points[i-1];
        const vec3 p2 = points[i];
        
        const float area = GLM_TriangleArea(p0, p1, p2);
        if (area > maxArea) {
            maxArea = area;
            bestI = i;
        }
    }
    
    if (bestI == -1 || maxArea < ZERO_TRI_AREA_EPSILON)
        return vec3(0);
    
    const vec3 p1 = points[bestI-1];
    const vec3 p2 = points[bestI];
    const vec3 normal = normalize(cross(p2 - p0, p1 - p0));
    return normal;
}

glm::vec4 GLM_PolyPlane(const std::vector<glm::vec3> &points)
{
    const vec3 normal = GLM_FaceNormal(points);
    const float dist = dot(points.at(0), normal);
    return vec4(normal, dist);
}

std::pair<bool, vec4>
GLM_MakeInwardFacingEdgePlane(const vec3 &v0, const vec3 &v1, const vec3 &faceNormal)
{
    const float v0v1len = length(v1-v0);
    if (v0v1len < POINT_EQUAL_EPSILON)
        return make_pair(false, vec4(0));
    
    const vec3 edgedir = (v1 - v0) / v0v1len;
    const vec3 edgeplane_normal = cross(edgedir, faceNormal);
    const float edgeplane_dist = dot(edgeplane_normal, v0);
    
    return make_pair(true, vec4(edgeplane_normal, edgeplane_dist));
}

vector<vec4>
GLM_MakeInwardFacingEdgePlanes(const std::vector<vec3> &points)
{
    const int N = points.size();
    if (N < 3)
        return {};
    
    vector<vec4> result;
    result.reserve(points.size());
    
    const vec3 faceNormal = GLM_FaceNormal(points);
    
    if (faceNormal == vec3(0,0,0))
        return {};
    
    for (int i=0; i<N; i++)
    {
        const vec3 v0 = points[i];
        const vec3 v1 = points[(i+1) % N];
        
        const auto edgeplane = GLM_MakeInwardFacingEdgePlane(v0, v1, faceNormal);
        if (!edgeplane.first)
            continue;
        
        result.push_back(edgeplane.second);
    }
    
    return result;
}

float GLM_EdgePlanes_PointInsideDist(const std::vector<glm::vec4> &edgeplanes, const glm::vec3 &point)
{
    float min = FLT_MAX;
    
    for (int i=0; i<edgeplanes.size(); i++) {
        const float planedist = GLM_DistAbovePlane(edgeplanes[i], point);
        if (planedist < min)
            min = planedist;
    }
    
    return min; // "outermost" point
}

bool
GLM_EdgePlanes_PointInside(const vector<vec4> &edgeplanes, const vec3 &point)
{
    if (edgeplanes.empty())
        return false;
    
    const float minDist = GLM_EdgePlanes_PointInsideDist(edgeplanes, point);
    return minDist >= -POINT_EQUAL_EPSILON;
}

vec3
GLM_TriangleCentroid(const vec3 &v0, const vec3 &v1, const vec3 &v2)
{
    return (v0 + v1 + v2) / 3.0f;
}

float
GLM_TriangleArea(const vec3 &v0, const vec3 &v1, const vec3 &v2)
{
    return 0.5f * length(cross(v2 - v0, v1 - v0));
}

float GLM_DistAbovePlane(const glm::vec4 &plane, const glm::vec3 &point)
{
    return dot(vec3(plane), point) - plane.w;
}

glm::vec3 GLM_ProjectPointOntoPlane(const glm::vec4 &plane, const glm::vec3 &point)
{
    float dist = GLM_DistAbovePlane(plane, point);
    vec3 move = -dist * vec3(plane);
    return point + move;
}

float GLM_PolyArea(const std::vector<glm::vec3> &points)
{
    Q_assert(points.size() >= 3);
    
    float poly_area = 0;
    
    const vec3 v0 = points.at(0);
    for (int i = 2; i < points.size(); i++) {
        const vec3 v1 = points.at(i-1);
        const vec3 v2 = points.at(i);
        
        const float triarea = GLM_TriangleArea(v0, v1, v2);
        
        poly_area += triarea;
    }
    
    return poly_area;
}

glm::vec3 GLM_PolyCentroid(const std::vector<glm::vec3> &points)
{
    Q_assert(points.size() >= 3);
    
    vec3 poly_centroid(0);
    float poly_area = 0;
    
    const vec3 v0 = points.at(0);
    for (int i = 2; i < points.size(); i++) {
        const vec3 v1 = points.at(i-1);
        const vec3 v2 = points.at(i);
        
        const float triarea = GLM_TriangleArea(v0, v1, v2);
        const vec3 tricentroid = GLM_TriangleCentroid(v0, v1, v2);
        
        poly_area += triarea;
        poly_centroid = poly_centroid + (triarea * tricentroid);
    }
    
    poly_centroid /= poly_area;
    
    return poly_centroid;
}

glm::vec3 GLM_PolyRandomPoint(const std::vector<glm::vec3> &points)
{
    Q_assert(points.size() >= 3);
    
    // FIXME: Precompute this
    float poly_area = 0;
    std::vector<float> triareas;
    
    const vec3 v0 = points.at(0);
    for (int i = 2; i < points.size(); i++) {
        const vec3 v1 = points.at(i-1);
        const vec3 v2 = points.at(i);
        
        const float triarea = GLM_TriangleArea(v0, v1, v2);
        Q_assert(triarea >= 0.0f);
        
        triareas.push_back(triarea);
        poly_area += triarea;
    }
    
    // Pick a random triangle, with probability proportional to triangle area
    const float uniformRandom = Random();
    const std::vector<float> cdf = MakeCDF(triareas);
    const int whichTri = SampleCDF(cdf, uniformRandom);
    
    Q_assert(whichTri >= 0 && whichTri < triareas.size());

    const tri_t tri { points.at(0), points.at(1 + whichTri), points.at(2 + whichTri) };
    
    // Pick random barycentric coords.
    const glm::vec3 bary = Barycentric_Random(Random(), Random());
    const glm::vec3 point = Barycentric_ToPoint(bary, tri);
    
    return point;
}

std::pair<int, glm::vec3> GLM_ClosestPointOnPolyBoundary(const std::vector<glm::vec3> &poly, const vec3 &point)
{
    const int N = static_cast<int>(poly.size());
    
    int bestI = -1;
    float bestDist = FLT_MAX;
    glm::vec3 bestPointOnPoly(0);
    
    for (int i=0; i<N; i++) {
        const glm::vec3 p0 = poly.at(i);
        const glm::vec3 p1 = poly.at((i + 1) % N);
        
        const glm::vec3 c = closestPointOnLine(point, p0, p1);
        const float distToC = length(c - point);
        
        if (distToC < bestDist) {
            bestI = i;
            bestDist = distToC;
            bestPointOnPoly = c;
        }
    }
    
    Q_assert(bestI != -1);
    
    return make_pair(bestI, bestPointOnPoly);
}

std::pair<bool, glm::vec3> GLM_InterpolateNormal(const std::vector<glm::vec3> &points,
                                                 const std::vector<glm::vec3> &normals,
                                                 const glm::vec3 &point)
{
    Q_assert(points.size() == normals.size());
    
    // Step through the triangles, being careful to handle zero-size ones

    const vec3 &p0 = points.at(0);
    const vec3 &n0 = normals.at(0);
    
    const int N = points.size();
    for (int i=2; i<N; i++) {
        const vec3 &p1 = points.at(i-1);
        const vec3 &n1 = normals.at(i-1);
        const vec3 &p2 = points.at(i);
        const vec3 &n2 = normals.at(i);
     
        const auto edgeplanes = GLM_MakeInwardFacingEdgePlanes({p0, p1, p2});
        if (edgeplanes.empty())
            continue;
        
        if (GLM_EdgePlanes_PointInside(edgeplanes, point)) {
            // Found the correct triangle
            
            const vec3 bary = Barycentric_FromPoint(point, make_tuple(p0, p1, p2));
            
            if (isnan(bary[0]) || isnan(bary[1]) || isnan(bary[2]))
                continue;

            const vec3 interpolatedNormal = Barycentric_ToPoint(bary, make_tuple(n0, n1, n2));
            return make_pair(true, interpolatedNormal);
        }
    }
    
    return make_pair(false, vec3(0));
}

static winding_t *glm_to_winding(const std::vector<glm::vec3> &poly)
{
    const int N = poly.size();
    winding_t *winding = AllocWinding(N);
    for (int i=0; i<N; i++) {
        glm_to_vec3_t(poly.at(i), winding->p[i]);
    }
    winding->numpoints = N;
    return winding;
}

static std::vector<glm::vec3> winding_to_glm(const winding_t *w)
{
    if (w == nullptr)
        return {};
    std::vector<glm::vec3> res;
    for (int i=0; i<w->numpoints; i++) {
        res.push_back(vec3_t_to_glm(w->p[i]));
    }
    return res;
}

/// Returns (front part, back part)
std::pair<std::vector<glm::vec3>,std::vector<glm::vec3>> GLM_ClipPoly(const std::vector<glm::vec3> &poly, const glm::vec4 &plane)
{
    vec3_t normal;
    winding_t *front = nullptr;
    winding_t *back = nullptr;
    
    if (poly.empty())
        return make_pair(vector<vec3>(),vector<vec3>());
    
    winding_t *w = glm_to_winding(poly);
    glm_to_vec3_t(vec3(plane), normal);
    ClipWinding(w, normal, plane.w, &front, &back);
    
    const auto res = make_pair(winding_to_glm(front), winding_to_glm(back));
    free(front);
    free(back);
    return res;
}

std::vector<glm::vec3> GLM_ShrinkPoly(const std::vector<glm::vec3> &poly, const float amount) {
    const vector<vec4> edgeplanes = GLM_MakeInwardFacingEdgePlanes(poly);
    
    vector<vec3> clipped = poly;
    
    for (const vec4 &edge : edgeplanes) {
        const vec4 shrunkEdgePlane(vec3(edge), edge.w + 1);
        clipped = GLM_ClipPoly(clipped, shrunkEdgePlane).first;
    }
    
    return clipped;
}

// from: http://stackoverflow.com/a/1501725
// see also: http://mathworld.wolfram.com/Projection.html
float FractionOfLine(const glm::vec3 &v, const glm::vec3 &w, const glm::vec3& p)
{
    const glm::vec3 vp = p - v;
    const glm::vec3 vw = w - v;
    
    const float l2 = glm::dot(vw, vw);
    if (l2 == 0) {
        return 0;
    }
    
    const float t = glm::dot(vp, vw) / l2;
    return t;
}

float DistToLine(const glm::vec3 &v, const glm::vec3 &w, const glm::vec3& p)
{
    const glm::vec3 closest = ClosestPointOnLine(v,w,p);
    return glm::distance(p, closest);
}

glm::vec3 ClosestPointOnLine(const glm::vec3 &v, const glm::vec3 &w, const glm::vec3& p)
{
    const glm::vec3 vp = p - v;
    const glm::vec3 vw_norm = glm::normalize(w - v);
    
    const float vp_scalarproj = glm::dot(vp, vw_norm);
    
    const glm::vec3 p_projected_on_vw = v + (vw_norm * vp_scalarproj);
    
    return p_projected_on_vw;
}

float DistToLineSegment(const glm::vec3 &v, const glm::vec3 &w, const glm::vec3& p)
{
    const glm::vec3 closest = ClosestPointOnLineSegment(v,w,p);
    return glm::distance(p, closest);
}

glm::vec3 ClosestPointOnLineSegment(const glm::vec3 &v, const glm::vec3 &w, const glm::vec3& p)
{
    const float frac = FractionOfLine(v, w, p);
    if (frac > 1)
        return w;
    if (frac < 0)
        return v;
    
    return ClosestPointOnLine(v, w, p);
}
