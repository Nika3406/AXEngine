#include "ecs/AnimationSystem.h"
#include <cmath>
#include <cstring>
#include <algorithm>

// ─────────────────────────────────────────────────────────────
//  Public entry point
// ─────────────────────────────────────────────────────────────
void AnimationSystem::update(SkinnedRenderComponent& comp, float dt) {
    comp.anim.clearRootMotionDelta();
    if (!comp.asset || comp.anim.clipIndex < 0 || !comp.anim.playing) return;
    const auto& clips = comp.asset->clips;
    if (comp.anim.clipIndex >= (int)clips.size()) return;

    const AnimationClip& clip = clips[comp.anim.clipIndex];
    comp.anim.time += dt * comp.anim.speed;

    if (comp.anim.loop) {
        if (clip.duration > 0.0f)
            comp.anim.time = std::fmod(comp.anim.time, clip.duration);
    } else {
        if (comp.anim.time >= clip.duration) {
            comp.anim.time    = clip.duration;
            comp.anim.playing = false;
            if (comp.anim.onClipEnd) comp.anim.onClipEnd(clip.name);
        }
    }

    if (comp.anim.blendFromClip >= 0) {
        comp.anim.blendElapsed += dt;
        if (comp.anim.blendElapsed >= comp.anim.blendTime)
            comp.anim.blendFromClip = -1;
    }

    computeJointMatrices(comp);
}

// ─────────────────────────────────────────────────────────────
//  Sample one channel at time t
// ─────────────────────────────────────────────────────────────
void AnimationSystem::sampleChannel(const AnimChannel& ch, float t, float* out) {
    const auto& times  = ch.times;
    const auto& values = ch.values;
    if (times.empty()) return;

    int comp = (ch.target == AnimTargetPath::Rotation) ? 4 : 3;

    if (t <= times.front()) { memcpy(out, values.data(), comp * sizeof(float)); return; }
    if (t >= times.back())  {
        memcpy(out, values.data() + ((int)times.size()-1)*comp, comp*sizeof(float));
        return;
    }

    int hi = (int)(std::lower_bound(times.begin(), times.end(), t) - times.begin());
    int lo = hi - 1;
    float alpha = (t - times[lo]) / (times[hi] - times[lo]);

    if (ch.interp == AnimInterpolation::Step) {
        memcpy(out, values.data() + lo*comp, comp*sizeof(float));
        return;
    }

    if (ch.interp == AnimInterpolation::CubicSpline) {
        int stride = 3*comp;
        const float* v0 = values.data() + lo*stride + comp;
        const float* b0 = values.data() + lo*stride + 2*comp;
        const float* a1 = values.data() + hi*stride;
        const float* v1 = values.data() + hi*stride + comp;
        float dt2 = times[hi]-times[lo];
        float a=alpha, a2=a*a, a3=a2*a;
        float h00=2*a3-3*a2+1, h10=a3-2*a2+a, h01=-2*a3+3*a2, h11=a3-a2;
        for (int i=0;i<comp;++i)
            out[i]=h00*v0[i]+h10*dt2*b0[i]+h01*v1[i]+h11*dt2*a1[i];
        if (ch.target==AnimTargetPath::Rotation) {
            float len=sqrtf(out[0]*out[0]+out[1]*out[1]+out[2]*out[2]+out[3]*out[3]);
            if (len>1e-6f){out[0]/=len;out[1]/=len;out[2]/=len;out[3]/=len;}
        }
        return;
    }

    // Linear
    if (ch.target == AnimTargetPath::Rotation)
        quatSlerp(values.data()+lo*4, values.data()+hi*4, alpha, out);
    else
        vec3Lerp(values.data()+lo*3, values.data()+hi*3, alpha, out);
}

