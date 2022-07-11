#include <glad/glad.h>

// to create an ImGui window in game
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

// GLFW library to create window and to manage I/O
#include <glfw/glfw3.h>

// classes developed during lab lectures to manage shaders, to load models, for FPS camera, and for physical simulation
#include <utils/shader.h>
#include <utils/camera.h>
#include <utils/model.h>
#include <utils/physics.h>

// GLM libraries for math operations
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>

// for images (textures)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

// number of lights in the scene
#define NR_LIGHTS 3

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

// WASD keys for camera class
void apply_camera_movements();

// dimensions of the game window
const unsigned int screenWidth = 1200;
const unsigned int screenHeight = 900;

// initial view, GL_TRUE stands for we are on ground and y-axis will be constant
Camera camera(glm::vec3(5.0f, 1.0f, 12.0f), GL_TRUE);

// we need to store the previous mouse position to calculate the offset with the current frame
GLfloat lastX, lastY;

// we will use these value to "pass" the cursor position to the keyboard callback, in order to determine the bullet trajectory
double cursorX, cursorY;

// for the first state of the mouse when the game starts
bool firstMouse = true;

// view and projection matrices (global because we need to use them in the keyboard callback)
glm::mat4 view, projection;

// parameters for time calculation (for animations)
GLfloat deltaTime = 0.0f;
GLfloat lastFrame = 0.0f;

// rotation angle on Y axis
GLfloat orientationY = 0.0f;
// rotation speed on Y axis for the rotation of the instanced objects in the background
GLfloat spin_speed = 30.0f;
// boolean to start/stop animated rotation on Y angle
GLboolean spinning = GL_TRUE;

// boolean to activate/deactivate wireframe rendering
GLboolean wireframe = GL_FALSE;

// index of the current shader subroutine (= 0 in the beginning)
GLuint current_subroutine = 0;
// a vector for all the shader subroutines names used and swapped in the application
vector<std::string> shaders;

// the name of the subroutines are searched in the shaders, and placed in the shaders vector (to allow shaders swapping)
void SetupShader(int shader_program);
// print on console the name of current shader subroutine
void PrintCurrentShader(int subroutine);

// load image from disk and create an OpenGL texture
GLint LoadTexture(const char* path);

vector<GLint> textureID;

// uniforms to be passed to shaders
// pointlights positions
glm::vec3 lightPositions[] = {
    glm::vec3(5.0f, 10.0f, 10.0f),
    glm::vec3(-5.0f, 10.0f, 10.0f),
    glm::vec3(5.0f, 10.0f, -10.0f),
};

// specular and ambient components
GLfloat specularColor[] = {1.0,1.0,1.0};
GLfloat ambientColor[] = {0.1,0.1,0.1};
// weights for the diffusive, specular and ambient components
GLfloat Kd = 0.8f;
GLfloat Ks = 0.5f;
GLfloat Ka = 0.1f;
// shininess coefficient for Blinn-Phong shader
GLfloat shininess = 25.0f;
// roughness index for GGX shader
GLfloat alpha = 0.2f;
// Fresnel reflectance at 0 degree (Schlik's approximation)
GLfloat F0 = 0.9f;

// dimension of the bullets (global because we need it also in the keyboard callback)
glm::vec3 ball_size = glm::vec3(0.16f, 0.16f, 0.16f);

// instance of the physics class
Physics bulletSimulation;

// we initialize an array of booleans for each keyboard key
bool keys[1024];

int repeat = 1;

// credentials of a particle
struct Particle {
    glm::vec3 Position;         // initialization position will be equal to the objects (pins and balls)
    glm::vec3 Speed;            // will be equal to the objects (pins and balls)
    glm::vec4 Color;            // will be green or white to distinguish better
    float     Life;             // each particle dies in a short time just after rendering

    // constructor
    Particle() : Position(0.0f), Speed(0.0f), Color(0.0f), Life(0.0f)
    {}
};

std::vector<Particle> particles;    // vector for all particles
int particleNum = 500;              // this can be tweaked with ImGui

// two functions to instantiate each particle after previous one's death
int lastUsedParticle = 0;
int FirstUnusedParticle();
void RespawnParticle(Particle &particle, btRigidBody &body, btTransform transform, glm::vec3 obj_size);

