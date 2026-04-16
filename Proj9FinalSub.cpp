// Proj9FinalSub.cpp
// Project 9 — Advanced Shader Scene
// Compatible with: g++ Proj9FinalSub.cpp -o run -lglfw -lGL -lGLEW -lSOIL -lassimp
//
// Authors: Jared Walker & James Hohn
// Course:  CST-310 — Computer Graphics
// School:  Grand Canyon University
//
// Controls:
//   W / S           — move up / down
//   A / D           — strafe left / right
//   Q / E           — roll camera
//   Arrow L/R       — yaw
//   Arrow U/D       — pitch
//   Shift+Up/Down   — move forward / backward
//   Ctrl+Arrows     — fine yaw / pitch
//   R               — reset camera
//   ESC             — quit

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>

// ─── Constants ────────────────────────────────────────────────────────────────
const float PI = 3.14159265f;
int winW = 900, winH = 600;

// ─── Shader source ────────────────────────────────────────────────────────────
const char* vertSrc = R"GLSL(
#version 120
attribute vec3 aPos;
attribute vec3 aNormal;
varying vec3 fragPos;
varying vec3 fragNormal;
uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
void main() {
    vec4 world  = model * vec4(aPos, 1.0);
    fragPos     = world.xyz;
    fragNormal  = mat3(model) * aNormal;
    gl_Position = proj * view * world;
}
)GLSL";

const char* fragSolidSrc = R"GLSL(
#version 120
varying vec3 fragPos;
varying vec3 fragNormal;
uniform vec3  objectColor;
uniform vec3  lightPos;
uniform vec3  lightColor;
uniform vec3  viewPos;
uniform float shininess;
void main() {
    vec3  norm     = normalize(fragNormal);
    vec3  lightDir = normalize(lightPos - fragPos);
    vec3  viewDir  = normalize(viewPos  - fragPos);
    vec3  halfDir  = normalize(lightDir + viewDir);
    vec3  ambient  = 0.2  * lightColor;
    float diff     = max(dot(norm, lightDir), 0.0);
    vec3  diffuse  = diff * lightColor;
    float spec     = pow(max(dot(norm, halfDir), 0.0), shininess);
    vec3  specular = 0.6  * spec * lightColor;
    gl_FragColor = vec4((ambient + diffuse + specular) * objectColor, 1.0);
}
)GLSL";

const char* fragBoardSrc = R"GLSL(
#version 120
varying vec3 fragPos;
varying vec3 fragNormal;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
void main() {
    float ix = floor(fragPos.x);
    float iz = floor(fragPos.z);
    float chk = mod(ix + iz, 2.0);
    vec3 color = (chk == 0.0) ? vec3(1,0,0) : vec3(1,1,1);
    vec3  norm     = normalize(fragNormal);
    vec3  lightDir = normalize(lightPos - fragPos);
    vec3  viewDir  = normalize(viewPos  - fragPos);
    vec3  halfDir  = normalize(lightDir + viewDir);
    float diff     = max(dot(norm, lightDir), 0.0);
    float spec     = pow(max(dot(norm, halfDir), 0.0), 16.0);
    vec3  result   = (0.28 + diff + 0.15 * spec) * color * lightColor;
    gl_FragColor = vec4(result, 1.0);
}
)GLSL";

// ─── Shader helpers ───────────────────────────────────────────────────────────
GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
        fprintf(stderr, "Shader error: %s\n", log);
    }
    return s;
}