// ─────────────────────────────────────────────────────────────
//  Build joint matrix palette
// ─────────────────────────────────────────────────────────────
void AnimationSystem::computeJointMatrices(SkinnedRenderComponent& comp) {
    const Skeleton& skel = comp.asset->skeleton;
    int jc = (int)skel.joints.size();
    if (jc == 0) return;

    comp.anim.jointMatrices.resize(jc * 16);

    struct LP { float t[3]; float r[4]; float s[3]; };
    std::vector<LP> pose(jc), fromPose(jc);

    bool blending = (comp.anim.blendFromClip >= 0 &&
                     comp.anim.blendFromClip < (int)comp.asset->clips.size());

    for (int j=0;j<jc;++j) {
        const Joint& jnt=skel.joints[j];
        memcpy(pose[j].t, jnt.localTranslation, 12);
        memcpy(pose[j].r, jnt.localRotation,    16);
        memcpy(pose[j].s, jnt.localScale,        12);
        if (blending) { memcpy(fromPose[j].t, jnt.localTranslation, 12);
                        memcpy(fromPose[j].r, jnt.localRotation,    16);
                        memcpy(fromPose[j].s, jnt.localScale,        12); }
    }

    const AnimationClip& clip = comp.asset->clips[comp.anim.clipIndex];
    for (const auto& ch : clip.channels) {
        if (ch.jointIndex<0||ch.jointIndex>=jc) continue;
        LP& lp=pose[ch.jointIndex];
        switch(ch.target){
            case AnimTargetPath::Translation: sampleChannel(ch,comp.anim.time,lp.t); break;
            case AnimTargetPath::Rotation:    sampleChannel(ch,comp.anim.time,lp.r); break;
            case AnimTargetPath::Scale:       sampleChannel(ch,comp.anim.time,lp.s); break;
        }
    }

    if (blending) {
        const AnimationClip& from=comp.asset->clips[comp.anim.blendFromClip];
        float ft=comp.anim.blendFromTime+comp.anim.blendElapsed*comp.anim.speed;
        if (from.duration>0) ft=fmodf(ft,from.duration);
        for (const auto& ch : from.channels) {
            if (ch.jointIndex<0||ch.jointIndex>=jc) continue;
            LP& lp=fromPose[ch.jointIndex];
            switch(ch.target){
                case AnimTargetPath::Translation: sampleChannel(ch,ft,lp.t); break;
                case AnimTargetPath::Rotation:    sampleChannel(ch,ft,lp.r); break;
                case AnimTargetPath::Scale:       sampleChannel(ch,ft,lp.s); break;
            }
        }
        float alpha=std::min(comp.anim.blendElapsed/std::max(comp.anim.blendTime,1e-5f),1.0f);
        for (int j=0;j<jc;++j){
            vec3Lerp(fromPose[j].t, pose[j].t, alpha, pose[j].t);
            quatSlerp(fromPose[j].r, pose[j].r, alpha, pose[j].r);
            vec3Lerp(fromPose[j].s, pose[j].s, alpha, pose[j].s);
        }
    }

    // ── Root motion extraction ───────────────────────────────
    // Extract local-space X/Z movement from the selected root-motion joint.
    // Then remove X/Z from that joint's sampled pose so the mesh does not
    // visually run forward and snap back when the clip ends. Demo.cpp applies
    // comp.anim.rootMotionDelta to the GameObject/player transform.
    if (comp.anim.rootMotionEnabled &&
        comp.anim.rootMotionJointIndex >= 0 &&
        comp.anim.rootMotionJointIndex < jc) {

        int rj = comp.anim.rootMotionJointIndex;
        float current[3] = { pose[rj].t[0], pose[rj].t[1], pose[rj].t[2] };

        if (comp.anim.rootMotionHasPrevSample) {
            comp.anim.rootMotionDelta[0] = current[0] - comp.anim.rootMotionPrevLocal[0];
            comp.anim.rootMotionDelta[1] = current[1] - comp.anim.rootMotionPrevLocal[1];
            comp.anim.rootMotionDelta[2] = current[2] - comp.anim.rootMotionPrevLocal[2];
        } else {
            comp.anim.rootMotionDelta[0] = 0.0f;
            comp.anim.rootMotionDelta[1] = 0.0f;
            comp.anim.rootMotionDelta[2] = 0.0f;
            comp.anim.rootMotionHasPrevSample = true;
        }

        comp.anim.rootMotionPrevLocal[0] = current[0];
        comp.anim.rootMotionPrevLocal[1] = current[1];
        comp.anim.rootMotionPrevLocal[2] = current[2];

        if (comp.anim.rootMotionRemoveXZFromPose) {
            // Preserve the bind/rest offset, but remove animated X/Z travel.
            pose[rj].t[0] = skel.joints[rj].localTranslation[0];
            pose[rj].t[2] = skel.joints[rj].localTranslation[2];
            // Keep Y so jumps/kicks/body height still animate visually.
        }
    }

    std::vector<float> global(jc*16);
    for (int j=0;j<jc;++j){
        float local[16];
        mat4FromTRS(pose[j].t, pose[j].r, pose[j].s, local);
        if (skel.joints[j].parentIndex<0)
            memcpy(&global[j*16], local, 64);
        else
            mat4Mul(&global[skel.joints[j].parentIndex*16], local, &global[j*16]);
    }

    comp.anim.animatedGlobalMatrices.resize(jc * 16);
    memcpy(comp.anim.animatedGlobalMatrices.data(), global.data(), jc * 16 * sizeof(float));

    for (int j=0;j<jc;++j) {
        float skin[16];
        mat4Mul(&global[j*16], skel.joints[j].inverseBindMatrix, skin);
        memcpy(&comp.anim.jointMatrices[j*16], skin, 64);
    }
}