int main()
{
    // Initialization of OpenGL context using GLFW
    glfwInit();
    // We set OpenGL specifications required for this application
     glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    // we create the application's window
    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "Space Bowling", nullptr, nullptr);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    // we put in relation the window and the callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glfwSwapInterval(0);

    // we define the viewport dimensions
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // we enable Z test
    glEnable(GL_DEPTH_TEST);

    // main background will have dark blue color
    glClearColor(0.0f, 0.0f, 0.3f, 0.0f);

    // initialization of ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();                         // initializing
    ImGuiIO& io = ImGui::GetIO();(void)io;          // io
    ImGui::StyleColorsDark();                       // main theme will be dark
    ImGui_ImplGlfw_InitForOpenGL(window, true);     // window will be opened
    ImGui_ImplOpenGL3_Init("#version 410 core");    // must be the same version

    // VAO and VBO for the particles
    unsigned int particleVAO, particleVBO;
    float particle_quad[] = {0.0f, 0.0f, 0.0f}; // to initialize
    // set up mesh and attribute properties
    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);
    glBindVertexArray(particleVAO);
    // fill mesh buffer
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(particle_quad), particle_quad, GL_STATIC_DRAW);
    // set mesh attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
    glBindVertexArray(0);

    // vector that includes all particles
    // to store the particles that will be born and die over and over
    for (unsigned int i = 0; i < particleNum; ++i)
        particles.push_back(Particle());

    // different shaders for instanced objects, particles, and main objects
    Shader instance_shader("instance.vert", "instance.frag");
    Shader particle_shader("particle.vert", "particle.frag");
    Shader illumination_shader = Shader("13_illumination_models_ML_TX.vert", "14_illumination_models_ML_TX.frag");

    SetupShader(illumination_shader.Program);
    PrintCurrentShader(current_subroutine);

    // no model for particles because it will be drawn directly as GL_POINTS
    Model instanceModel("../../models/cube.obj");
    Model planeModel("../../models/cube.obj");
    Model pinModel("../../models/cube.obj");
    Model ballModel("../../models/sphere.obj");

    // plane has to have a little height to be a collidable
    glm::vec3 plane_pos = glm::vec3(0.0f, -1.0f, 4.0f);
    glm::vec3 plane_size = glm::vec3(2.0f, 0.1f, 11.0f);
    glm::vec3 plane_rot = glm::vec3(0.0f, 0.0f, 0.0f);

    // textures
    textureID.push_back(LoadTexture("../../textures/bowling_pin_TEX.jpg"));
    textureID.push_back(LoadTexture("../../textures/bowling_floor.jpeg"));
    textureID.push_back(LoadTexture("../../textures/bowling_ball.jpg"));

    // first initialization of the instanced objects
    int amount = 10000;                                 // this can be tweaked with ImGui
    glm::mat4* modelMatrices;                           // to store all 10000 objects
    modelMatrices = new glm::mat4[amount];
    srand(static_cast<unsigned int>(glfwGetTime()));    // initialize random function
    float offset = 6.0f;                                // random constant for their lineup
    for (unsigned int i = 0; i < amount; i++)       // to create the cross (X)
    {
        glm::mat4 model = glm::mat4(1.0f);
        // displacement value creates randomness for each instanced object
        float displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
        // to create the letter X, each object goes left if even or right if odd
        // they meet at the middle and moves apart towards the end
        float x = (i % 2 == 0) ? -90.0f + (0.0165f * i) + displacement + 5.0f : 90.0f - (0.0165f * i) - displacement - 5.0f;
        displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
        // on a bigger picture, it is setting the height of all objects on y-axis
        float y = -33.0f + (0.0165f * i) + displacement;
        // whole shape's thickness
        float z = displacement * 2.0f;

        //scaling each object between 0.1f-0.5f
        float scale = static_cast<float>((rand() % 40) / 100.0 + 0.1);
        // rotation to create randomness on each object
        float rotAngle = static_cast<float>((rand() % 360));

        model = glm::translate(model, glm::vec3(x, y, z));
        model = glm::scale(model, glm::vec3(scale));
        model = glm::rotate(model, rotAngle, glm::vec3(0.4f, 0.6f, 0.8f));

        modelMatrices[i] = model;
    }

    // reserving them a buffer
    unsigned int buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, amount * sizeof(glm::mat4), &modelMatrices[0], GL_STATIC_DRAW);

    // setting their matrices into their instance vertex from their mesh class
    for (unsigned int i = 0; i < instanceModel.meshes.size(); i++)
    {
        unsigned int instanceVAO = instanceModel.meshes[i].VAO;
        glBindVertexArray(instanceVAO);
        // the reason they are 3,4,5,6 is because it is located at 3
        // at its vertex shader, and it is mat4 (4 vec4)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4)));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * sizeof(glm::vec4)));
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * sizeof(glm::vec4)));

        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
        glVertexAttribDivisor(5, 1);
        glVertexAttribDivisor(6, 1);

        glBindVertexArray(0);
    }

    // creating three planes with mass=0 to not being a movable object
    btRigidBody* plane = bulletSimulation.createRigidBody(BOX,plane_pos,plane_size,plane_rot,0.0f,0.2f,0.2f);
    btRigidBody* plane2 = bulletSimulation.createRigidBody(BOX,(plane_pos + glm::vec3(5.0f,0.0f,0.0f)),plane_size,plane_rot,0.0f,0.2f,0.2f);
    btRigidBody* plane3 = bulletSimulation.createRigidBody(BOX,(plane_pos + glm::vec3(10.0f,0.0f,0.0f)),plane_size,plane_rot,0.0f,0.2f,0.2f);

    // we set the maximum delta time for the update of the physical simulation
    GLfloat maxSecPerFrame = 1.0f / 60.0f;

    // First 10 bowling pins to be in a triangle shape
    GLint num_rows = 4;
    // total number of the pins per plane
    GLint total_pins = num_rows * (num_rows + 1) / 2;
    GLint i,j;
    // position of the pin
    glm::vec3 pin_pos;
    // dimension of the pin
    glm::vec3 pin_size = glm::vec3(0.12f, 0.38f, 0.12f);
    // placeholder rotation
    glm::vec3 pin_rot = glm::vec3(0.0f, 0.0f, 0.0f);
    // rigid body
    btRigidBody* pin;

    // creating triangle shape for the first 10 pins (their (x, z) coordinates):
    //     (-0.75f, -3.0f)    (-0.25f, -3.0f)   (0.25f, -3.0f)    (0.75f, -3.0f)
    //              (-0.5f, -2.5f)   (0.0f, -2.5f)    (0.5f, -2.5f)
    //                      (-0.25f, -2.0f)   (0.25f, -2.0f)
    //                              (0.0f, -1.5f)

    for (int h = 0; h < 3; h++)
    {
        for(i = 0; i < num_rows; i++)
        {
            for(j = 0; j < (num_rows - i); j++ ) // to make it decrease row by row, it has to be equal to i
            {
                pin_pos = glm::vec3((h * 5.0f + ((-0.75f + 0.25f * i) + 0.5f * j)), 0.0f, (i * 0.5 - 3.0f));

                // bowling pins are created with masses 1.5, 2.5, and 3.5, respectively
                pin = bulletSimulation.createRigidBody(BOX,pin_pos,pin_size,pin_rot,1.5f+float(h),0.5f,0.5f);
            }
        }
    }

    // Projection matrix: FOV angle, aspect ratio, near and far planes
    projection = glm::perspective(45.0f, (float)screenWidth/(float)screenHeight, 0.1f, 10000.0f);

    // helper boolean value to set the cursor to the center at the beginning of the game
    bool gameStarted = false;

    // Model and Normal transformation matrices for the objects in the scene: we set to identity
    glm::mat4 instanceModelMatrix = glm::mat4(1.0f);
    glm::mat4 particleModelMatrix = glm::mat4(1.0f);
    glm::mat4 planeModelMatrix = glm::mat4(1.0f);
    glm::mat3 planeNormalMatrix = glm::mat3(1.0f);
    glm::mat4 objModelMatrix = glm::mat4(1.0f);
    glm::mat3 objNormalMatrix = glm::mat3(1.0f);

    while (!glfwWindowShouldClose(window))
    {
        // we determine the time passed from the beginning
        // and we calculate time difference between current frame rendering and the previous one
        GLfloat currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Check is an I/O event is happening
        glfwPollEvents();
        // we apply FPS camera movements
        apply_camera_movements();

        // configure transformation matrices
        view = camera.GetViewMatrix();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // we set the rendering mode
        if (wireframe) {
            // Draw in wireframe
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
        else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        // if animated rotation is activated, than we increment the rotation angle using delta time and the rotation speed parameter
        if (spinning)
            orientationY+=(deltaTime*spin_speed);
        
        // we set the viewport for the final rendering step
        glViewport(0, 0, width, height);

        // when game starts, this while loop will work once
        // and attach the cursor to the center of the window
        while(!gameStarted)
        {
            glfwSetCursorPos(window, screenWidth/2, screenHeight/2);
            gameStarted = true;
        }

        bulletSimulation.dynamicsWorld->stepSimulation((deltaTime < maxSecPerFrame ? deltaTime : maxSecPerFrame),10);

        /////////////////// PLANE ////////////////////////////////////////////////
        illumination_shader.Use();
        // we search inside the Shader Program the name of the subroutine, and we get the numerical index
        GLuint index = glGetSubroutineIndex(illumination_shader.Program, GL_FRAGMENT_SHADER, shaders[current_subroutine].c_str());
        // we activate the subroutine using the index (this is where shaders swapping happens)
        glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &index);

        // we determine the position in the Shader Program of the uniform variables
        GLint textureLocation = glGetUniformLocation(illumination_shader.Program, "tex");
        GLint repeatLocation = glGetUniformLocation(illumination_shader.Program, "repeat");
        GLint matAmbientLocation = glGetUniformLocation(illumination_shader.Program, "ambientColor");
        GLint matSpecularLocation = glGetUniformLocation(illumination_shader.Program, "specularColor");
        GLint kaLocation = glGetUniformLocation(illumination_shader.Program, "Ka");
        GLint kdLocation = glGetUniformLocation(illumination_shader.Program, "Kd");
        GLint ksLocation = glGetUniformLocation(illumination_shader.Program, "Ks");
        GLint shineLocation = glGetUniformLocation(illumination_shader.Program, "shininess");
        GLint alphaLocation = glGetUniformLocation(illumination_shader.Program, "alpha");
        GLint f0Location = glGetUniformLocation(illumination_shader.Program, "F0");

        // we assign the value to the uniform variables
        glUniform3fv(matAmbientLocation, 1, ambientColor);
        glUniform3fv(matSpecularLocation, 1, specularColor);
        glUniform1f(shineLocation, shininess);
        glUniform1f(alphaLocation, alpha);
        glUniform1f(f0Location, F0);
        // for the plane, we make it mainly Lambertian, by setting at 0 the specular component
        glUniform1f(kaLocation, 0.0f);
        glUniform1f(kdLocation, 0.6f);
        glUniform1f(ksLocation, 0.0f);

        // we pass projection and view matrices to the Shader Program
        glUniformMatrix4fv(glGetUniformLocation(illumination_shader.Program, "projectionMatrix"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(illumination_shader.Program, "viewMatrix"), 1, GL_FALSE, glm::value_ptr(view));

        // we pass each light position to the shader
        for (GLuint i = 0; i < NR_LIGHTS; i++)
        {
            string number = to_string(i);
            glUniform3fv(glGetUniformLocation(illumination_shader.Program, ("lights[" + number + "]").c_str()), 1, glm::value_ptr(lightPositions[i]));
        }

        // texture for plane
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textureID[1]);
        glUniform1i(textureLocation, 1);
        glUniform1f(repeatLocation, 1.0f);

        int planeNum = 3;   // same number we created the rigidbodies
        for (int i = 0; i < planeNum; i++)
        {
            // we create the transformation matrix
            // we reset to identity at each frame
            planeModelMatrix = glm::mat4(1.0f);
            planeNormalMatrix = glm::mat3(1.0f);
            planeModelMatrix = glm::translate(planeModelMatrix, glm::vec3(plane_pos.x + i*5, plane_pos.y, plane_pos.z));
            planeModelMatrix = glm::scale(planeModelMatrix, plane_size);
            planeNormalMatrix = glm::inverseTranspose(glm::mat3(view*planeModelMatrix));
            glUniformMatrix4fv(glGetUniformLocation(illumination_shader.Program, "modelMatrix"), 1, GL_FALSE, glm::value_ptr(planeModelMatrix));
            glUniformMatrix3fv(glGetUniformLocation(illumination_shader.Program, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(planeNormalMatrix));

            // we render the plane
            planeModel.Draw();
        }

        /////////////////// OBJECTS (PINS + BALL) ////////////////////////////////////////////////
        // array of 16 floats = "native" matrix of OpenGL.
        // We need it as an intermediate data structure to "convert" the Bullet matrix to a GLM matrix
        GLfloat matrix[16];
        btTransform transform;

        // we need two variables to manage the rendering of both pins and bullets
        glm::vec3 obj_size;
        Model* objectModel;

        // we ask Bullet to provide the total number of Rigid Bodies in the scene
        // at the beginning they are 26 (the static plane + the falling pins)
        int num_cobjs = bulletSimulation.dynamicsWorld->getNumCollisionObjects();

        // we cycle among all the Rigid Bodies (starting from 1 to avoid the plane)
        for (i=planeNum; i<num_cobjs;i++)
        {
            // the first 30 objects are the falling pins
            if (i <= planeNum * (total_pins + (planeNum - 1*planeNum)) + planeNum - 1)
            {
                // we point objectModel to the pin
                objectModel = &pinModel;
                obj_size = pin_size;

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, textureID[0]);
                glUniform1i(textureLocation, 0);
                glUniform1f(repeatLocation, repeat);
            }
            // over 30 (number of pins), there are bullets
            else
            {
                // we point objectModel to the ball
                objectModel = &ballModel;
                obj_size = ball_size;

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, textureID[2]);
                glUniform1i(textureLocation, 0);
                glUniform1f(repeatLocation, repeat);
            }

            // we take the Collision Object from the list
            btCollisionObject* obj = bulletSimulation.dynamicsWorld->getCollisionObjectArray()[i];

            // we upcast it in order to use the methods of the main class RigidBody
            btRigidBody* body = btRigidBody::upcast(obj);

            // we take the transformation matrix of the rigid boby, as calculated by the physics engine
            body->getMotionState()->getWorldTransform(transform);

            // we convert the Bullet matrix (transform) to an array of floats
            transform.getOpenGLMatrix(matrix);
            // we reset to identity at each frame
            objModelMatrix = glm::mat4(1.0f);
            objNormalMatrix = glm::mat3(1.0f);

            // if it has already fallen from the plane
            // no need to make them fall to infinity
            // this if statement to not render after its y-axis is -7.0f
            if (transform.getOrigin().getY() >= -7.0f)
            {
                // putting particles before the objects so that
                // main objects will be rendered after the particles
                // like creating a hierarchical system
                int nr_new_particles = 2;
                // add new particles
                for (int i = 0; i < nr_new_particles; ++i)
                {
                    // finding dead particles and respawning new ones
                    int unusedParticle = FirstUnusedParticle();
                    RespawnParticle(particles[unusedParticle], *body, transform, obj_size);
                }
                // update all other particles
                for (int i = 0; i < particles.size(); ++i)
                {
                    Particle &p = particles[i];
                    p.Life -= deltaTime;            // reduce its lifetime
                    if (p.Life > 0.0f)          // if particle is alive
                    {
                        p.Position -= p.Speed * deltaTime;
                        p.Color.a -= deltaTime * 2.5f;
                    }
                }
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // creating a blend effect on particles
                particle_shader.Use();
                for (Particle particle : particles) // for each particle
                {
                    if (particle.Life > 0.0f)       // if they are alive
                    {
                        particleModelMatrix = glm::mat4(1.0f);
                        particleModelMatrix = glm::translate(particleModelMatrix, particle.Position);
                        glUniformMatrix4fv(glGetUniformLocation(particle_shader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
                        glUniformMatrix4fv(glGetUniformLocation(particle_shader.Program, "view"), 1, GL_FALSE, glm::value_ptr(view));
                        glUniformMatrix4fv(glGetUniformLocation(particle_shader.Program, "modelMatrix"), 1, GL_FALSE, glm::value_ptr(particleModelMatrix));
                        glUniform4fv(glGetUniformLocation(particle_shader.Program, "color"), 1, glm::value_ptr(particle.Color));
                        // drawing the particles
                        glBindVertexArray(particleVAO);
                        glEnable(GL_POINT_SIZE);
                        glPointSize(20);            // size of the particles
                        glDrawArrays(GL_POINTS, 0, 3);
                        glBindVertexArray(0);
                    }
                }
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // finishing the blending effect

                // drawing the objects (pins or balls)
                illumination_shader.Use();
                // we search inside the Shader Program the name of the subroutine, and we get the numerical index
                index = glGetSubroutineIndex(illumination_shader.Program, GL_FRAGMENT_SHADER, shaders[current_subroutine].c_str());
                // we activate the subroutine using the index (this is where shaders swapping happens)
                glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &index);

                objModelMatrix = glm::make_mat4(matrix) * glm::scale(objModelMatrix, obj_size);
                objNormalMatrix = glm::inverseTranspose(glm::mat3(view*objModelMatrix));
                glUniformMatrix4fv(glGetUniformLocation(illumination_shader.Program, "modelMatrix"), 1, GL_FALSE, glm::value_ptr(objModelMatrix));
                glUniformMatrix3fv(glGetUniformLocation(illumination_shader.Program, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(objNormalMatrix));

                objectModel->Draw();
                // we "reset" the matrix
                objModelMatrix = glm::mat4(1.0f);
            }
            // if its y-axis value is below -7.0f
            // its rigid body should also be destroyed
            // since it will not be rendered
            else
            {
                body->~btRigidBody();
            }
        }

        /////////////////// INSTANCED OBJECTS ////////////////////////////////////////////////
        instance_shader.Use();
        // we reset to identity at each frame
        instanceModelMatrix = glm::mat4(1.0f);
        instanceModelMatrix = glm::translate(instanceModelMatrix, glm::vec3(0.0f, 0.0f, -50.0f));
        instanceModelMatrix = glm::rotate(instanceModelMatrix, glm::radians(orientationY), glm::vec3(0.0f, 0.0f, 1.0f));
        glUniformMatrix4fv(glGetUniformLocation(instance_shader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(instance_shader.Program, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(instance_shader.Program, "modelMatrix"), 1, GL_FALSE, glm::value_ptr(instanceModelMatrix));

        // to create nice color flow from red to blue
        float dynamicRed = abs(sin(currentFrame/2));
        float dynamicBlue = abs(cos(currentFrame/2));
        glUniform4f(glGetUniformLocation(instance_shader.Program, "color"), dynamicRed, 0.0f, dynamicBlue, 1.0f);

        // drawing the instanced objects
        instance_shader.Use();
        for (unsigned int i = 0; i < instanceModel.meshes.size(); i++)
        {
            glBindVertexArray(instanceModel.meshes[i].VAO);
            glDrawElementsInstanced(GL_TRIANGLES, static_cast<unsigned int>(instanceModel.meshes[i].indices.size()), GL_UNSIGNED_INT, 0, amount);
            glBindVertexArray(0);
        }

        // ImGui window creation and its parameters
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Bowling Game"); 
        ImGui::SliderInt(" ##1", &amount, 100, 10000, "Instance Amount = %.3f");
        ImGui::SliderInt(" ##2", &particleNum, 10, 500, "Particle Amount = %.3f");
        ImGui::ShowMetricsWindow();
        ImGui::End();
        //ImGui::ShowDemoWindow();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // when I exit from the graphics loop, it is because the application is closing
    // we delete the Shader Programs
    illumination_shader.Delete();
    particle_shader.Delete();
    instance_shader.Delete();
    // we delete the data of the physical simulation
    bulletSimulation.Clear();

    glfwTerminate();
    return 0;
}

int FirstUnusedParticle()
{
    // to increase the efficiency, first it is searched from the last used particle
    // because it takes shorter time to find
    for (int i = lastUsedParticle; i < particleNum; ++i)
    {
        if (particles[i].Life <= 0.0f)
        {
            lastUsedParticle = i;
            return i;
        }
    }
    // if the for loop above does not work, we do a usual linear search
    for (int i = 0; i < lastUsedParticle; ++i)
    {
        if (particles[i].Life <= 0.0f)
        {
            lastUsedParticle = i;
            return i;
        }
    }
    // if turns out that all of them are alive
    lastUsedParticle = 0;
    return 0;
}

void RespawnParticle(Particle &particle, btRigidBody &body, btTransform transform, glm::vec3 obj_size)
{
    // Current position of the ball
    glm::vec3 pos = glm::vec3(transform.getOrigin().getX(), transform.getOrigin().getY(), transform.getOrigin().getZ());
    // Current speed of the ball
    glm::vec3 speed = glm::vec3(body.getLinearVelocity().getX(), body.getLinearVelocity().getY(), body.getLinearVelocity().getZ());

    float rColor = 0.5f + ((rand() % 100) / 100.0f);
    particle.Position = pos;
    // if the object is a ball, then it will be green. If it is a pin, then it will be white
    particle.Color = (obj_size == ball_size) ? glm::vec4(0.0f, rColor, 0.0f, 1.0f) : glm::vec4(rColor, rColor, rColor, 1.0f);
    particle.Life = 1.0f;
    particle.Speed = speed * 0.1f;
}

///////////////////////////////////////////
// The function parses the content of the Shader Program, searches for the Subroutine type names,
// the subroutines implemented for each type, print the names of the subroutines on the terminal, and add the names of
// the subroutines to the shaders vector, which is used for the shaders swapping
void SetupShader(int program)
{
    int maxSub,maxSubU,countActiveSU;
    GLchar name[256];
    int len, numCompS;

    // global parameters about the Subroutines parameters of the system
    glGetIntegerv(GL_MAX_SUBROUTINES, &maxSub);
    glGetIntegerv(GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS, &maxSubU);
    std::cout << "Max Subroutines:" << maxSub << " - Max Subroutine Uniforms:" << maxSubU << std::endl;

    // get the number of Subroutine uniforms (only for the Fragment shader, due to the nature of the exercise)
    // it is possible to add similar calls also for the Vertex shader
    glGetProgramStageiv(program, GL_FRAGMENT_SHADER, GL_ACTIVE_SUBROUTINE_UNIFORMS, &countActiveSU);

    // print info for every Subroutine uniform
    for (int i = 0; i < countActiveSU; i++) {

        // get the name of the Subroutine uniform (in this example, we have only one)
        glGetActiveSubroutineUniformName(program, GL_FRAGMENT_SHADER, i, 256, &len, name);
        // print index and name of the Subroutine uniform
        std::cout << "Subroutine Uniform: " << i << " - name: " << name << std::endl;

        // get the number of subroutines
        glGetActiveSubroutineUniformiv(program, GL_FRAGMENT_SHADER, i, GL_NUM_COMPATIBLE_SUBROUTINES, &numCompS);

        // get the indices of the active subroutines info and write into the array s
        int *s =  new int[numCompS];
        glGetActiveSubroutineUniformiv(program, GL_FRAGMENT_SHADER, i, GL_COMPATIBLE_SUBROUTINES, s);
        std::cout << "Compatible Subroutines:" << std::endl;

        // for each index, get the name of the subroutines, print info, and save the name in the shaders vector
        for (int j=0; j < numCompS; ++j) {
            glGetActiveSubroutineName(program, GL_FRAGMENT_SHADER, s[j], 256, &len, name);
            std::cout << "\t" << s[j] << " - " << name << "\n";
            shaders.push_back(name);
        }
        std::cout << std::endl;

        delete[] s;
    }
}

//////////////////////////////////////////
// we print on console the name of the currently used shader subroutine
void PrintCurrentShader(int subroutine)
{
    std::cout << "Current shader subroutine: " << shaders[subroutine]  << std::endl;
}

//////////////////////////////////////////
// we load the image from disk and we create an OpenGL texture
GLint LoadTexture(const char* path)
{
    GLuint textureImage;
    int w, h, channels;
    unsigned char* image;
    image = stbi_load(path, &w, &h, &channels, STBI_rgb);

    if (image == nullptr)
        std::cout << "Failed to load texture!" << std::endl;

    glGenTextures(1, &textureImage);
    glBindTexture(GL_TEXTURE_2D, textureImage);
    // 3 channels = RGB ; 4 channel = RGBA
    if (channels==3)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    else if (channels==4)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);
    // we set how to consider UVs outside [0,1] range
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // we set the filtering for minification and magnification
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_NEAREST);

    // we free the memory once we have created an OpenGL texture
    stbi_image_free(image);

    // we set the binding to 0 once we have finished
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureImage;
}

//////////////////////////////////////////
// If one of the WASD keys is pressed, the camera is moved accordingly (the code is in utils/camera.h)
void apply_camera_movements()
{
    if(keys[GLFW_KEY_W])
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if(keys[GLFW_KEY_S])
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if(keys[GLFW_KEY_A])
        camera.ProcessKeyboard(LEFT, deltaTime);
    if(keys[GLFW_KEY_D])
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

//////////////////////////////////////////
// callback for keyboard events
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    GLuint new_subroutine;

    // if ESC is pressed, we close the application
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    // if L is pressed, we activate/deactivate wireframe rendering of models
    if(key == GLFW_KEY_L && action == GLFW_PRESS)
        wireframe=!wireframe;
    
    // if P is pressed, we start/stop the animated rotation of models
    if(key == GLFW_KEY_P && action == GLFW_PRESS)
        spinning=!spinning;

    // pressing a key number, we change the shader applied to the models
    // if the key is between 1 and 9, we proceed and check if the pressed key corresponds to
    // a valid subroutine
    if((key >= GLFW_KEY_1 && key <= GLFW_KEY_9) && action == GLFW_PRESS)
    {
        // "1" to "9" -> ASCII codes from 49 to 59
        // we subtract 48 (= ASCII CODE of "0") to have integers from 1 to 9
        // we subtract 1 to have indices from 0 to 8
        new_subroutine = (key-'0'-1);
        // if the new index is valid ( = there is a subroutine with that index in the shaders vector),
        // we change the value of the current_subroutine variable
        // NB: we can just check if the new index is in the range between 0 and the size of the shaders vector,
        // avoiding to use the std::find function on the vector
        if (new_subroutine<shaders.size())
        {
            current_subroutine = new_subroutine;
            PrintCurrentShader(current_subroutine);
        }
    }

    ///////
    /// BULLET MANAGEMENT (SPACE KEY)
    // if space is pressed, we "shoot" a bullet in the scene
    // the initial trajectory of the bullet is given by a vector from the position of the camera to the mouse cursor position, which must be converted from Viewport Coordinates back to World Coordinate
    btVector3 impulse;
    // we need a initial rotation, even if useless for a ball
    glm::vec3 rot = glm::vec3(10.0f, 0.0f, 3.0f);
    glm::vec4 shoot;
    // initial Speed of the bullet
    GLfloat shootInitialSpeed = 40.0f;
    // rigid body of the bullet
    btRigidBody* ball;
    // matrix for the inverse matrix of view and projection
    glm::mat4 unproject;

    // if space is pressed
    if(key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        // Bowling ball is created with a realistic mass which is 2.85 kg (average mass IRL)
        // y-axis value is -0.6 to create the effect of sending the ball close to ground as in real-life
        ball = bulletSimulation.createRigidBody(SPHERE,glm::vec3(camera.Position.x, -0.6f, camera.Position.z),ball_size,rot,2.85f,0.2f,0.2f);

        // we must retro-project the coordinates of the mouse pointer, in order to have a point in world coordinate to be used to determine a vector from the camera (= direction and orientation of the bullet)
        // we convert the cursor position (taken from the mouse callback) from Viewport Coordinates to Normalized Device Coordinate (= [-1,1] in both coordinates)
        shoot.x = (cursorX/screenWidth) * 2.0f - 1.0f;
        shoot.y = -(cursorY/screenHeight) * 2.0f + 1.0f; // Viewport Y coordinates are from top-left corner to the bottom
        // we need a 3D point, so we set a minimum value to the depth with respect to camera position
        shoot.z = 1.0f;
        // w = 1.0 because we are using homogeneous coordinates
        shoot.w = 1.0f;

        // we determine the inverse matrix for the projection and view transformations
        unproject = glm::inverse(projection * view);

        // we convert the position of the cursor from NDC to world coordinates, and we multiply the vector by the initial speed
        shoot = glm::normalize(unproject * shoot) * shootInitialSpeed;

        // we apply the impulse and shoot the bullet in the scene
        // N.B.) the graphical aspect of the bullet is treated in the rendering loop
        impulse = btVector3(shoot.x, shoot.y, shoot.z);
        ball->applyCentralImpulse(impulse);
    }

    // we keep trace of the pressed keys
    // with this method, we can manage 2 keys pressed at the same time:
    // many I/O managers often consider only 1 key pressed at the time (the first pressed, until it is released)
    // using a boolean array, we can then check and manage all the keys pressed at the same time
    if(action == GLFW_PRESS)
        keys[key] = true;
    else if(action == GLFW_RELEASE)
        keys[key] = false;
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

//////////////////////////////////////////
// callback for mouse events
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
      // we move the camera view following the mouse cursor
      // we calculate the offset of the mouse cursor from the position in the last frame
      // when rendering the first frame, we do not have a "previous state" for the mouse, so we set the previous state equal to the initial values (thus, the offset will be = 0)

    if(firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    // we save the current cursor position in 2 global variables, in order to use the values in the keyboard callback function
    cursorX = xpos;
    cursorY = ypos;

    // offset of mouse cursor position
    GLfloat xoffset = xpos - lastX;
    GLfloat yoffset = lastY - ypos;

    // the new position will be the previous one for the next frame
    lastX = xpos;
    lastY = ypos;

    // we pass the offset to the Camera class instance in order to update the rendering
    camera.ProcessMouseMovement(xoffset, yoffset);
}