// Main.cpp
// Project 9 — Advanced Shader Scene
// GLFW + GLEW version. Zero GLU, zero GLSL, zero external dependencies
// beyond glfw, GL, and GLEW. Compiles with his exact command.
//
// Authors: Jared Walker & James Hohn
// Course:  CST-310 — Computer Graphics
// School:  Grand Canyon University
//
// Compile:
//   g++ Main.cpp -o run -lglfw -lGL -lGLEW -std=c++17
// Run:
//   ./run
//
// Controls:
//   W / S             — move camera up / down
//   A / D             — strafe left / right
//   Q / E             — roll camera
//   Arrow L/R         — yaw
//   Arrow U/D         — pitch
//   Shift + Up/Down   — move forward / backward (Z)
//   Ctrl  + Arrows    — fine yaw / pitch
//   R                 — reset camera
//   ESC               — quit

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>

const float PI = 3.14159265f;
int winW = 900, winH = 600;

// ─── Hand-written matrix helpers (no GLU needed) ──────────────────────────────

void setPerspective(float fovDeg, float aspect, float zn, float zf) {
    float f = 1.0f / tanf(fovDeg * PI / 360.0f);
    float m[16] = {
        f/aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (zf+zn)/(zn-zf), -1,
        0, 0, (2*zf*zn)/(zn-zf), 0
    };
    glMultMatrixf(m);
}

void setLookAt(float ex, float ey, float ez,
               float tx, float ty, float tz,
               float ux, float uy, float uz) {
    float f0=tx-ex, f1=ty-ey, f2=tz-ez;
    float fl=sqrtf(f0*f0+f1*f1+f2*f2);
    f0/=fl; f1/=fl; f2/=fl;
    float s0=f1*uz-f2*uy, s1=f2*ux-f0*uz, s2=f0*uy-f1*ux;
    float sl=sqrtf(s0*s0+s1*s1+s2*s2);
    s0/=sl; s1/=sl; s2/=sl;
    float u0=s1*f2-s2*f1, u1=s2*f0-s0*f2, u2=s0*f1-s1*f0;
    float m[16] = {
        s0, u0, -f0, 0,
        s1, u1, -f1, 0,
        s2, u2, -f2, 0,
        -(s0*ex+s1*ey+s2*ez),
        -(u0*ex+u1*ey+u2*ez),
         (f0*ex+f1*ey+f2*ez), 1
    };
    glMultMatrixf(m);
}

// ─── Hand-written geometry (no GLU quadrics needed) ───────────────────────────

void drawSphere(float r, int stacks, int slices) {
    for (int i = 0; i < stacks; i++) {
        float phi0 = PI * i / stacks;
        float phi1 = PI * (i+1) / stacks;
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float theta = 2*PI*j/slices;
            for (int k = 0; k < 2; k++) {
                float phi = (k==0) ? phi0 : phi1;
                float x = sinf(phi)*cosf(theta);
                float y = cosf(phi);
                float z = sinf(phi)*sinf(theta);
                glNormal3f(x, y, z);
                glVertex3f(r*x, r*y, r*z);
            }
        }
        glEnd();
    }
}

void drawCylinder(float r, float h, int slices) {
    // Side surface
    glBegin(GL_QUAD_STRIP);
    for (int j = 0; j <= slices; j++) {
        float theta = 2*PI*j/slices;
        float x = cosf(theta), z = sinf(theta);
        glNormal3f(x, 0, z);
        glVertex3f(r*x, 0, r*z);
        glVertex3f(r*x, h, r*z);
    }
    glEnd();
    // Top cap
    glNormal3f(0, 1, 0);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0, h, 0);
    for (int j = 0; j <= slices; j++) {
        float theta = 2*PI*j/slices;
        glVertex3f(r*cosf(theta), h, r*sinf(theta));
    }
    glEnd();
    // Bottom cap
    glNormal3f(0, -1, 0);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0, 0, 0);
    for (int j = slices; j >= 0; j--) {
        float theta = 2*PI*j/slices;
        glVertex3f(r*cosf(theta), 0, r*sinf(theta));
    }
    glEnd();
}

void drawCube(float s) {
    glBegin(GL_QUADS);
    glNormal3f( 0, 0, 1); glVertex3f(-s,-s,s); glVertex3f(s,-s,s); glVertex3f(s,s,s);  glVertex3f(-s,s,s);
    glNormal3f( 0, 0,-1); glVertex3f(s,-s,-s); glVertex3f(-s,-s,-s);glVertex3f(-s,s,-s);glVertex3f(s,s,-s);
    glNormal3f(-1, 0, 0); glVertex3f(-s,-s,-s);glVertex3f(-s,-s,s); glVertex3f(-s,s,s); glVertex3f(-s,s,-s);
    glNormal3f( 1, 0, 0); glVertex3f(s,-s,s);  glVertex3f(s,-s,-s); glVertex3f(s,s,-s); glVertex3f(s,s,s);
    glNormal3f( 0, 1, 0); glVertex3f(-s,s,s);  glVertex3f(s,s,s);   glVertex3f(s,s,-s); glVertex3f(-s,s,-s);
    glNormal3f( 0,-1, 0); glVertex3f(-s,-s,-s);glVertex3f(s,-s,-s); glVertex3f(s,-s,s); glVertex3f(-s,-s,s);
    glEnd();
}

