// project9.cpp
//
// Authors: Jared Walker & James Hohn
// Course:  CST-310 — Computer Graphics
// School:  Grand Canyon University
//
// Compile:
//   g++ project9.cpp -o project9 -lGL -lGLU -lGLEW -lglut -std=c++17
// Run:
//   ./project9
//
// Controls:
//   W / S         — move forward / backward
//   A / D         — strafe left / right
//   Q / E         - roll left / right
//   Arrow Up/Down — pitch camera up / down
//   Arrow L/R     — rotate camera left / right
//   ESC           — quit

#ifdef __APPLE_CC__
#include <GLUT/glut.h>
#else
#include <GL/glew.h>
#include <GL/glut.h>
#endif

#include <cmath>
#include <vector>
#include <iostream>

const float PI = 3.14159265f;

// ─── Vertex shader ────────────────────────────────────────────────────────────
// Transforms each vertex through MVP matrices. Passes world-space
// position and transformed normal to the fragment shader.
const char* vertSrc = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

out vec3 fragPos;
out vec3 fragNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

void main() {
    vec4 world  = model * vec4(aPos, 1.0);
    fragPos     = world.xyz;
    fragNormal  = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = proj * view * world;
}
)GLSL";

// ─── Ball / solid object fragment shader ─────────────────────────────────────
// Per-fragment Blinn-Phong: ambient + diffuse (N dot L) + specular (N dot H).
const char* fragSolidSrc = R"GLSL(
#version 330 core
in vec3 fragPos;
in vec3 fragNormal;
out vec4 FragColor;

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

    FragColor = vec4((ambient + diffuse + specular) * objectColor, 1.0);
}
)GLSL";

// ─── Checkerboard fragment shader ────────────────────────────────────────────
// Generates the red/white pattern procedurally from world XZ position.
// No texture file needed — pure math in the shader.
const char* fragBoardSrc = R"GLSL(
#version 330 core
in vec3 fragPos;
in vec3 fragNormal;
out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

void main() {
    // Procedural checkerboard from integer floor of world X and Z
    int  ix    = int(floor(fragPos.x));
    int  iz    = int(floor(fragPos.z));
    vec3 color = ((ix + iz) % 2 == 0) ? vec3(1,0,0) : vec3(1,1,1);

    vec3  norm     = normalize(fragNormal);
    vec3  lightDir = normalize(lightPos - fragPos);
    vec3  viewDir  = normalize(viewPos  - fragPos);
    vec3  halfDir  = normalize(lightDir + viewDir);

    float diff   = max(dot(norm, lightDir), 0.0);
    float spec   = pow(max(dot(norm, halfDir), 0.0), 16.0);
    vec3  result = (0.28 + diff + 0.15 * spec) * color * lightColor;

    FragColor = vec4(result, 1.0);
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
        std::cerr << "Shader error: " << log << "\n";
    }
    return s;
}

