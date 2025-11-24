#include "lupine/rendering/backends/opengl/OpenGLGizmoRenderer.hpp"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include <GL/glew.h>
#include <GL/glu.h>
#include <cmath>

namespace lupine {

void OpenGLGizmoRenderer::drawMove2D() {

    glLineWidth(3.0f);
    glBegin(GL_LINES);

    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(1.0f, 0.0f);

    glVertex2f(1.0f, 0.0f);
    glVertex2f(0.9f, 0.05f);
    glVertex2f(1.0f, 0.0f);
    glVertex2f(0.9f, -0.05f);

    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(0.0f, 1.0f);

    glVertex2f(0.0f, 1.0f);
    glVertex2f(0.05f, 0.9f);
    glVertex2f(0.0f, 1.0f);
    glVertex2f(-0.05f, 0.9f);
    glEnd();
    glLineWidth(1.0f);
}

void OpenGLGizmoRenderer::drawScale2D() {

    glLineWidth(2.0f);
    glBegin(GL_LINES);

    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(1.0f, 0.0f);

    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(0.0f, 1.0f);
    glEnd();

    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(0.95f, -0.05f);
    glVertex2f(1.05f, -0.05f);
    glVertex2f(1.05f, 0.05f);
    glVertex2f(0.95f, 0.05f);
    glEnd();
    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(-0.05f, 0.95f);
    glVertex2f(0.05f, 0.95f);
    glVertex2f(0.05f, 1.05f);
    glVertex2f(-0.05f, 1.05f);
    glEnd();
    glLineWidth(1.0f);
}

void OpenGLGizmoRenderer::drawRotate2D() {

    glColor3f(0.2f, 0.6f, 1.0f);
    float cx = 0.0f, cy = 0.0f, r = 0.8f;
    int segments = 64;
    glLineWidth(3.0f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float theta = (float)i / (float)segments * 2.0f * 3.1415926f;
        float x = r * std::cos(theta);
        float y = r * std::sin(theta);
        glVertex2f(cx + x, cy + y);
    }
    glEnd();

    float handleTheta = 0.0f;
    float hx = r * std::cos(handleTheta);
    float hy = r * std::sin(handleTheta);
    float hr = 0.07f;
    glBegin(GL_TRIANGLE_FAN);
    glColor3f(1.0f, 1.0f, 0.0f);
    glVertex2f(hx, hy);
    for (int i = 0; i <= segments; ++i) {
        float theta = (float)i / (float)segments * 2.0f * 3.1415926f;
        glVertex2f(hx + hr * std::cos(theta), hy + hr * std::sin(theta));
    }
    glEnd();
    glLineWidth(1.0f);
}

void OpenGLGizmoRenderer::drawComposite2D() {

    drawMove2D();
    drawScale2D();
    drawRotate2D();
}

void OpenGLGizmoRenderer::drawScale3D() {

}

void OpenGLGizmoRenderer::drawRotate3D() {

}

void OpenGLGizmoRenderer::drawMove3D() {

}

void OpenGLGizmoRenderer::drawComposite3D() {

}

void OpenGLGizmoRenderer::begin2DGizmoOverlay(int viewportWidth, int viewportHeight) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, viewportWidth, 0, viewportHeight);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void OpenGLGizmoRenderer::end2DGizmoOverlay() {
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

void OpenGLGizmoRenderer::drawMove2DAt(float x, float y) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    drawMove2D();
    glPopMatrix();
}

void OpenGLGizmoRenderer::drawScale2DAt(float x, float y) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    drawScale2D();
    glPopMatrix();
}

void OpenGLGizmoRenderer::drawRotate2DAt(float x, float y) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    drawRotate2D();
    glPopMatrix();
}

void OpenGLGizmoRenderer::drawComposite2DAt(float x, float y) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    drawComposite2D();
    glPopMatrix();
}

}
