// main.cpp
// Reproduction of your Raylib example in C++ using GLFW + GLAD + GLM (no experimental GLM).
// Build: g++ -std=c++17 main.cpp glad.c -lglfw -ldl -lm -o gungale

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // perspective, lookAt, translate, rotate, scale
#include <glm/gtc/type_ptr.hpp>         // value_ptr

// -----------------------------
// Constants (same as your C file)
#define GRAVITY         32.0f
#define MAX_SPEED       20.0f
#define CROUCH_SPEED     5.0f
#define JUMP_FORCE      12.0f
#define MAX_ACCEL      150.0f
#define FRICTION         0.86f
#define AIR_DRAG         0.98f
#define CONTROL         15.0f
#define CROUCH_HEIGHT    0.0f
#define STAND_HEIGHT     1.0f
#define BOTTOM_HEIGHT    0.5f

const int SCREEN_W = 1200;
const int SCREEN_H = 1000;
const float PI_F = 3.14159265358979323846f;

// -----------------------------
// Types (mirror your Raylib Body)
struct Body {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 dir{0.0f};
    bool isGrounded = true;
};

// Globals ported from your C file
static glm::vec2 sensitivity = glm::vec2(0.001f, 0.001f);

static Body player;
static glm::vec2 lookRotation = glm::vec2(0.0f); // x = yaw, y = pitch
static float headTimer = 0.0f;
static float walkLerp = 0.0f;
static float headLerp = STAND_HEIGHT;
static glm::vec2 lean = glm::vec2(0.0f);

// Input helpers
static double lastMouseX = 0.0, lastMouseY = 0.0;
static bool firstMouse = true;
static bool prevSpace = false;

// Timing
static double lastTime = 0.0;
static float getDeltaSeconds() {
    double now = glfwGetTime();
    static double prev = 0.0;
    if (prev == 0.0) prev = now;
    float dt = (float)(now - prev);
    prev = now;
    if (dt <= 0.0f) dt = 1.0f/60.0f;
    return dt;
}

// -----------------------------
// Simple math helpers that match Raylib semantics
static glm::vec3 RotateVecByAxisAngle(const glm::vec3 &v, const glm::vec3 &axis, float angle) {
    // Use rotation matrix: rotate(mat4(1), angle, axis) * vec4(v, 1)
    glm::mat4 M = glm::rotate(glm::mat4(1.0f), angle, glm::normalize(axis));
    glm::vec4 r = M * glm::vec4(v, 1.0f);
    return glm::vec3(r.x, r.y, r.z);
}

static float VecAngle(const glm::vec3 &a, const glm::vec3 &b) {
    float la = glm::length(a), lb = glm::length(b);
    if (la == 0.0f || lb == 0.0f) return 0.0f;
    float d = glm::dot(a, b) / (la * lb);
    d = glm::clamp(d, -1.0f, 1.0f);
    return acosf(d);
}

static glm::vec3 VecNeg(const glm::vec3 &v) { return -v; }

// -----------------------------
// Simple shader (lambert-ish)
const char* vs_src = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uModel;
uniform mat4 uVP;
out vec3 vWorldPos;
out vec3 vNormal;
void main(){
    vec4 worldPos = uModel * vec4(aPos,1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uVP * worldPos;
}
)";
const char* fs_src = R"(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
out vec4 FragColor;
uniform vec3 uColor;
uniform vec3 uLightPos;
void main(){
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPos - vWorldPos);
    float diff = max(dot(N,L), 0.0);
    vec3 col = uColor * (0.2 + diff*0.8);
    FragColor = vec4(col,1.0);
}
)";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetShaderInfoLog(s, 1024, NULL, buf);
        std::fprintf(stderr, "Shader compile error: %s\n", buf);
    }
    return s;
}
static GLuint linkProgram(const char* vs, const char* fs) {
    GLuint vs_s = compileShader(GL_VERTEX_SHADER, vs);
    GLuint fs_s = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs_s); glAttachShader(p, fs_s);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetProgramInfoLog(p, 1024, NULL, buf);
        std::fprintf(stderr, "Program link error: %s\n", buf);
    }
    glDeleteShader(vs_s); glDeleteShader(fs_s);
    return p;
}

