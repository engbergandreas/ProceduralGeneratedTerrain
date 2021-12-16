#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/noise.hpp>
#include <glad/glad.h> //must be included before glfw 
#include <GLFW/glfw3.h>

#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>

#include "header/Shader.h"
#include "header/Mesh.h"
#include "header/CameraControl.h"
#include "header/ChunkHandler.h"
#include "header/CameraPlane.h"
#include "header/glugg.h"


void initialize();
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_position_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
std::vector<CameraPlane> computeCameraPlanes(const std::vector<glm::vec3>& points);
void updateCamera2();

void printmat4(const glm::mat4& mat);

//settings
int constexpr gridSize{ 3 };

const unsigned int SCREEN_WIDTH = 800, SCREEN_HEIGHT = 600;
glm::mat4 camera2 = glm::mat4(1.0f);
bool toggleCamera{ true }; //Toggles between camera 1 and 2 
CameraControl camera1Control{ glm::vec3{ 0.0f, 1.0f, 0.0f } };
float deltaTime = 0.0f, lastFrame = 0.0f;
bool cull = false, wireFrame = false, drawbb = false;


void MakeCylinderAlt(int aSlices, float height, float topwidth, float bottomwidth)
{
    gluggMode(GLUGG_TRIANGLE_STRIP);
    glm::vec3 top = glm::vec3{ 0, height, 0 };
    glm::vec3 center = glm::vec3(0, 0, 0);
    glm::vec3 bn = glm::vec3(0, -1, 0); // Bottom normal
    glm::vec3 tn = glm::vec3(0, 1, 0); // Top normal

    for (float a = 0.0; a < 2.0 * M_PI + 0.0001; a += 2.0 * M_PI / aSlices)
    {
        float a1 = a;

        glm::vec3 p1 = glm::vec3(topwidth * cos(a1), height, topwidth * sin(a1));
        glm::vec3 p2 = glm::vec3(bottomwidth * cos(a1), 0, bottomwidth * sin(a1));
        glm::vec3 pn = glm::vec3(cos(a1), 0, sin(a1));

        // Done making points and normals. Now create polygons!
        gluggNormalv(pn);
        gluggTexCoord(height, a1 / M_PI);
        gluggVertexv(p2);
        gluggTexCoord(0, a1 / M_PI);
        gluggVertexv(p1);
    }

    // Then walk around the top and bottom with fans
    gluggMode(GLUGG_TRIANGLE_FAN);
    gluggNormalv(bn);
    gluggVertexv(center);
    // Walk around edge
    for (float a = 0.0; a <= 2.0 * M_PI + 0.001; a += 2.0 * M_PI / aSlices)
    {
        glm::vec3 p = glm::vec3(bottomwidth * cos(a), 0, bottomwidth * sin(a));
        gluggVertexv(p);
    }
    // Walk around edge
    gluggMode(GLUGG_TRIANGLE_FAN); // Reset to new fan
    gluggNormalv(tn);
    gluggVertexv(top);
    for (float a = 2.0 * M_PI; a >= -0.001; a -= 2.0 * M_PI / aSlices)
    {
        glm::vec3 p = glm::vec3(topwidth * cos(a), height, topwidth * sin(a));
        gluggVertexv(p);
    }
}

void CreateTreeBranches(int count, int maxlevel, float height);

/// <summary>
/// Creates a tree with root at 
/// </summary>
/// <param name="program"></param>
/// <param name="position"></param>
/// <returns></returns>
std::pair<GLuint, int> MakeTree(GLuint program, glm::vec3 position)
{
    gluggSetPositionName("position");
    gluggSetNormalName("normal");
    gluggSetTexCoordName("in_st");

    gluggBegin(GLUGG_TRIANGLES);
    // Between gluggBegin and gluggEnd, call MakeCylinderAlt plus glugg transformations
    // to create a tree.
    gluggTranslate(position);
    MakeCylinderAlt(10, 2, 0.1, 0.15);
    CreateTreeBranches(0, 4, 2.0f);
    int count;
    GLuint id = gluggEnd(&count, program, 0);

    return std::make_pair(id, count);
}