// ─────────────────────────────────────────────────────────────
//  Math
// ─────────────────────────────────────────────────────────────
void AnimationSystem::mat4Identity(float* m){memset(m,0,64);m[0]=m[5]=m[10]=m[15]=1;}

void AnimationSystem::mat4Mul(const float* a, const float* b, float* o){
    float t[16]={};
    for(int c=0;c<4;++c)for(int r=0;r<4;++r)for(int k=0;k<4;++k)
        t[c*4+r]+=a[k*4+r]*b[c*4+k];
    memcpy(o,t,64);
}

void AnimationSystem::mat4FromTRS(const float* t,const float* r,const float* s,float* m){
    float x=r[0],y=r[1],z=r[2],w=r[3];
    float x2=x+x,y2=y+y,z2=z+z;
    float xx=x*x2,xy=x*y2,xz=x*z2,yy=y*y2,yz=y*z2,zz=z*z2,wx=w*x2,wy=w*y2,wz=w*z2;
    m[ 0]=(1-(yy+zz))*s[0]; m[ 1]=(xy+wz)*s[0]; m[ 2]=(xz-wy)*s[0]; m[ 3]=0;
    m[ 4]=(xy-wz)*s[1]; m[ 5]=(1-(xx+zz))*s[1]; m[ 6]=(yz+wx)*s[1]; m[ 7]=0;
    m[ 8]=(xz+wy)*s[2]; m[ 9]=(yz-wx)*s[2]; m[10]=(1-(xx+yy))*s[2]; m[11]=0;
    m[12]=t[0]; m[13]=t[1]; m[14]=t[2]; m[15]=1;
}

void AnimationSystem::quatSlerp(const float* a,const float* b,float al,float* o){
    float bx=b[0],by=b[1],bz=b[2],bw=b[3];
    float dot=a[0]*bx+a[1]*by+a[2]*bz+a[3]*bw;
    if(dot<0){bx=-bx;by=-by;bz=-bz;bw=-bw;dot=-dot;}
    if(dot>0.9995f){
        o[0]=a[0]+al*(bx-a[0]);o[1]=a[1]+al*(by-a[1]);
        o[2]=a[2]+al*(bz-a[2]);o[3]=a[3]+al*(bw-a[3]);
        float l=sqrtf(o[0]*o[0]+o[1]*o[1]+o[2]*o[2]+o[3]*o[3]);
        if(l>1e-6f){o[0]/=l;o[1]/=l;o[2]/=l;o[3]/=l;}
        return;
    }
    float th0=acosf(dot),th=th0*al,s0=sinf(th0);
    float fa=cosf(th)-dot*sinf(th)/s0,fb=sinf(th)/s0;
    o[0]=fa*a[0]+fb*bx;o[1]=fa*a[1]+fb*by;o[2]=fa*a[2]+fb*bz;o[3]=fa*a[3]+fb*bw;
}

void AnimationSystem::vec3Lerp(const float* a,const float* b,float al,float* o){
    o[0]=a[0]+al*(b[0]-a[0]);o[1]=a[1]+al*(b[1]-a[1]);o[2]=a[2]+al*(b[2]-a[2]);
}