GLuint buildProgram(const char* vs, const char* fs) {
    GLuint v = compileShader(GL_VERTEX_SHADER,   vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

GLuint solidProg, boardProg;

// ─── Matrix math ──────────────────────────────────────────────────────────────
void buildPerspective(float fov, float aspect, float zn, float zf, float* m) {
    float f = 1.0f / tanf(fov * PI / 360.0f);
    m[0]=f/aspect;m[1]=0;m[2]=0;m[3]=0;
    m[4]=0;m[5]=f;m[6]=0;m[7]=0;
    m[8]=0;m[9]=0;m[10]=(zf+zn)/(zn-zf);m[11]=-1;
    m[12]=0;m[13]=0;m[14]=(2*zf*zn)/(zn-zf);m[15]=0;
}

// Produces a view matrix from eye position, forward, and up vectors
void buildView(float ex,float ey,float ez,
               float fx,float fy,float fz,
               float ux,float uy,float uz, float* m) {
    // right = forward x up
    float rx=fy*uz-fz*uy, ry=fz*ux-fx*uz, rz=fx*uy-fy*ux;
    float rl=sqrtf(rx*rx+ry*ry+rz*rz);
    rx/=rl;ry/=rl;rz/=rl;
    // recompute up = right x forward
    float ux2=ry*fz-rz*fy, uy2=rz*fx-rx*fz, uz2=rx*fy-ry*fx;
    m[0]=rx;m[1]=ux2;m[2]=-fx;m[3]=0;
    m[4]=ry;m[5]=uy2;m[6]=-fy;m[7]=0;
    m[8]=rz;m[9]=uz2;m[10]=-fz;m[11]=0;
    m[12]=-(rx*ex+ry*ey+rz*ez);
    m[13]=-(ux2*ex+uy2*ey+uz2*ez);
    m[14]=(fx*ex+fy*ey+fz*ez);m[15]=1;
}

// Build a simple translate+scale model matrix
void buildModel(float tx,float ty,float tz,
                float sx,float sy,float sz, float* m) {
    m[0]=sx;m[1]=0;m[2]=0;m[3]=0;
    m[4]=0;m[5]=sy;m[6]=0;m[7]=0;
    m[8]=0;m[9]=0;m[10]=sz;m[11]=0;
    m[12]=tx;m[13]=ty;m[14]=tz;m[15]=1;
}

float viewMat[16], projMat[16];

void setUniforms(GLuint prog, float* model,
                 float r,float g,float b,
                 float lx,float ly,float lz,
                 float cx,float cy,float cz,
                 float shine) {
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"model"),1,GL_FALSE,model);
    glUniformMatrix4fv(glGetUniformLocation(prog,"view"), 1,GL_FALSE,viewMat);
    glUniformMatrix4fv(glGetUniformLocation(prog,"proj"), 1,GL_FALSE,projMat);
    glUniform3f(glGetUniformLocation(prog,"objectColor"),r,g,b);
    glUniform3f(glGetUniformLocation(prog,"lightPos"),lx,ly,lz);
    glUniform3f(glGetUniformLocation(prog,"lightColor"),1,1,1);
    glUniform3f(glGetUniformLocation(prog,"viewPos"),cx,cy,cz);
    glUniform1f(glGetUniformLocation(prog,"shininess"),shine);
}

void setBoardUniforms(GLuint prog, float* model,
                      float lx,float ly,float lz,
                      float cx,float cy,float cz) {
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"model"),1,GL_FALSE,model);
    glUniformMatrix4fv(glGetUniformLocation(prog,"view"), 1,GL_FALSE,viewMat);
    glUniformMatrix4fv(glGetUniformLocation(prog,"proj"), 1,GL_FALSE,projMat);
    glUniform3f(glGetUniformLocation(prog,"lightPos"),lx,ly,lz);
    glUniform3f(glGetUniformLocation(prog,"lightColor"),1,1,1);
    glUniform3f(glGetUniformLocation(prog,"viewPos"),cx,cy,cz);
}

// ─── Geometry builders ────────────────────────────────────────────────────────

// Generic mesh container
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
        glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(float),
                     verts.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,idx.size()*sizeof(unsigned int),
                     idx.data(),GL_STATIC_DRAW);
        // position (location 0)
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
        glEnableVertexAttribArray(0);
        // normal (location 1)
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),
                              (void*)(3*sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }

    void draw() {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES,indexCount,GL_UNSIGNED_INT,0);
        glBindVertexArray(0);
    }
};

// UV Sphere using spherical coordinates:
//   x = sin(phi)*cos(theta), y = cos(phi), z = sin(phi)*sin(theta)
Mesh buildSphere(int stacks, int slices) {
    std::vector<float>        v;
    std::vector<unsigned int> idx;
    for (int i=0;i<=stacks;i++) {
        float phi=PI*i/stacks;
        for (int j=0;j<=slices;j++) {
            float theta=2*PI*j/slices;
            float x=sinf(phi)*cosf(theta);
            float y=cosf(phi);
            float z=sinf(phi)*sinf(theta);
            v.insert(v.end(),{x,y,z, x,y,z}); // pos + normal
        }
    }
    for (int i=0;i<stacks;i++)
        for (int j=0;j<slices;j++) {
            unsigned int tl=i*(slices+1)+j, tr=tl+1;
            unsigned int bl=tl+(slices+1),  br=bl+1;
            idx.insert(idx.end(),{tl,bl,tr, tr,bl,br});
        }
    Mesh m; m.upload(v,idx); return m;
}