// ─── Camera ───────────────────────────────────────────────────────────────────

struct Camera {
    float x=0, y=2, z=10;
    float yaw=-90, pitch=-10, roll=0;

    void getForward(float& fx, float& fy, float& fz) {
        float yr=yaw*PI/180, pr=pitch*PI/180;
        fx = cosf(pr)*cosf(yr);
        fy = sinf(pr);
        fz = cosf(pr)*sinf(yr);
    }
    void moveRight(float d) {
        float fx,fy,fz; getForward(fx,fy,fz);
        x += fz*d; z -= fx*d;
    }
    void apply() {
        float fx,fy,fz; getForward(fx,fy,fz);
        float rr = roll*PI/180;
        float ux = -sinf(rr), uy = cosf(rr), uz = 0;
        setLookAt(x,y,z, x+fx,y+fy,z+fz, ux,uy,uz);
    }
    void reset() {
        x=0; y=2; z=10;
        yaw=-90; pitch=-10; roll=0;
    }
} cam;

// ─── Lighting ─────────────────────────────────────────────────────────────────

void setupLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    GLfloat lpos0[] = { 6.0f,  8.0f,  6.0f, 1.0f};
    GLfloat lamb0[] = { 0.2f,  0.2f,  0.2f, 1.0f};
    GLfloat ldif0[] = { 1.0f,  1.0f,  1.0f, 1.0f};
    GLfloat lspc0[] = { 1.0f,  1.0f,  1.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lpos0);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  lamb0);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  ldif0);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lspc0);

    GLfloat lpos1[] = {-8.0f,  5.0f, -4.0f, 1.0f};
    GLfloat lamb1[] = { 0.0f,  0.0f,  0.0f, 1.0f};
    GLfloat ldif1[] = { 0.3f, 0.35f,  0.5f, 1.0f};
    GLfloat lspc1[] = { 0.2f,  0.2f,  0.3f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, lpos1);
    glLightfv(GL_LIGHT1, GL_AMBIENT,  lamb1);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  ldif1);
    glLightfv(GL_LIGHT1, GL_SPECULAR, lspc1);

    GLfloat fogColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_START, 12.0f);
    glFogf(GL_FOG_END,   35.0f);
}

// ─── Material ─────────────────────────────────────────────────────────────────

void setMaterial(float r, float g, float b, float shine) {
    GLfloat amb[] = {r*0.25f, g*0.25f, b*0.25f, 1};
    GLfloat dif[] = {r,       g,       b,       1};
    GLfloat spc[] = {0.8f,    0.8f,    0.8f,    1};
    glMaterialfv(GL_FRONT, GL_AMBIENT,   amb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE,   dif);
    glMaterialfv(GL_FRONT, GL_SPECULAR,  spc);
    glMaterialf (GL_FRONT, GL_SHININESS, shine);
    glColor3f(r, g, b);
}

// ─── Checkerboard ─────────────────────────────────────────────────────────────

void drawCheckerboard(int w, int d) {
    GLfloat spc[] = {0.3f, 0.3f, 0.3f, 1};
    glMaterialfv(GL_FRONT, GL_SPECULAR,  spc);
    glMaterialf (GL_FRONT, GL_SHININESS, 20);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    for (int xi = 0; xi < w; xi++) {
        for (int zi = 0; zi < d; zi++) {
            float wx = xi - w/2.0f;
            float wz = zi - d/2.0f;
            if ((xi + zi) % 2 == 0) {
                GLfloat dif[]={1,0,0,1}; GLfloat amb[]={0.3f,0,0,1};
                glMaterialfv(GL_FRONT,GL_DIFFUSE,dif);
                glMaterialfv(GL_FRONT,GL_AMBIENT,amb);
                glColor3f(1,0,0);
            } else {
                GLfloat dif[]={1,1,1,1}; GLfloat amb[]={0.3f,0.3f,0.3f,1};
                glMaterialfv(GL_FRONT,GL_DIFFUSE,dif);
                glMaterialfv(GL_FRONT,GL_AMBIENT,amb);
                glColor3f(1,1,1);
            }
            glVertex3f(wx,   0, wz);
            glVertex3f(wx+1, 0, wz);
            glVertex3f(wx+1, 0, wz+1);
            glVertex3f(wx,   0, wz+1);
        }
    }
    glEnd();
}

// ─── Scene ────────────────────────────────────────────────────────────────────