GLuint buildProgram(const char* vs, const char* fs) {
    GLuint v = compileShader(GL_VERTEX_SHADER,   vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glBindAttribLocation(p, 0, "aPos");
    glBindAttribLocation(p, 1, "aNormal");
    glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

GLuint solidProg, boardProg;

// ─── Matrix helpers (no GLU) ──────────────────────────────────────────────────
glm::mat4 myPerspective(float fovDeg, float aspect, float zn, float zf) {
    float f = 1.0f / tanf(fovDeg * PI / 360.0f);
    glm::mat4 m(0.0f);
    m[0][0] = f / aspect;
    m[1][1] = f;
    m[2][2] = (zf + zn) / (zn - zf);
    m[2][3] = -1.0f;
    m[3][2] = (2.0f * zf * zn) / (zn - zf);
    return m;
}

glm::mat4 myLookAt(glm::vec3 eye, glm::vec3 target, glm::vec3 up) {
    glm::vec3 f = glm::normalize(target - eye);
    glm::vec3 r = glm::normalize(glm::cross(f, glm::normalize(up)));
    glm::vec3 u = glm::cross(r, f);
    glm::mat4 m(1.0f);
    m[0][0]=r.x; m[1][0]=r.y; m[2][0]=r.z;
    m[0][1]=u.x; m[1][1]=u.y; m[2][1]=u.z;
    m[0][2]=-f.x;m[1][2]=-f.y;m[2][2]=-f.z;
    m[3][0]=-glm::dot(r,eye);
    m[3][1]=-glm::dot(u,eye);
    m[3][2]= glm::dot(f,eye);
    return m;
}

// ─── Mesh ─────────────────────────────────────────────────────────────────────
struct Mesh {
    GLuint vao=0, vbo=0, ebo=0;
    int    indexCount=0;

    void upload(const std::vector<float>& verts,
                const std::vector<unsigned int>& idx) {
        indexCount = (int)idx.size();
        glGenVertexArrays(1,&vao);
        glGenBuffers(1,&vbo);
        glGenBuffers(1,&ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER,verts.size()*4,verts.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,idx.size()*4,idx.data(),GL_STATIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,(void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,24,(void*)12);
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }
    void draw() {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES,indexCount,GL_UNSIGNED_INT,0);
        glBindVertexArray(0);
    }
};

Mesh buildSphere(int stacks, int slices) {
    std::vector<float> v;
    std::vector<unsigned int> idx;
    for (int i=0;i<=stacks;i++) {
        float phi=PI*i/stacks;
        for (int j=0;j<=slices;j++) {
            float theta=2*PI*j/slices;
            float x=sinf(phi)*cosf(theta), y=cosf(phi), z=sinf(phi)*sinf(theta);
            v.insert(v.end(),{x,y,z,x,y,z});
        }
    }
    for (int i=0;i<stacks;i++)
        for (int j=0;j<slices;j++) {
            unsigned int tl=i*(slices+1)+j, tr=tl+1, bl=tl+(slices+1), br=bl+1;
            idx.insert(idx.end(),{tl,bl,tr,tr,bl,br});
        }
    Mesh m; m.upload(v,idx); return m;
}

Mesh buildCylinder(int slices, float h) {
    std::vector<float> v;
    std::vector<unsigned int> idx;
    float hh=h/2.0f;
    for (int j=0;j<=slices;j++) {
        float theta=2*PI*j/slices, cx=cosf(theta), cz=sinf(theta);
        v.insert(v.end(),{cx,-hh,cz, cx,0,cz});
        v.insert(v.end(),{cx, hh,cz, cx,0,cz});
    }
    for (int j=0;j<slices;j++) {
        unsigned int b0=j*2,t0=b0+1,b1=b0+2,t1=b0+3;
        idx.insert(idx.end(),{b0,b1,t0,t0,b1,t1});
    }
    unsigned int cBot=(unsigned int)(v.size()/6);
    v.insert(v.end(),{0,-hh,0, 0,-1,0});
    for (int j=0;j<=slices;j++) {
        float theta=2*PI*j/slices;
        v.insert(v.end(),{cosf(theta),-hh,sinf(theta), 0,-1,0});
    }
    for (int j=0;j<slices;j++) idx.insert(idx.end(),{cBot,cBot+j+2,cBot+j+1});
    unsigned int cTop=(unsigned int)(v.size()/6);
    v.insert(v.end(),{0,hh,0, 0,1,0});
    for (int j=0;j<=slices;j++) {
        float theta=2*PI*j/slices;
        v.insert(v.end(),{cosf(theta),hh,sinf(theta), 0,1,0});
    }
    for (int j=0;j<slices;j++) idx.insert(idx.end(),{cTop,cTop+j+1,cTop+j+2});
    Mesh m; m.upload(v,idx); return m;
}

Mesh buildCube() {
    std::vector<float> v = {
        -1,-1, 1, 0,0,1,  1,-1, 1, 0,0,1,  1, 1, 1, 0,0,1, -1, 1, 1, 0,0,1,
         1,-1,-1, 0,0,-1,-1,-1,-1, 0,0,-1,-1, 1,-1, 0,0,-1,  1, 1,-1, 0,0,-1,
        -1,-1,-1,-1,0,0, -1,-1, 1,-1,0,0, -1, 1, 1,-1,0,0, -1, 1,-1,-1,0,0,
         1,-1, 1, 1,0,0,  1,-1,-1, 1,0,0,  1, 1,-1, 1,0,0,  1, 1, 1, 1,0,0,
        -1, 1, 1, 0,1,0,  1, 1, 1, 0,1,0,  1, 1,-1, 0,1,0, -1, 1,-1, 0,1,0,
        -1,-1,-1, 0,-1,0, 1,-1,-1, 0,-1,0, 1,-1, 1, 0,-1,0,-1,-1, 1, 0,-1,0,
    };
    std::vector<unsigned int> idx;
    for (int f=0;f<6;f++) { unsigned int b=f*4; idx.insert(idx.end(),{b,b+1,b+2,b,b+2,b+3}); }
    Mesh m; m.upload(v,idx); return m;
}

Mesh buildBoard() {
    std::vector<float> v = {
        -4,0,-4, 0,1,0,  4,0,-4, 0,1,0,  4,0,4, 0,1,0,
        -4,0,-4, 0,1,0,  4,0, 4, 0,1,0, -4,0,4, 0,1,0,
    };
    GLuint vao,vbo;
    glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,v.size()*4,v.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,(void*)0);  glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,24,(void*)12); glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    Mesh m; m.vao=vao; m.vbo=vbo; m.indexCount=6; return m;
}