// Cylinder built from stacked rings using parametric circle:
//   x = cos(theta), z = sin(theta)
// Top and bottom caps are filled with triangle fans.
Mesh buildCylinder(int slices, float height) {
    std::vector<float>        v;
    std::vector<unsigned int> idx;
    float hh = height / 2.0f;

    // Side vertices — two rings, outward normals
    for (int j=0;j<=slices;j++) {
        float theta=2*PI*j/slices;
        float cx=cosf(theta), cz=sinf(theta);
        // bottom ring
        v.insert(v.end(),{cx,-hh,cz, cx,0,cz});
        // top ring
        v.insert(v.end(),{cx, hh,cz, cx,0,cz});
    }
    // Side triangles
    for (int j=0;j<slices;j++) {
        unsigned int b0=j*2, t0=b0+1, b1=b0+2, t1=b0+3;
        idx.insert(idx.end(),{b0,b1,t0, t0,b1,t1});
    }

    // Bottom cap — center vertex then fan
    unsigned int cBot=(unsigned int)(v.size()/6);
    v.insert(v.end(),{0,-hh,0, 0,-1,0});
    for (int j=0;j<=slices;j++) {
        float theta=2*PI*j/slices;
        v.insert(v.end(),{cosf(theta),-hh,sinf(theta), 0,-1,0});
    }
    for (int j=0;j<slices;j++)
        idx.insert(idx.end(),{cBot, cBot+j+2, cBot+j+1});

    // Top cap
    unsigned int cTop=(unsigned int)(v.size()/6);
    v.insert(v.end(),{0,hh,0, 0,1,0});
    for (int j=0;j<=slices;j++) {
        float theta=2*PI*j/slices;
        v.insert(v.end(),{cosf(theta),hh,sinf(theta), 0,1,0});
    }
    for (int j=0;j<slices;j++)
        idx.insert(idx.end(),{cTop, cTop+j+1, cTop+j+2});

    Mesh m; m.upload(v,idx); return m;
}

// Cube — 6 faces, each with its own outward normal for correct shading
Mesh buildCube() {
    // Each face: 4 vertices * (pos3 + norm3) = 24 floats
    std::vector<float> v = {
        // Front  (z=+1)
        -1,-1, 1, 0,0,1,   1,-1, 1, 0,0,1,
         1, 1, 1, 0,0,1,  -1, 1, 1, 0,0,1,
        // Back   (z=-1)
         1,-1,-1, 0,0,-1, -1,-1,-1, 0,0,-1,
        -1, 1,-1, 0,0,-1,  1, 1,-1, 0,0,-1,
        // Left   (x=-1)
        -1,-1,-1,-1,0,0,  -1,-1, 1,-1,0,0,
        -1, 1, 1,-1,0,0,  -1, 1,-1,-1,0,0,
        // Right  (x=+1)
         1,-1, 1, 1,0,0,   1,-1,-1, 1,0,0,
         1, 1,-1, 1,0,0,   1, 1, 1, 1,0,0,
        // Top    (y=+1)
        -1, 1, 1, 0,1,0,   1, 1, 1, 0,1,0,
         1, 1,-1, 0,1,0,  -1, 1,-1, 0,1,0,
        // Bottom (y=-1)
        -1,-1,-1, 0,-1,0,  1,-1,-1, 0,-1,0,
         1,-1, 1, 0,-1,0, -1,-1, 1, 0,-1,0,
    };
    std::vector<unsigned int> idx;
    for (int f=0;f<6;f++) {
        unsigned int b=f*4;
        idx.insert(idx.end(),{b,b+1,b+2, b,b+2,b+3});
    }
    Mesh m; m.upload(v,idx); return m;
}

