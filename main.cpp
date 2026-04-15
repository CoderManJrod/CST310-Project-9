// main.cpp
// Project 9 — Advanced Shader Scene
// GLFW + GLEW + GLM version for maximum compatibility.
//
// Authors: Jared Walker & James Hohn
// Course:  CST-310 — Computer Graphics and Visualization
// School:  Grand Canyon University
//
// Compile:
//   g++ main.cpp -o run -lglfw -lGL -lGLEW -lGLU -std=c++17
// Run:
//   ./run
//
// Controls:
//   W / S             — move camera up / down
//   A / D             — strafe left / right
//   Q / E             — roll camera
//   Arrow L/R         — rotate yaw
//   Arrow U/D         — rotate pitch
//   Shift + Up/Down   — move forward / backward (Z)
//   Ctrl + Arrows     — fine yaw / pitch (2 degrees)
//   R                 — reset camera
//   ESC               — quit

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdio>

const float PI = 3.14159265f;

// ─── Window ───────────────────────────────────────────────────────────────────
int winW = 900, winH = 600;

// ─── Camera ───────────────────────────────────────────────────────────────────
struct Camera {
    float x=0, y=2, z=10;
    float yaw=-90, pitch=-10, roll=0;
    float speed=0.1f;

    void getForward(float& fx, float& fy, float& fz) {
        float yr=yaw*PI/180, pr=pitch*PI/180;
        fx=cosf(pr)*cosf(yr);
        fy=sinf(pr);
        fz=cosf(pr)*sinf(yr);
    }

    void moveRight(float d) {
        float fx,fy,fz; getForward(fx,fy,fz);
        float rx=fz, rz=-fx;
        x+=rx*d; z+=rz*d;
    }

    void apply() {
        float fx,fy,fz; getForward(fx,fy,fz);
        float rr=roll*PI/180;
        float ux=-sinf(rr), uy=cosf(rr), uz=0;
        gluLookAt(x,y,z, x+fx,y+fy,z+fz, ux,uy,uz);
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
    GLfloat ldif1[] = { 0.3f,  0.35f, 0.5f, 1.0f};
    GLfloat lspc1[] = { 0.2f,  0.2f,  0.3f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, lpos1);
    glLightfv(GL_LIGHT1, GL_AMBIENT,  lamb1);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  ldif1);
    glLightfv(GL_LIGHT1, GL_SPECULAR, lspc1);