void drawScene() {
    GLfloat lpos0[] = { 6.0f, 8.0f,  6.0f, 1.0f};
    GLfloat lpos1[] = {-8.0f, 5.0f, -4.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lpos0);
    glLightfv(GL_LIGHT1, GL_POSITION, lpos1);

    drawCheckerboard(8, 8);

    // Sphere — green, LEFT
    glPushMatrix();
    glTranslatef(-2.5f, 1.2f, 0);
    setMaterial(0, 0.8f, 0.1f, 96);
    drawSphere(1.2f, 32, 32);
    glPopMatrix();

    // Cube — blue, CENTER
    glPushMatrix();
    glTranslatef(0, 1.0f, 0);
    setMaterial(0.1f, 0.15f, 0.85f, 48);
    drawCube(1.0f);
    glPopMatrix();

    // Cylinder — olive, RIGHT
    glPushMatrix();
    glTranslatef(2.5f, 0.0f, 0);
    setMaterial(0.55f, 0.5f, 0.05f, 32);
    drawCylinder(1.0f, 2.0f, 32);
    glPopMatrix();
}

// ─── Input ────────────────────────────────────────────────────────────────────

void processInput(GLFWwindow* win, float dt) {
    float mv = 3.0f * dt;
    float rt = 60.0f * dt;
    float fn = 30.0f * dt;

    int sh = (glfwGetKey(win,GLFW_KEY_LEFT_SHIFT)   ==GLFW_PRESS ||
              glfwGetKey(win,GLFW_KEY_RIGHT_SHIFT)  ==GLFW_PRESS);
    int ct = (glfwGetKey(win,GLFW_KEY_LEFT_CONTROL) ==GLFW_PRESS ||
              glfwGetKey(win,GLFW_KEY_RIGHT_CONTROL)==GLFW_PRESS);

    if (glfwGetKey(win,GLFW_KEY_ESCAPE)==GLFW_PRESS)
        glfwSetWindowShouldClose(win,true);

    if (glfwGetKey(win,GLFW_KEY_W)==GLFW_PRESS) cam.y += mv;
    if (glfwGetKey(win,GLFW_KEY_S)==GLFW_PRESS) cam.y -= mv;
    if (glfwGetKey(win,GLFW_KEY_A)==GLFW_PRESS) cam.moveRight(-mv);
    if (glfwGetKey(win,GLFW_KEY_D)==GLFW_PRESS) cam.moveRight( mv);
    if (glfwGetKey(win,GLFW_KEY_Q)==GLFW_PRESS) cam.roll -= rt;
    if (glfwGetKey(win,GLFW_KEY_E)==GLFW_PRESS) cam.roll += rt;
    if (glfwGetKey(win,GLFW_KEY_R)==GLFW_PRESS) cam.reset();

    if (sh) {
        if (glfwGetKey(win,GLFW_KEY_UP)  ==GLFW_PRESS) cam.z -= mv;
        if (glfwGetKey(win,GLFW_KEY_DOWN)==GLFW_PRESS) cam.z += mv;
    } else if (ct) {
        if (glfwGetKey(win,GLFW_KEY_LEFT) ==GLFW_PRESS) cam.yaw -= fn;
        if (glfwGetKey(win,GLFW_KEY_RIGHT)==GLFW_PRESS) cam.yaw += fn;
        if (glfwGetKey(win,GLFW_KEY_UP)  ==GLFW_PRESS){cam.pitch+=fn;if(cam.pitch>89)cam.pitch=89;}
        if (glfwGetKey(win,GLFW_KEY_DOWN)==GLFW_PRESS){cam.pitch-=fn;if(cam.pitch<-89)cam.pitch=-89;}
    } else {
        if (glfwGetKey(win,GLFW_KEY_LEFT) ==GLFW_PRESS) cam.yaw -= rt;
        if (glfwGetKey(win,GLFW_KEY_RIGHT)==GLFW_PRESS) cam.yaw += rt;
        if (glfwGetKey(win,GLFW_KEY_UP)  ==GLFW_PRESS){cam.pitch+=rt;if(cam.pitch>89)cam.pitch=89;}
        if (glfwGetKey(win,GLFW_KEY_DOWN)==GLFW_PRESS){cam.pitch-=rt;if(cam.pitch<-89)cam.pitch=-89;}
    }
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main() {
    if (!glfwInit()) { fprintf(stderr,"GLFW init failed\n"); return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    GLFWwindow* win = glfwCreateWindow(winW, winH,
                                       "Project 9 — Shader Scene", NULL, NULL);
    if (!win) { fprintf(stderr,"Window failed\n"); glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);

    glewExperimental = GL_TRUE;
    glewInit();

    glEnable(GL_DEPTH_TEST);
    setupLighting();

    float lastTime = 0.0f;
    while (!glfwWindowShouldClose(win)) {
        float now = (float)glfwGetTime();
        float dt  = now - lastTime;
        lastTime  = now;

        processInput(win, dt);

        int w, h;
        glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);

        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        setPerspective(60.0f, (float)w/h, 0.1f, 200.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        cam.apply();

        drawScene();

        glfwSwapBuffers(win);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