//Callback function to create new trees when generating new chunk
struct TreeGenerator {
    unsigned int ID;
    std::vector<std::pair<unsigned int, int>>& trees;

    /// <summary>
    /// Creates a new tree inside the bounding box of min - max x and z
    /// </summary>
    void operator()(ChunkHandler& chandler, float minX, float maxX, float minZ, float maxZ) {
        glUseProgram(ID); //Activate shader 
        
        for (int i = 0; i < nroftrees; i++) {
            float x = random(minX, maxX);
            float z = random(minZ, maxZ);
            glm::vec3 offset{ 0.0f, -0.1f, 0.0f };
            glm::vec3 position = chandler.getPointOnTerrain(x, z) + offset;

            trees.push_back(MakeTree(ID, position));
        }
    }
    const int nroftrees{ 1 };
private:
    float random(float low, float high) {
        return low + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (high - low)));
    }
};

void CreateTreeBranches(int count, int maxlevel, float height) {
    if (count == maxlevel)
    {
        return;

    }
    ++count;
    int nrOfBranches = (rand() % 6) + 2;
    //gluggPushMatrix();

    gluggTranslate(0, height / count, 0);
    float angle = 2 * M_PI / nrOfBranches;

    for (int i = 0; i < nrOfBranches; ++i) {
        gluggPushMatrix();
        //float r = (double) rand() / RAND_MAX;
        gluggRotate(angle * i, 0, 1, 0);
        gluggRotate(30 * M_PI / 180, 0, 0, 1);
        MakeCylinderAlt(10, height / (count + 1), 0.1 / (1 + count), 0.1 / count);
        CreateTreeBranches(count, maxlevel, height);
        gluggPopMatrix();
    }
}