// -----------------------------
// Mesh builders (cube & sphere)
struct Mesh { GLuint vao=0, vbo=0, ebo=0; GLsizei indexCount=0; };

static Mesh makeCube() {
    // 36 vertices (pos + normal)
    float data[] = {
        // pos                // normal
        -0.5f,-0.5f,-0.5f,     0,0,-1,
         0.5f,-0.5f,-0.5f,     0,0,-1,
         0.5f, 0.5f,-0.5f,     0,0,-1,
         0.5f, 0.5f,-0.5f,     0,0,-1,
        -0.5f, 0.5f,-0.5f,     0,0,-1,
        -0.5f,-0.5f,-0.5f,     0,0,-1,

        -0.5f,-0.5f, 0.5f,     0,0,1,
         0.5f,-0.5f, 0.5f,     0,0,1,
         0.5f, 0.5f, 0.5f,     0,0,1,
         0.5f, 0.5f, 0.5f,     0,0,1,
        -0.5f, 0.5f, 0.5f,     0,0,1,
        -0.5f,-0.5f, 0.5f,     0,0,1,

        -0.5f, 0.5f, 0.5f,    -1,0,0,
        -0.5f, 0.5f,-0.5f,    -1,0,0,
        -0.5f,-0.5f,-0.5f,    -1,0,0,
        -0.5f,-0.5f,-0.5f,    -1,0,0,
        -0.5f,-0.5f, 0.5f,    -1,0,0,
        -0.5f, 0.5f, 0.5f,    -1,0,0,

         0.5f, 0.5f, 0.5f,     1,0,0,
         0.5f, 0.5f,-0.5f,     1,0,0,
         0.5f,-0.5f,-0.5f,     1,0,0,
         0.5f,-0.5f,-0.5f,     1,0,0,
         0.5f,-0.5f, 0.5f,     1,0,0,
         0.5f, 0.5f, 0.5f,     1,0,0,

        -0.5f,-0.5f,-0.5f,     0,-1,0,
         0.5f,-0.5f,-0.5f,     0,-1,0,
         0.5f,-0.5f, 0.5f,     0,-1,0,
         0.5f,-0.5f, 0.5f,     0,-1,0,
        -0.5f,-0.5f, 0.5f,     0,-1,0,
        -0.5f,-0.5f,-0.5f,     0,-1,0,

        -0.5f, 0.5f,-0.5f,     0,1,0,
         0.5f, 0.5f,-0.5f,     0,1,0,
         0.5f, 0.5f, 0.5f,     0,1,0,
         0.5f, 0.5f, 0.5f,     0,1,0,
        -0.5f, 0.5f, 0.5f,     0,1,0,
        -0.5f, 0.5f,-0.5f,     0,1,0
    };
    Mesh m;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    m.indexCount = 36;
    return m;
}