// Checkerboard — single flat quad, pattern from shader
Mesh buildBoard(int w, int d) {
    float fw=(float)w, fd=(float)d;
    std::vector<float> v = {
        0,0,0, 0,1,0,   fw,0,0, 0,1,0,   fw,0,fd, 0,1,0,
        0,0,0, 0,1,0,   fw,0,fd,0,1,0,     0,0,fd, 0,1,0,
    };
    // No EBO for the board — use DrawArrays
    GLuint vao,vbo;
    glGenVertexArrays(1,&vao);
    glGenBuffers(1,&vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),
                          (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    Mesh m; m.vao=vao; m.vbo=vbo; m.indexCount=6; return m;
}

// ─── Camera ───────────────────────────────────────────────────────────────────
// Free-look camera using yaw and pitch angles.
struct Camera {
    float x=4, y=2, z=14;       // eye position
    float yaw=-90, pitch=-10;   // degrees
    float roll=0;               // camera roll angle
    float speed=0.2f, sens=1.5f;

    // Compute forward direction from yaw/pitch
    void forward(float& fx,float& fy,float& fz) {
        float yr=yaw*PI/180, pr=pitch*PI/180;
        fx=cosf(pr)*cosf(yr);
        fy=sinf(pr);
        fz=cosf(pr)*sinf(yr);
    }
    void moveForward(float d) {
        float fx,fy,fz; forward(fx,fy,fz);
        x+=fx*d; z+=fz*d;
    }
    void moveRight(float d) {
        float fx,fy,fz; forward(fx,fy,fz);
        // right = forward x world_up
        float rx=fz, rz=-fx;
        x+=rx*d; z+=rz*d;
    }
    void buildViewMat(float* m) {
        float fx,fy,fz; forward(fx,fy,fz);
        // Compute base right and up vectors
        float rx=fz, ry=0, rz=-fx;
        float rl=sqrtf(rx*rx+rz*rz);
        rx/=rl; rz/=rl;
        float ux=fy*rz-fz*ry, uy=fz*rx-fx*rz, uz=fx*ry-fy*rx;
        // Apply roll by rotating right and up around forward axis
        float rr = roll * PI / 180.0f;
        float cr=cosf(rr), sr=sinf(rr);
        float rx2 = cr*rx + sr*ux;
        float ry2 = cr*ry + sr*uy;
        float rz2 = cr*rz + sr*uz;
        float ux2 =-sr*rx + cr*ux;
        float uy2 =-sr*ry + cr*uy;
        float uz2 =-sr*rz + cr*uz;
        // Build view matrix with rolled up vector
        m[0]=rx2; m[1]=ux2; m[2]=-fx; m[3]=0;
        m[4]=ry2; m[5]=uy2; m[6]=-fy; m[7]=0;
        m[8]=rz2; m[9]=uz2; m[10]=-fz;m[11]=0;
        m[12]=-(rx2*x+ry2*y+rz2*z);
        m[13]=-(ux2*x+uy2*y+uz2*z);
        m[14]=(fx*x+fy*y+fz*z);m[15]=1;
    }
} cam;

// ─── Meshes & state ───────────────────────────────────────────────────────────
Mesh sphere, cylinder, cube, board;
float lightPos[] = {6, 8, 6};
int winW=900, winH=600;

// ─── Display ──────────────────────────────────────────────────────────────────
void display() {
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    cam.buildViewMat(viewMat);
    float cx=cam.x, cy=cam.y, cz=cam.z;
    float lx=lightPos[0],ly=lightPos[1],lz=lightPos[2];

    // Checkerboard floor — 8x8 centered at origin
    float bModel[16]; buildModel(-4,0,-4, 1,1,1, bModel);
    setBoardUniforms(boardProg,bModel,lx,ly,lz,cx,cy,cz);
    glBindVertexArray(board.vao);
    glDrawArrays(GL_TRIANGLES,0,6);
    glBindVertexArray(0);

    // Sphere — green, shiny, LEFT
    float sModel[16]; buildModel(-2.5f,1.2f,0, 1.2f,1.2f,1.2f, sModel);
    setUniforms(solidProg,sModel, 0,0.8f,0.1f, lx,ly,lz,cx,cy,cz, 96);
    sphere.draw();

    // Cube — blue, CENTER
    float cuModel[16]; buildModel(0,1,0, 1,1,1, cuModel);
    setUniforms(solidProg,cuModel, 0.1f,0.15f,0.85f, lx,ly,lz,cx,cy,cz, 48);
    cube.draw();

    // Cylinder — olive/yellow, RIGHT
    float cModel[16]; buildModel(2.5f,1,0, 1,1.5f,1, cModel);
    setUniforms(solidProg,cModel, 0.55f,0.5f,0.05f, lx,ly,lz,cx,cy,cz, 32);
    cylinder.draw();
    glutSwapBuffers();
}

// ─── Keyboard ─────────────────────────────────────────────────────────────────
void keyboard(unsigned char key, int, int) {
    switch (key) {
        // W/S — move up and down along Y axis
        case 'w': case 'W': cam.y += cam.speed; break;
        case 's': case 'S': cam.y -= cam.speed; break;
        // A/D — strafe left and right
        case 'a': case 'A': cam.moveRight(-cam.speed); break;
        case 'd': case 'D': cam.moveRight( cam.speed); break;
        // Q/E — roll camera
        case 'q': case 'Q': cam.roll -= 2.0f; break;
        case 'e': case 'E': cam.roll += 2.0f; break;
        // R — reset camera
        case 'r': case 'R':
            cam.x=4; cam.y=2; cam.z=14;
            cam.yaw=-90; cam.pitch=-10; cam.roll=0;
            break;
        case 27: exit(0);
    }
    glutPostRedisplay();
}

void special(int key, int, int) {
    int mod = glutGetModifiers();
    if (mod == GLUT_ACTIVE_SHIFT) {
        // Shift + Up   — move camera forward (Z-)
        // Shift + Down — move camera backward (Z+)
        switch (key) {
            case GLUT_KEY_UP:   cam.z -= 1.0f; break;
            case GLUT_KEY_DOWN: cam.z += 1.0f; break;
        }
    } else if (mod == GLUT_ACTIVE_CTRL) {
        // Ctrl arrows — precise pitch and yaw, 2 degrees each
        switch (key) {
            case GLUT_KEY_UP:    cam.pitch -= 2.0f;
                                 if(cam.pitch<-89) cam.pitch=-89; break;
            case GLUT_KEY_DOWN:  cam.pitch += 2.0f;
                                 if(cam.pitch> 89) cam.pitch= 89; break;
            case GLUT_KEY_LEFT:  cam.yaw -= 2.0f; break;
            case GLUT_KEY_RIGHT: cam.yaw += 2.0f; break;
        }
    } else {
        // Plain arrow keys — yaw and pitch
        switch (key) {
            case GLUT_KEY_LEFT:  cam.yaw   -= cam.sens * 2; break;
            case GLUT_KEY_RIGHT: cam.yaw   += cam.sens * 2; break;
            case GLUT_KEY_UP:    cam.pitch += cam.sens * 2;
                                 if(cam.pitch> 89) cam.pitch= 89; break;
            case GLUT_KEY_DOWN:  cam.pitch -= cam.sens * 2;
                                 if(cam.pitch<-89) cam.pitch=-89; break;
        }
    }
    glutPostRedisplay();
}

// ─── Reshape & timer ──────────────────────────────────────────────────────────
void reshape(int w, int h) {
    winW=w; winH=(h==0)?1:h;
    glViewport(0,0,winW,winH);
    buildPerspective(60,(float)winW/winH,0.1f,200,projMat);
}

void timer(int v) {
    glutPostRedisplay();
    glutTimerFunc(16,timer,v);
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH);
    glutInitWindowPosition(80,80);
    glutInitWindowSize(winW,winH);
    glutCreateWindow("Project 9 — Shader Scene");

    glewExperimental=GL_TRUE;
    if (glewInit()!=GLEW_OK) {
        std::cerr<<"GLEW failed\n"; return -1;
    }
    glEnable(GL_DEPTH_TEST);

    solidProg = buildProgram(vertSrc, fragSolidSrc);
    boardProg = buildProgram(vertSrc, fragBoardSrc);

    sphere   = buildSphere(32,32);
    cylinder = buildCylinder(32, 2.0f);
    cube     = buildCube();
    board    = buildBoard(8,8);

    buildPerspective(60,(float)winW/winH,0.1f,200,projMat);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutTimerFunc(16,timer,0);
    glutMainLoop();
    return 0;
}