int main() {

    initialize();
    //create window
    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Procedural Terrain", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    //load opengl functions using glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    //Callback functions
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback); //recieve callback when key is pressed or released 
    glfwSetCursorPosCallback(window, mouse_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    /*** Shaders ***/
    Shader myShader{ "shaders/vertex.vert", "shaders/fragment.frag" };
    Shader cameraShader{ "shaders/cameraVertex.vert", "shaders/cameraFragment.frag" };
    Shader boundingBoxShader{ "shaders/boundingBoxVertex.vert", "shaders/boundingBoxFragment.frag" };
    Shader treeShader{ "shaders/treeVertex.vert", "shaders/treeFragment.frag" };

    glm::mat4 perspective = glm::perspective(glm::radians(45.0f), static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, 0.1f, 30.0f);
    glm::mat4 perspective2 = glm::perspective(glm::radians(45.0f), static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, 0.1f, 100.0f);

    /*** Always use the larger perspective to render camera frustum, otherwise it risk being culled in viewport ***/
    cameraShader.use();
    cameraShader.setMat4("P", perspective2);
    /*** Set camera 2 start position and rotation ***/
    camera2 = glm::translate(camera2, glm::vec3(0.0f, -5.0f, -25.0f));
    camera2 = glm::rotate(camera2, glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    
    Vertex v1{ glm::vec3{-1.0f, -1.0f, -1.0f} }, v2{ glm::vec3{1.0f, -1.0f, -1.0f} }, v3{ glm::vec3{1.0f, 1.0f, -1.0f} },
        v4{ glm::vec3{-1.0f, 1.0f, -1.0f} }, v5{ glm::vec3{-1.0f, -1.0f, 1.0f} }, v6{ glm::vec3{1.0f, -1.0f, 1.0f} },
        v7{ glm::vec3{1.0f, 1.0f, 1.0f} }, v8{ glm::vec3{-1.0f, 1.0f, 1.0f} };
    
    std::vector<Vertex> campoints{ v1, v2, v3, v4, v5, v6, v7, v8 };
    std::vector<unsigned int> camIndices{ 
        0, 1, 2, 3, 0, //near plane
        4, 5, 6, 7, 4, //far plane
        7, 3, 2, 6, 5, 1 //diagonal lines
    };

    Mesh camera1Mesh{ campoints, camIndices };

    std::vector<std::pair<GLuint, int>> trees;

    TreeGenerator treeGenerator{ treeShader.ID, trees };
    ChunkHandler chandler{gridSize, 65, 0.1f , 1.8f , treeGenerator};   // (gridSize, nrVertices, spacing, yScale)

    //OpenGL render Settings
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    /*****************
    *  Render loop   *
    *****************/

    float timer{ 0.0f };


    //Make trees 
    //int nroftrees = 5;
    //treeShader.use();
    //for (int i = 0; i < nroftrees; i++) {
    //    trees.push_back(MakeTree(myShader.ID, chandler));
    //}
    
    while (!glfwWindowShouldClose(window))
    {
        //compute delta time between frames.
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        //display fps
        if (timer <= 0.0f) {
            std::string title = "fps: " + std::to_string(1.0f / deltaTime);
            glfwSetWindowTitle(window, title.c_str());
            timer = 0.1f;
        }
        else
        {
            timer -= deltaTime;
        }
        //process inputs 
        processInput(window);
        camera1Control.pollMouse(window, SCREEN_WIDTH, SCREEN_HEIGHT);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 camera1 = camera1Control.computeCameraViewMatrix();
        glm::mat4 modelM = glm::mat4(1.0f);

        /*** Draw camera frustum ***/
        auto invproj = glm::inverse(perspective * camera1);
        cameraShader.use();
        cameraShader.setMat4("InvP", invproj);
        toggleCamera ? cameraShader.setMat4("V", camera1) : cameraShader.setMat4("V", camera2);
        camera1Mesh.draw(GL_LINE_STRIP);

        /*** Compute camera frustum points in world coordinates ***/
        std::vector<glm::vec3> worldCamPoints;
        worldCamPoints.reserve(campoints.size());
        for (int i = 0; i < campoints.size(); ++i) {
            glm::vec4 p = invproj * glm::vec4(campoints[i].position, 1.0f);
            glm::vec3 c = glm::vec3{ p.x / p.w, p.y / p.w, p.z / p.w };
            worldCamPoints.push_back(c);
        }
        auto planes = computeCameraPlanes(worldCamPoints);

        /*** Cull terrain ***/
        if (cull) {
            chandler.cullTerrain(cull);
        }
        else {
            chandler.cullTerrainChunk(planes);
        }

        /*** Update terrain chunks ***/
        chandler.updateChunks(camera1Control.getCameraPosition());

        /*** Draw terrain chunks ***/
        myShader.use();
        myShader.setMat4("M", modelM);
        toggleCamera ? myShader.setMat4("V", camera1) : myShader.setMat4("V", camera2);
        toggleCamera ? myShader.setMat4("P", perspective) : myShader.setMat4("P", perspective2);

        if(glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS)
            myShader.setBool("colorDistance", true);
        if(glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
            myShader.setBool("colorDistance", false);
        if (wireFrame) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            chandler.draw();
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        }
        else {
            chandler.draw();
        }
        treeShader.use();
        treeShader.setMat4("M", modelM);
        toggleCamera ? treeShader.setMat4("V", camera1) : treeShader.setMat4("V", camera2);
        toggleCamera ? treeShader.setMat4("P", perspective) : treeShader.setMat4("P", perspective2);
        for (int i = 0; i < trees.size(); i++) {
            glBindVertexArray(trees[i].first);	// Select VAO
            glDrawArrays(GL_TRIANGLES, 0, trees[i].second);
        }

        /*** Draw bounding boxes around  ***/
        boundingBoxShader.use();
        boundingBoxShader.setMat4("M", modelM);
        toggleCamera ? boundingBoxShader.setMat4("V", camera1) : boundingBoxShader.setMat4("V", camera2);
        toggleCamera ? boundingBoxShader.setMat4("P", perspective) : boundingBoxShader.setMat4("P", perspective2);
        if(drawbb)
            chandler.drawBoundingBox();

        //if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        //    //printmat4(invproj);
        //    //std::cout << '\n';
        //    for (int i = 0; i < campoints.size(); ++i) {
        //        glm::vec4 p = (invproj) * glm::vec4(campoints[i].position, 1.0f);
        //        glm::vec3 c = glm::vec3{ p.x / p.w, p.y / p.w, p.z / p.w };
        //        //std::cout << "point " << i+1 << ": "  << c.x << " " << c.y << " " << c.z << '\n';
        //    }
        //    //std::cout << '\n';

        //}

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
	return 0;
}


std::vector<CameraPlane> computeCameraPlanes(const std::vector<glm::vec3>& points) {
    //top plane
    glm::vec3 e1 = points[7] - points[3];
    glm::vec3 e2 = points[2] - points[3];
    CameraPlane p1{ points[3], glm::normalize(glm::cross(e2, e1)) };
    //std::cout << "top plane normal y: " << p1.normal.y << '\n';
    //bottom plane
    e1 = points[4] - points[0];
    e2 = points[1] - points[0];
    CameraPlane p2{ points[0], glm::normalize(glm::cross(e1, e2)) };
    //std::cout << "bottom plane normal y: " << p2.normal.y << '\n';
    //left plane 
    e1 = points[7] - points[3];
    e2 = points[0] - points[3];
    CameraPlane p3{ points[3], glm::normalize(glm::cross(e1, e2)) };
    //std::cout << "left plane normal x: " << p3.normal.x << '\n';
    //rightplane
    e1 = points[6] - points[2];
    e2 = points[1] - points[2];
    CameraPlane p4{ points[2], glm::normalize(glm::cross(e2, e1)) };
    //std::cout << "right plane normal x: " << p4.normal.x << '\n';
    //far plane
    e1 = points[7] - points[6];
    e2 = points[5] - points[6];
    CameraPlane p5{ points[6], glm::normalize(glm::cross(e2, e1)) };
    //std::cout << "far plane normal z: " << p5.normal.z << '\n';
    //near plane ?? 
    e1 = points[3] - points[2];
    e2 = points[1] - points[2];
    CameraPlane p6{ points[2], glm::normalize(glm::cross(e1, e2)) };
    //std::cout << "near plane normal z: " << p6.normal.z << '\n';

    //std::cout << '\n';
    //TODO check if near plane is necessary
    return std::vector<CameraPlane>{p1, p2, p3, p4, p5};
}


void initialize() {
    //list of possible settings: https://www.glfw.org/docs/latest/window.html#window_hints
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        camera1Control.moveCamera(FORWARD, deltaTime);
        updateCamera2();
    }

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        camera1Control.moveCamera(BACKWARD, deltaTime);
        updateCamera2();
    }

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        camera1Control.moveCamera(LEFT, deltaTime);
        updateCamera2();
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        camera1Control.moveCamera(RIGHT, deltaTime);
        updateCamera2();
    }

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    {
        camera1Control.moveCamera(DESCEND, deltaTime);
        updateCamera2();
    }

    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        camera1Control.moveCamera(ASCEND, deltaTime);
        updateCamera2();
    }
   
}

void updateCamera2() {
    camera2 = glm::mat4{ 1.0f };
    camera2 = glm::rotate(camera2, glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    camera2 = glm::translate(camera2, -camera1Control.getCameraPosition());
    camera2 = glm::translate(camera2, glm::vec3(0.0f, -20.0f, -15.0f));
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        toggleCamera = !toggleCamera;
    }
    //Reset camera rotation
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        camera1Control.resetCamera();
    }
    if (key == GLFW_KEY_C && action == GLFW_PRESS) {
        cull = !cull;
    }
    if (key == GLFW_KEY_V && action == GLFW_PRESS) {
        wireFrame = !wireFrame;
    }
    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        drawbb = !drawbb;
    }  
}

void mouse_position_callback(GLFWwindow* window, double xpos, double ypos) {
    //std::cout << "mouse moved: " << xpos << " " << ypos << '\n';
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        //glfwGetCursorPos(window, &START_MOUSE_X, &START_MOUSE_Y);
        camera1Control.setMouseStartPos(window);
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    std::cout << "mouse wheel scrolled " << xoffset << " " << yoffset << '\n';
}

void printmat4(const glm::mat4& mat) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            std::cout << mat[j][i] << " ";
        }
        std::cout << '\n';
    }
}