static Mesh makeSphere(int latSeg = 24, int lonSeg = 32) {
    std::vector<float> verts;
    std::vector<unsigned int> idx;
    for (int y=0;y<=latSeg;y++){
        float theta = (float)y/latSeg * PI_F;
        for (int x=0;x<=lonSeg;x++){
            float phi = (float)x/lonSeg * 2.0f*PI_F;
            float sx = sinf(theta)*cosf(phi);
            float sy = cosf(theta);
            float sz = sinf(theta)*sinf(phi);
            verts.push_back(sx); verts.push_back(sy); verts.push_back(sz);
            verts.push_back(sx); verts.push_back(sy); verts.push_back(sz);
        }
    }
    for (int y=0;y<latSeg;y++){
        for (int x=0;x<lonSeg;x++){
            int a = y*(lonSeg+1) + x;
            int b = a + lonSeg + 1;
            idx.push_back(a); idx.push_back(b); idx.push_back(a+1);
            idx.push_back(b); idx.push_back(b+1); idx.push_back(a+1);
        }
    }
    Mesh m;
    glGenVertexArrays(1,&m.vao); glGenBuffers(1,&m.vbo); glGenBuffers(1,&m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
    m.indexCount = (GLsizei)idx.size();
    return m;
}

// -----------------------------
// Ported UpdateBody (uses glm, same logic)
static void UpdateBody(Body *body, float rot, char side, char forward, bool jumpPressed, bool crouchHold, float delta)
{
    glm::vec2 input = glm::vec2((float)side, (float)-forward);

#if 0
    if ((side != 0) && (forward != 0)) input = glm::normalize(input);
#endif

    if (!body->isGrounded) body->velocity.y -= GRAVITY*delta;

    // Jump press (raylib used IsKeyPressed) -> handled by caller as jumpPressed
    if (body->isGrounded && jumpPressed)
    {
        body->velocity.y = JUMP_FORCE;
        body->isGrounded = false;
    }

    glm::vec3 front = glm::vec3( sinf(rot), 0.0f, cosf(rot) );
    glm::vec3 right = glm::vec3( cosf(-rot), 0.0f, sinf(-rot) );

    glm::vec3 desiredDir = glm::vec3( input.x*right.x + input.y*front.x, 0.0f, input.x*right.z + input.y*front.z );
    body->dir = glm::mix(body->dir, desiredDir, glm::clamp(CONTROL*delta, 0.0f, 1.0f));

    float decel = (body->isGrounded ? FRICTION : AIR_DRAG);
    glm::vec3 hvel = glm::vec3(body->velocity.x*decel, 0.0f, body->velocity.z*decel);

    float hvelLength = glm::length(hvel);
    if (hvelLength < (MAX_SPEED*0.01f)) hvel = glm::vec3(0.0f);

    float speed = glm::dot(hvel, body->dir);

    float maxSpeed = (crouchHold? CROUCH_SPEED : MAX_SPEED);
    float accel = glm::clamp(maxSpeed - speed, 0.0f, MAX_ACCEL*delta);
    hvel.x += body->dir.x*accel;
    hvel.z += body->dir.z*accel;

    body->velocity.x = hvel.x;
    body->velocity.z = hvel.z;

    body->position.x += body->velocity.x*delta;
    body->position.y += body->velocity.y*delta;
    body->position.z += body->velocity.z*delta;

    if (body->position.y <= 0.0f)
    {
        body->position.y = 0.0f;
        body->velocity.y = 0.0f;
        body->isGrounded = true;
    }
}

// -----------------------------
// UpdateCameraFPS port (recreates the same math)
static void UpdateCameraFPS(glm::vec3 &camPosition, glm::vec3 &camTarget, glm::vec3 &camUp)
{
    const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 targetOffset = glm::vec3(0.0f, 0.0f, -1.0f);

    // Left and right (yaw)
    glm::vec3 yaw = RotateVecByAxisAngle(targetOffset, up, lookRotation.x);

    // Clamp view up
    float maxAngleUp = VecAngle(up, yaw);
    maxAngleUp -= 0.001f;
    if ( -lookRotation.y > maxAngleUp ) { lookRotation.y = -maxAngleUp; }

    // Clamp view down
    float maxAngleDown = VecAngle(-up, yaw);
    maxAngleDown *= -1.0f;
    maxAngleDown += 0.001f;
    if ( -lookRotation.y < maxAngleDown ) { lookRotation.y = -maxAngleDown; }

    // Right vector
    glm::vec3 right = glm::normalize(glm::cross(yaw, up));

    // Rotate view vector around right axis (pitch)
    float pitchAngle = -lookRotation.y - lean.y;
    pitchAngle = glm::clamp(pitchAngle, -PI_F/2.0f + 0.0001f, PI_F/2.0f - 0.0001f);
    glm::vec3 pitch = RotateVecByAxisAngle(yaw, right, pitchAngle);

    // Head animation: rotate up direction around forward axis
    float headSin = sinf(headTimer*PI_F);
    float headCos = cosf(headTimer*PI_F);
    const float stepRotation = 0.01f;
    camUp = RotateVecByAxisAngle(up, pitch, headSin*stepRotation + lean.x);

    // Camera bob
    const float bobSide = 0.1f;
    const float bobUp = 0.15f;
    glm::vec3 bobbing = right * (headSin * bobSide);
    bobbing.y = fabsf(headCos * bobUp);

    camPosition += bobbing * walkLerp;
    camTarget = camPosition + pitch;
}

// -----------------------------
// MAIN
int main()
{
    // GLFW init
    if (!glfwInit()) {
        fprintf(stderr, "GLFW init failed\n");
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCREEN_W, SCREEN_H, "gungale - glm exact port", NULL, NULL);
    if (!window) { fprintf(stderr, "Window create failed\n"); glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { fprintf(stderr, "GLAD init failed\n"); return -1; }

    glEnable(GL_DEPTH_TEST);

    GLuint prog = linkProgram(vs_src, fs_src);
    glUseProgram(prog);

    Mesh cube = makeCube();
    Mesh sphere = makeSphere(24, 32);

    // initial camera
    float cameraFovy = 60.0f; // degrees
    glm::vec3 cameraPosition = glm::vec3(player.position.x, player.position.y + (BOTTOM_HEIGHT + headLerp), player.position.z);
    glm::vec3 cameraTarget = cameraPosition + glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    // mouse setup
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    lastMouseX = mx; lastMouseY = my; firstMouse = true;

    double lastLog = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        float delta = getDeltaSeconds();

        // --- Input / Mouse delta (Raylib used GetMouseDelta)
        glfwGetCursorPos(window, &mx, &my);
        if (firstMouse) { lastMouseX = mx; lastMouseY = my; firstMouse = false; }
        double dx = mx - lastMouseX;
        double dy = my - lastMouseY;
        lastMouseX = mx; lastMouseY = my;

        lookRotation.x -= (float)(dx * sensitivity.x);
        lookRotation.y += (float)(dy * sensitivity.y);

        // Key states replicate Raylib's (IsKeyDown)
        char sideway = (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
        char forward = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);
        bool crouching = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS);
        bool spaceNow = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
        bool jumpPressed = (!prevSpace && spaceNow); // IsKeyPressed behaviour
        prevSpace = spaceNow;

        // Update body (uses delta)
        UpdateBody(&player, lookRotation.x, sideway, forward, jumpPressed, crouching, delta);

        // head lerp
        headLerp = glm::mix(headLerp, (crouching ? CROUCH_HEIGHT : STAND_HEIGHT), glm::clamp(20.0f*delta, 0.0f, 1.0f));
        cameraPosition = glm::vec3(player.position.x, player.position.y + (BOTTOM_HEIGHT + headLerp), player.position.z);

        if (player.isGrounded && ((forward != 0) || (sideway != 0))) {
            headTimer += delta*3.0f;
            walkLerp = glm::mix(walkLerp, 1.0f, glm::clamp(10.0f*delta, 0.0f, 1.0f));
            cameraFovy = glm::mix(cameraFovy, 55.0f, glm::clamp(5.0f*delta, 0.0f, 1.0f));
        } else {
            walkLerp = glm::mix(walkLerp, 0.0f, glm::clamp(10.0f*delta, 0.0f, 1.0f));
            cameraFovy = glm::mix(cameraFovy, 60.0f, glm::clamp(5.0f*delta, 0.0f, 1.0f));
        }

        lean.x = glm::mix(lean.x, sideway*0.02f, glm::clamp(10.0f*delta, 0.0f, 1.0f));
        lean.y = glm::mix(lean.y, forward*0.015f, glm::clamp(10.0f*delta, 0.0f, 1.0f));

        // Update camera orientation/target/up using same logic as UpdateCameraFPS
        // We'll compute cameraTarget & cameraUp from cameraPosition and lookRotation/head bob/lean
        // Reset cameraPosition before applying bob (UpdateCameraFPS adds bob to camera->position)
        cameraPosition = glm::vec3(player.position.x, player.position.y + (BOTTOM_HEIGHT + headLerp), player.position.z);

        // Update camera vectors
        UpdateCameraFPS(cameraPosition, cameraTarget, cameraUp);

        // Prepare matrices
        int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
        float aspect = (float)fbw / (float)fbh;
        glm::mat4 P = glm::perspective(glm::radians(cameraFovy), aspect, 0.1f, 2000.0f);
        glm::mat4 V = glm::lookAt(cameraPosition, cameraTarget, cameraUp);
        glm::mat4 VP = P * V;

        // render
        glViewport(0,0,fbw,fbh);
        glClearColor(0.94f, 0.94f, 0.94f, 1.0f); // RAYWHITE background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(prog);
        GLint loc_uModel = glGetUniformLocation(prog, "uModel");
        GLint loc_uVP = glGetUniformLocation(prog, "uVP");
        GLint loc_uColor = glGetUniformLocation(prog, "uColor");
        GLint loc_uLightPos = glGetUniformLocation(prog, "uLightPos");

        glUniform3f(loc_uLightPos, 300.0f, 300.0f, 0.0f);

        // Draw level (identical to DrawLevel)
        const int floorExtent = 25;
        const float tileSize = 5.0f;
        glm::vec3 tileColor1 = glm::vec3(150.0f/255.0f, 200.0f/255.0f, 200.0f/255.0f);
        glm::vec3 lightGray = glm::vec3(0.827f, 0.827f, 0.827f);

        glBindVertexArray(cube.vao);
        for (int y = -floorExtent; y < floorExtent; ++y) {
            for (int x = -floorExtent; x < floorExtent; ++x) {
                bool drawTile = false;
                glm::vec3 color = lightGray;
                if ((y & 1) && (x & 1)) { drawTile = true; color = tileColor1; }
                else if (!(y & 1) && !(x & 1)) { drawTile = true; color = lightGray; }
                if (!drawTile) continue;

                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x*tileSize, 0.0f, y*tileSize));
                model = glm::scale(model, glm::vec3(tileSize, 0.001f, tileSize)); // flat plane using cube scaled
                glm::mat4 mvp = VP * model;
                glUniformMatrix4fv(loc_uModel, 1, GL_FALSE, glm::value_ptr(model));
                glUniformMatrix4fv(loc_uVP, 1, GL_FALSE, glm::value_ptr(VP));
                glUniform3f(loc_uColor, color.r, color.g, color.b);
                glDrawArrays(GL_TRIANGLES, 0, cube.indexCount);
            }
        }

        // towers
        glm::vec3 towerSize = glm::vec3(16.0f, 32.0f, 16.0f);
        glm::vec3 towerPos = glm::vec3(16.0f, 16.0f, 16.0f);
        for (int i=0;i<4;i++){
            glm::vec3 pos = towerPos;
            if (i==1) pos.x *= -1.0f;
            if (i==2) pos.z *= -1.0f;
            if (i==3) { pos.x *= -1.0f; } // returns to +x * -1? mirrors sequence in original code
            glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
            model = glm::scale(model, towerSize);
            glUniformMatrix4fv(loc_uModel, 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(loc_uVP, 1, GL_FALSE, glm::value_ptr(VP));
            glUniform3f(loc_uColor, 150.0f/255.0f, 200.0f/255.0f, 200.0f/255.0f);
            glBindVertexArray(cube.vao);
            glDrawArrays(GL_TRIANGLES, 0, cube.indexCount);
        }

        // red sun
        {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(300.0f,300.0f,0.0f));
            model = glm::scale(model, glm::vec3(100.0f));
            glUniformMatrix4fv(loc_uModel, 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(loc_uVP, 1, GL_FALSE, glm::value_ptr(VP));
            glUniform3f(loc_uColor, 1.0f, 0.0f, 0.0f);
            glBindVertexArray(sphere.vao);
            glDrawElements(GL_TRIANGLES, sphere.indexCount, GL_UNSIGNED_INT, 0);
        }

        // Print velocity occasionally (since no text HUD)
        double now = glfwGetTime();
        if (now - lastLog > 1.0) {
            float vel = glm::length(glm::vec2(player.velocity.x, player.velocity.z));
            printf("Velocity Len: %06.3f\n", vel);
            lastLog = now;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // cleanup
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