    // Fog
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
    glEnable(GL_LIGHTING);
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
                GLfloat dif[] = {1,0,0,1};
                GLfloat amb[] = {0.3f,0,0,1};
                glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
                glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
                glColor3f(1,0,0);
            } else {
                GLfloat dif[] = {1,1,1,1};
                GLfloat amb[] = {0.3f,0.3f,0.3f,1};
                glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
                glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
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

// ─── Draw scene ───────────────────────────────────────────────────────────────
void drawScene() {
    GLfloat lpos0[] = { 6.0f,  8.0f,  6.0f, 1.0f};
    GLfloat lpos1[] = {-8.0f,  5.0f, -4.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lpos0);
    glLightfv(GL_LIGHT1, GL_POSITION, lpos1);

    drawCheckerboard(8, 8);

    // Sphere — green, LEFT
    glPushMatrix();
    glTranslatef(-2.5f, 1.2f, 0);
    setMaterial(0, 0.8f, 0.1f, 96);
    GLUquadric* qs = gluNewQuadric();
    gluQuadricNormals(qs, GLU_SMOOTH);
    gluSphere(qs, 1.2, 32, 32);
    gluDeleteQuadric(qs);
    glPopMatrix();

    // Cube — blue, CENTER
    glPushMatrix();
    glTranslatef(0, 1.0f, 0);
    setMaterial(0.1f, 0.15f, 0.85f, 48);
    // Draw cube manually with normals
    float s = 1.0f;
    glBegin(GL_QUADS);
    glNormal3f(0,0,1);  glVertex3f(-s,-s,s); glVertex3f(s,-s,s); glVertex3f(s,s,s);  glVertex3f(-s,s,s);
    glNormal3f(0,0,-1); glVertex3f(s,-s,-s); glVertex3f(-s,-s,-s);glVertex3f(-s,s,-s);glVertex3f(s,s,-s);
    glNormal3f(-1,0,0); glVertex3f(-s,-s,-s);glVertex3f(-s,-s,s); glVertex3f(-s,s,s); glVertex3f(-s,s,-s);
    glNormal3f(1,0,0);  glVertex3f(s,-s,s);  glVertex3f(s,-s,-s); glVertex3f(s,s,-s); glVertex3f(s,s,s);
    glNormal3f(0,1,0);  glVertex3f(-s,s,s);  glVertex3f(s,s,s);   glVertex3f(s,s,-s); glVertex3f(-s,s,-s);
    glNormal3f(0,-1,0); glVertex3f(-s,-s,-s);glVertex3f(s,-s,-s); glVertex3f(s,-s,s); glVertex3f(-s,-s,s);
    glEnd();
    glPopMatrix();

    // Cylinder — olive, RIGHT
    glPushMatrix();
    glTranslatef(2.5f, 0.0f, 0);
    glRotatef(-90, 1, 0, 0);
    setMaterial(0.55f, 0.5f, 0.05f, 32);
    GLUquadric* qc = gluNewQuadric();
    gluQuadricNormals(qc, GLU_SMOOTH);
    gluCylinder(qc, 1, 1, 2, 32, 1);
    glPushMatrix();
    glTranslatef(0, 0, 2);
    gluDisk(qc, 0, 1, 32, 1);
    glPopMatrix();
    glPushMatrix();
    glRotatef(180, 1, 0, 0);
    gluDisk(qc, 0, 1, 32, 1);
    glPopMatrix();
    gluDeleteQuadric(qc);
    glPopMatrix();
}

// ─── Input ────────────────────────────────────────────────────────────────────
void processInput(GLFWwindow* win, float dt) {
    float moveSpeed = 3.0f * dt;
    float rotSpeed  = 60.0f * dt;
    float fineSpeed = 30.0f * dt;

    int shiftHeld = (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT)    == GLFW_PRESS ||
                     glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT)   == GLFW_PRESS);
    int ctrlHeld  = (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL)  == GLFW_PRESS ||
                     glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);

    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(win, true);

    // W/S — up and down
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) cam.y += moveSpeed;
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) cam.y -= moveSpeed;

    // A/D — strafe
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) cam.moveRight(-moveSpeed);
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) cam.moveRight( moveSpeed);

    // Q/E — roll
    if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS) cam.roll -= rotSpeed;
    if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS) cam.roll += rotSpeed;

    // R — reset
    if (glfwGetKey(win, GLFW_KEY_R) == GLFW_PRESS) cam.reset();

    if (shiftHeld) {
        // Shift + Up/Down — move forward/backward along Z
        if (glfwGetKey(win, GLFW_KEY_UP)   == GLFW_PRESS) cam.z -= moveSpeed;
        if (glfwGetKey(win, GLFW_KEY_DOWN)  == GLFW_PRESS) cam.z += moveSpeed;
    } else if (ctrlHeld) {
        // Ctrl + arrows — fine pitch/yaw
        if (glfwGetKey(win, GLFW_KEY_LEFT)  == GLFW_PRESS) cam.yaw   -= fineSpeed;
        if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) cam.yaw   += fineSpeed;
        if (glfwGetKey(win, GLFW_KEY_UP)    == GLFW_PRESS) { cam.pitch += fineSpeed; if(cam.pitch> 89) cam.pitch= 89; }
        if (glfwGetKey(win, GLFW_KEY_DOWN)  == GLFW_PRESS) { cam.pitch -= fineSpeed; if(cam.pitch<-89) cam.pitch=-89; }
    } else {
        // Plain arrows — yaw and pitch
        if (glfwGetKey(win, GLFW_KEY_LEFT)  == GLFW_PRESS) cam.yaw   -= rotSpeed;
        if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) cam.yaw   += rotSpeed;
        if (glfwGetKey(win, GLFW_KEY_UP)    == GLFW_PRESS) { cam.pitch += rotSpeed; if(cam.pitch> 89) cam.pitch= 89; }
        if (glfwGetKey(win, GLFW_KEY_DOWN)  == GLFW_PRESS) { cam.pitch -= rotSpeed; if(cam.pitch<-89) cam.pitch=-89; }
    }
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    if (!glfwInit()) { fprintf(stderr, "GLFW init failed\n"); return -1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    GLFWwindow* win = glfwCreateWindow(winW, winH,
                                       "Project 9 — Shader Scene", NULL, NULL);
    if (!win) { fprintf(stderr, "Window failed\n"); glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);

    glewExperimental = GL_TRUE;
    glewInit();

    glEnable(GL_DEPTH_TEST);
    setupLighting();

    float lastTime = 0.0f;

    while (!glfwWindowShouldClose(win)) {
        float currentTime = (float)glfwGetTime();
        float dt = currentTime - lastTime;
        lastTime = currentTime;

        processInput(win, dt);

        int w, h;
        glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Projection
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60.0, (double)w/h, 0.1, 200.0);

        // View
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
