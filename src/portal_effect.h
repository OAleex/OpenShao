#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
namespace openshao {
namespace portal_detail {
using Vec = std::array<double,3>;
inline Vec Sub(Vec a,Vec b) {
    for(unsigned j=0;j<3;++j) a[j]-=b[j];
    return a;
}
inline double Dot(Vec a,Vec b) {
    double sum=0;
    for(unsigned j=0;j<3;++j) sum+=a[j]*b[j];
    return sum;
}
inline Vec Cross(Vec a,Vec b) {
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}
inline bool Unit(Vec& a) {
    const double length=std::sqrt(Dot(a,a));
    if(!std::isfinite(length)||length<1e-9) return false;
    for(auto& value:a) value/=length;
    return true;
}
}

// Rotate the whole ribbon about its end-center line, keeping taper, UVs and color.
inline bool FacePortalQuad(std::span<uint8_t> bytes,std::array<float,3> camera) {
    using namespace portal_detail;
    if(bytes.size()!=80) return false;
    for(float value:camera) if(!std::isfinite(value)) return false;
    std::array<Vec,4> points;
    for(unsigned i=0;i<4;++i) {
        std::array<float,3> input;
        std::memcpy(input.data(),bytes.data()+i*20,12);
        for(unsigned j=0;j<3;++j) {
            if(!std::isfinite(input[j])) return false;
            points[i][j]=input[j];
        }
    }
    Vec first{},last{},center{};
    for(unsigned j=0;j<3;++j) {
        first[j]=(points[0][j]+points[1][j])/2;
        last[j]=(points[2][j]+points[3][j])/2;
        center[j]=(first[j]+last[j])/2;
    }
    Vec axis=Sub(last,first);
    if(!Unit(axis)) return false;
    Vec normal=Cross(Sub(points[1],points[0]),Sub(points[3],points[0]));
    Vec view=Sub({camera[0],camera[1],camera[2]},center);
    const double normal_axis=Dot(normal,axis),view_axis=Dot(view,axis);
    for(unsigned j=0;j<3;++j) {
        normal[j]-=normal_axis*axis[j];
        view[j]-=view_axis*axis[j];
    }
    if(!Unit(normal)||!Unit(view)) return false;
    if(Dot(normal,view)<0) for(auto& value:view) value=-value;
    const double cosine=std::clamp(Dot(normal,view),-1.0,1.0);
    if(cosine>1-1e-12) return false;
    const double sine=Dot(axis,Cross(normal,view));
    std::array<std::array<float,3>,4> output;
    for(unsigned i=0;i<4;++i) {
        auto relative=Sub(points[i],center),tangent=Cross(axis,relative);
        const double along=Dot(axis,relative);
        for(unsigned j=0;j<3;++j) {
            const double value=center[j]+relative[j]*cosine+tangent[j]*sine+
                               axis[j]*along*(1-cosine);
            if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max()) return false;
            output[i][j]=float(value);
        }
    }
    for(unsigned i=0;i<4;++i) std::memcpy(bytes.data()+i*20,output[i].data(),12);
    return true;
}

// Undo the sprite producer's extra camera rotation about the particle origin.
inline bool RestorePortalWorldAxes(std::span<uint8_t> bytes,const std::array<float,16>& matrix) {
    if(bytes.size()!=80) return false;
    for(float value:matrix) if(!std::isfinite(value)) return false;
    for(unsigned i=0;i<3;++i) for(unsigned k=0;k<3;++k) {
        double dot=0;
        for(unsigned j=0;j<3;++j) dot+=double(matrix[i*4+j])*matrix[k*4+j];
        if(std::abs(dot-(i==k?1.0:0.0))>.0001) return false;
    }
    std::array<std::array<float,3>,4> output;
    for(unsigned i=0;i<4;++i) {
        std::array<float,3> point;
        std::memcpy(point.data(),bytes.data()+i*20,12);
        for(float value:point) if(!std::isfinite(value)) return false;
        for(unsigned j=0;j<3;++j) {
            double value=matrix[12+j];
            for(unsigned k=0;k<3;++k) value+=(double(point[k])-matrix[12+k])*matrix[j*4+k];
            output[i][j]=float(value);
            if(!std::isfinite(output[i][j])) return false;
        }
    }
    for(unsigned i=0;i<4;++i) std::memcpy(bytes.data()+i*20,output[i].data(),12);
    return true;
}
}