// ─── Camera ───────────────────────────────────────────────────────────────────
struct Camera {
    float x=0,y=2,z=10,yaw=-90,pitch=-10,roll=0;

    glm::vec3 forward() {
        float yr=yaw*PI/180, pr=pitch*PI/180;
        return glm::normalize(glm::vec3(cosf(pr)*cosf(yr), sinf(pr), cosf(pr)*sinf(yr)));
    }
    void moveRight(float d) {
        glm::vec3 f=forward();
        x+=f.z*d; z-=f.x*d;
    }
    glm::mat4 viewMatrix() {
        glm::vec3 f=forward();
        float rr=roll*PI/180;
        glm::vec3 up(-sinf(rr), cosf(rr), 0);
        return myLookAt(glm::vec3(x,y,z), glm::vec3(x,y,z)+f, up);
    }
    void reset(){ x=0;y=2;z=10;yaw=-90;pitch=-10;roll=0; }
} cam;

// ─── Uniform helpers ──────────────────────────────────────────────────────────
void setUniforms(GLuint prog, glm::mat4 model, glm::mat4 view, glm::mat4 proj,
                 float r,float g,float b,
                 float lx,float ly,float lz,
                 float cx,float cy,float cz, float shine) {
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"model"),1,GL_FALSE,glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(prog,"view"), 1,GL_FALSE,glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(prog,"proj"), 1,GL_FALSE,glm::value_ptr(proj));
    glUniform3f(glGetUniformLocation(prog,"objectColor"),r,g,b);
    glUniform3f(glGetUniformLocation(prog,"lightPos"),lx,ly,lz);
    glUniform3f(glGetUniformLocation(prog,"lightColor"),1,1,1);
    glUniform3f(glGetUniformLocation(prog,"viewPos"),cx,cy,cz);
    glUniform1f(glGetUniformLocation(prog,"shininess"),shine);
}

void setBoardUniforms(GLuint prog, glm::mat4 model, glm::mat4 view, glm::mat4 proj,
                      float lx,float ly,float lz, float cx,float cy,float cz) {
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"model"),1,GL_FALSE,glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(prog,"view"), 1,GL_FALSE,glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(prog,"proj"), 1,GL_FALSE,glm::value_ptr(proj));
    glUniform3f(glGetUniformLocation(prog,"lightPos"),lx,ly,lz);
    glUniform3f(glGetUniformLocation(prog,"lightColor"),1,1,1);
    glUniform3f(glGetUniformLocation(prog,"viewPos"),cx,cy,cz);
}

// ─── Meshes ───────────────────────────────────────────────────────────────────
Mesh sphereMesh, cylinderMesh, cubeMesh, boardMesh;

// ─── Input ────────────────────────────────────────────────────────────────────
void processInput(GLFWwindow* win, float dt) {
    float mv=3.0f*dt, rt=60.0f*dt, fn=30.0f*dt;
    bool sh=(glfwGetKey(win,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS||
             glfwGetKey(win,GLFW_KEY_RIGHT_SHIFT)==GLFW_PRESS);
    bool ct=(glfwGetKey(win,GLFW_KEY_LEFT_CONTROL)==GLFW_PRESS||
             glfwGetKey(win,GLFW_KEY_RIGHT_CONTROL)==GLFW_PRESS);

    if(glfwGetKey(win,GLFW_KEY_ESCAPE)==GLFW_PRESS) glfwSetWindowShouldClose(win,true);
    if(glfwGetKey(win,GLFW_KEY_W)==GLFW_PRESS) cam.y+=mv;
    if(glfwGetKey(win,GLFW_KEY_S)==GLFW_PRESS) cam.y-=mv;
    if(glfwGetKey(win,GLFW_KEY_A)==GLFW_PRESS) cam.moveRight(-mv);
    if(glfwGetKey(win,GLFW_KEY_D)==GLFW_PRESS) cam.moveRight( mv);
    if(glfwGetKey(win,GLFW_KEY_Q)==GLFW_PRESS) cam.roll-=rt;
    if(glfwGetKey(win,GLFW_KEY_E)==GLFW_PRESS) cam.roll+=rt;
    if(glfwGetKey(win,GLFW_KEY_R)==GLFW_PRESS) cam.reset();

    if(sh){
        if(glfwGetKey(win,GLFW_KEY_UP)==GLFW_PRESS)   cam.z-=mv;
        if(glfwGetKey(win,GLFW_KEY_DOWN)==GLFW_PRESS) cam.z+=mv;
    } else if(ct){
        if(glfwGetKey(win,GLFW_KEY_LEFT)==GLFW_PRESS)  cam.yaw-=fn;
        if(glfwGetKey(win,GLFW_KEY_RIGHT)==GLFW_PRESS) cam.yaw+=fn;
        if(glfwGetKey(win,GLFW_KEY_UP)==GLFW_PRESS)  {cam.pitch+=fn;if(cam.pitch>89)cam.pitch=89;}
        if(glfwGetKey(win,GLFW_KEY_DOWN)==GLFW_PRESS){cam.pitch-=fn;if(cam.pitch<-89)cam.pitch=-89;}
    } else {
        if(glfwGetKey(win,GLFW_KEY_LEFT)==GLFW_PRESS)  cam.yaw-=rt;
        if(glfwGetKey(win,GLFW_KEY_RIGHT)==GLFW_PRESS) cam.yaw+=rt;
        if(glfwGetKey(win,GLFW_KEY_UP)==GLFW_PRESS)  {cam.pitch+=rt;if(cam.pitch>89)cam.pitch=89;}
        if(glfwGetKey(win,GLFW_KEY_DOWN)==GLFW_PRESS){cam.pitch-=rt;if(cam.pitch<-89)cam.pitch=-89;}
    }
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    if(!glfwInit()){fprintf(stderr,"GLFW failed\n");return -1;}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
    GLFWwindow* win=glfwCreateWindow(winW,winH,"Project 9 — Shader Scene",NULL,NULL);
    if(!win){fprintf(stderr,"Window failed\n");glfwTerminate();return -1;}
    glfwMakeContextCurrent(win);
    glewExperimental=GL_TRUE; glewInit();
    glEnable(GL_DEPTH_TEST);

    solidProg = buildProgram(vertSrc, fragSolidSrc);
    boardProg = buildProgram(vertSrc, fragBoardSrc);

    sphereMesh   = buildSphere(32,32);
    cylinderMesh = buildCylinder(32,2.0f);
    cubeMesh     = buildCube();
    boardMesh    = buildBoard();

    float lastTime=0;
    while(!glfwWindowShouldClose(win)){
        float now=(float)glfwGetTime(), dt=now-lastTime; lastTime=now;
        processInput(win,dt);

        int w,h; glfwGetFramebufferSize(win,&w,&h);
        glViewport(0,0,w,h);
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = myPerspective(60.0f,(float)w/h,0.1f,200.0f);
        glm::mat4 view = cam.viewMatrix();
        float lx=6,ly=8,lz=6;
        float cx=cam.x,cy=cam.y,cz=cam.z;

        // Board
        glm::mat4 bm = glm::mat4(1.0f);
        setBoardUniforms(boardProg,bm,view,proj,lx,ly,lz,cx,cy,cz);
        glBindVertexArray(boardMesh.vao);
        glDrawArrays(GL_TRIANGLES,0,6);
        glBindVertexArray(0);

        // Sphere — LEFT
        glm::mat4 sm = glm::translate(glm::mat4(1.0f),glm::vec3(-2.5f,1.2f,0));
        sm = glm::scale(sm,glm::vec3(1.2f));
        setUniforms(solidProg,sm,view,proj, 0,0.8f,0.1f, lx,ly,lz,cx,cy,cz,96);
        sphereMesh.draw();

        // Cube — CENTER
        glm::mat4 cm = glm::translate(glm::mat4(1.0f),glm::vec3(0,1.0f,0));
        setUniforms(solidProg,cm,view,proj, 0.1f,0.15f,0.85f, lx,ly,lz,cx,cy,cz,48);
        cubeMesh.draw();

        // Cylinder — RIGHT
        glm::mat4 cym = glm::translate(glm::mat4(1.0f),glm::vec3(2.5f,1.0f,0));
        cym = glm::scale(cym, glm::vec3(1.0f, 1.5f, 1.0f));
        setUniforms(solidProg,cym,view,proj, 0.55f,0.5f,0.05f, lx,ly,lz,cx,cy,cz,32);
        cylinderMesh.draw();

        glfwSwapBuffers(win);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
