#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>
#include <thread>
#include <atomic>
#include <chrono>

#include <GL/glew.h>
//#include <OpenGL/gl3.h>   // The GL Header File
#include <GLFW/glfw3.h> // The GLFW header
#include <glm/glm.hpp> // GL Math library header
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp> 
#include <ft2build.h>
#include FT_FREETYPE_H

#define BUFFER_OFFSET(i) ((char*)NULL + (i))
#define FRONT 0
#define RIGHT 1
#define BACK 2
#define LEFT 3

using namespace std;

GLuint gProgram[3];
int gWidth = 600, gHeight = 1000;
GLuint gVertexAttribBuffer, gTextVBO, gIndexBuffer;
GLuint gTex2D;
int gVertexDataSizeInBytes, gNormalDataSizeInBytes;
int gTriangleIndexDataSizeInBytes, gLineIndexDataSizeInBytes;

GLint modelingMatrixLoc[2];
GLint viewingMatrixLoc[2];
GLint projectionMatrixLoc[2];
GLint eyePosLoc[2];
GLint lightPosLoc[2];
GLint kdLoc[2];

glm::mat4 projectionMatrix;
glm::mat4 viewingMatrix;
//glm::mat4 modelingMatrix = glm::translate(glm::mat4(1.f), glm::vec3(-0.5, -0.5, -0.5));
glm::mat4 modelingMatrix = glm::mat4(1.f);
glm::vec3 eyePos = glm::vec3(0, 3, 30);
glm::vec3 lightPos = glm::vec3(0, 0, 7);

glm::vec3 kdGround(0.334, 0.288, 0.635); // this is the ground color in the demo
glm::vec3 kdCubes(0.86, 0.11, 0.31);

int activeProgramIndex = 0;

bool stop_timer = false;
bool timer_alert = false;
bool game_over;
double speedMultiplier = 1.0;
int viewing_mode;
int points;
bool new_cube;
bool rotating = false;
float target_rot_angle;
float current_rot_angle = 0;
float rotation_step = 5.0; //degrees per frame
const glm::vec3 y_axis(0.0f, 1.0f, 0.0f);
std::string lastPressedKey = ""; // Store the last pressed key
bool s_pressed = false;

// Holds all state information relevant to a character as loaded using FreeType
struct Character {
    GLuint TextureID;   // ID handle of the glyph texture
    glm::ivec2 Size;    // Size of glyph
    glm::ivec2 Bearing;  // Offset from baseline to left/top of glyph
    GLuint Advance;    // Horizontal offset to advance to next glyph
};

std::map<GLchar, Character> Characters;

struct Cube {
    glm::vec3 position;

    Cube(float x, float y, float z) : position(x, y, z) {}
};

std::vector<Cube> cubes;

int mod(int a, int b) {
    int result = a % b;
    return (result < 0) ? (result + b) : result;
}

void controllableTimer() {
    while (stop_timer==false) {
        for (int i = 0; i < 10 && !stop_timer; ++i)
        {
            if (speedMultiplier == 0) std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(100)));
            else std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(100 / speedMultiplier)));
        }
        //std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(1000 / speedMultiplier)));
        if (speedMultiplier != 0) timer_alert = true;
    }
}

// For reading GLSL files
bool ReadDataFromFile(
    const string& fileName, ///< [in]  Name of the shader file
    string&       data)     ///< [out] The contents of the file
{
    fstream myfile;

    // Open the input 
    myfile.open(fileName.c_str(), std::ios::in);

    if (myfile.is_open())
    {
        string curLine;

        while (getline(myfile, curLine))
        {
            data += curLine;
            if (!myfile.eof())
            {
                data += "\n";
            }
        }

        myfile.close();
    }
    else
    {
        return false;
    }

    return true;
}

string get_mode_string(int viewing_mode)
{
    switch (viewing_mode)
    {
    case 0:
        return "Front";
    case 1:
        return "Right";
    case 2:
        return "Back";
    case 3:
        return "Left";
    }

    return "False mode value: " + to_string(viewing_mode);
}

GLuint createVS(const char* shaderName)
{
    string shaderSource;

    string filename(shaderName);
    if (!ReadDataFromFile(filename, shaderSource))
    {
        cout << "Cannot find file name: " + filename << endl;
        exit(-1);
    }

    GLint length = shaderSource.length();
    const GLchar* shader = (const GLchar*) shaderSource.c_str();

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &shader, &length);
    glCompileShader(vs);

    char output[1024] = {0};
    glGetShaderInfoLog(vs, 1024, &length, output);
    printf("VS compile log: %s\n", output);

	return vs;
}

GLuint createFS(const char* shaderName)
{
    string shaderSource;

    string filename(shaderName);
    if (!ReadDataFromFile(filename, shaderSource))
    {
        cout << "Cannot find file name: " + filename << endl;
        exit(-1);
    }

    GLint length = shaderSource.length();
    const GLchar* shader = (const GLchar*) shaderSource.c_str();

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &shader, &length);
    glCompileShader(fs);

    char output[1024] = {0};
    glGetShaderInfoLog(fs, 1024, &length, output);
    printf("FS compile log: %s\n", output);

	return fs;
}

void initFonts(int windowWidth, int windowHeight)
{
    // Set OpenGL options
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::mat4 projection = glm::ortho(0.0f, static_cast<GLfloat>(windowWidth), 0.0f, static_cast<GLfloat>(windowHeight));
    glUseProgram(gProgram[2]);
    glUniformMatrix4fv(glGetUniformLocation(gProgram[2], "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // FreeType
    FT_Library ft;
    // All functions return a value different than 0 whenever an error occurred
    if (FT_Init_FreeType(&ft))
    {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
    }

    // Load font as face
    FT_Face face;
    if (FT_New_Face(ft, "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", 0, &face))
    //if (FT_New_Face(ft, "/usr/share/fonts/truetype/gentium-basic/GenBkBasR.ttf", 0, &face)) // you can use different fonts
    {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
    }

    // Set size to load glyphs as
    FT_Set_Pixel_Sizes(face, 0, 48);

    // Disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 

    // Load first 128 characters of ASCII set
    for (GLubyte c = 0; c < 128; c++)
    {
        // Load character glyph 
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
            continue;
        }
        // Generate texture
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RED,
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows,
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer
                );
        // Set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // Now store character for later use
        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            (GLuint) face->glyph->advance.x
        };
        Characters.insert(std::pair<GLchar, Character>(c, character));
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    // Destroy FreeType once we're finished
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    //
    // Configure VBO for texture quads
    //
    glGenBuffers(1, &gTextVBO);
    glBindBuffer(GL_ARRAY_BUFFER, gTextVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void initShaders()
{
	// Create the programs

    gProgram[0] = glCreateProgram(); //returns program id
	gProgram[1] = glCreateProgram();
	gProgram[2] = glCreateProgram();

	// Create the shaders for both programs

    GLuint vs1 = createVS("vert.glsl"); // for cube shading
    GLuint fs1 = createFS("frag.glsl");

	GLuint vs2 = createVS("vert2.glsl"); // for border shading
	GLuint fs2 = createFS("frag2.glsl");

	GLuint vs3 = createVS("vert_text.glsl");  // for text shading
	GLuint fs3 = createFS("frag_text.glsl");

	// Attach the shaders to the programs

	glAttachShader(gProgram[0], vs1);
	glAttachShader(gProgram[0], fs1);

	glAttachShader(gProgram[1], vs2);
	glAttachShader(gProgram[1], fs2);

	glAttachShader(gProgram[2], vs3);
	glAttachShader(gProgram[2], fs3);

	// Link the programs

    for (int i = 0; i < 3; ++i)
    {
        glLinkProgram(gProgram[i]);
        GLint status;
        glGetProgramiv(gProgram[i], GL_LINK_STATUS, &status);

        if (status != GL_TRUE)
        {
            cout << "Program link failed: " << i << endl;
            exit(-1);
        }
    }


	// Get the locations of the uniform variables from both programs

	for (int i = 0; i < 2; ++i)
	{
		modelingMatrixLoc[i] = glGetUniformLocation(gProgram[i], "modelingMatrix");
		viewingMatrixLoc[i] = glGetUniformLocation(gProgram[i], "viewingMatrix");
		projectionMatrixLoc[i] = glGetUniformLocation(gProgram[i], "projectionMatrix");
		eyePosLoc[i] = glGetUniformLocation(gProgram[i], "eyePos");
		lightPosLoc[i] = glGetUniformLocation(gProgram[i], "lightPos");
		kdLoc[i] = glGetUniformLocation(gProgram[i], "kd");

        glUseProgram(gProgram[i]);
        //give values to the uniform variables at program i:
        glUniformMatrix4fv(modelingMatrixLoc[i], 1, GL_FALSE, glm::value_ptr(modelingMatrix));
        glUniform3fv(eyePosLoc[i], 1, glm::value_ptr(eyePos));
        glUniform3fv(lightPosLoc[i], 1, glm::value_ptr(lightPos));
        glUniform3fv(kdLoc[i], 1, glm::value_ptr(kdCubes));
	}
}

// VBO setup for drawing a cube and its borders
void initVBO()
{
    GLuint vao;
    glGenVertexArrays(1, &vao);
    assert(vao > 0);
    glBindVertexArray(vao);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	assert(glGetError() == GL_NONE);

	glGenBuffers(1, &gVertexAttribBuffer);
	glGenBuffers(1, &gIndexBuffer);

	assert(gVertexAttribBuffer > 0 && gIndexBuffer > 0);

	glBindBuffer(GL_ARRAY_BUFFER, gVertexAttribBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIndexBuffer);

    GLuint indices[] = { //triangles
        0, 1, 2, // front
        3, 0, 2, // front
        4, 7, 6, // back
        5, 4, 6, // back
        0, 3, 4, // left
        3, 7, 4, // left
        2, 1, 5, // right
        6, 2, 5, // right
        3, 2, 7, // top
        2, 6, 7, // top
        0, 4, 1, // bottom
        4, 5, 1  // bottom
    };

    GLuint indicesLines[] = { //faces
        7, 3, 2, 6, // top
        4, 5, 1, 0, // bottom
        2, 1, 5, 6, // right
        5, 4, 7, 6, // back
        0, 1, 2, 3, // front
        0, 3, 7, 4, // left
    };

    GLfloat vertexPos[] = { //vertices
        0, 0, 1, // 0: bottom-left-front
        1, 0, 1, // 1: bottom-right-front
        1, 1, 1, // 2: top-right-front
        0, 1, 1, // 3: top-left-front
        0, 0, 0, // 0: bottom-left-back
        1, 0, 0, // 1: bottom-right-back
        1, 1, 0, // 2: top-right-back
        0, 1, 0, // 3: top-left-back
    };

    GLfloat vertexNor[] = { //normals
         1.0,  1.0,  1.0, // 0: unused
         0.0, -1.0,  0.0, // 1: bottom
         0.0,  0.0,  1.0, // 2: front
         1.0,  1.0,  1.0, // 3: unused
        -1.0,  0.0,  0.0, // 4: left
         1.0,  0.0,  0.0, // 5: right
         0.0,  0.0, -1.0, // 6: back 
         0.0,  1.0,  0.0, // 7: top
    };

	gVertexDataSizeInBytes = sizeof(vertexPos);
	gNormalDataSizeInBytes = sizeof(vertexNor);
    gTriangleIndexDataSizeInBytes = sizeof(indices);
    gLineIndexDataSizeInBytes = sizeof(indicesLines);
    int allIndexSize = gTriangleIndexDataSizeInBytes + gLineIndexDataSizeInBytes;

	glBufferData(GL_ARRAY_BUFFER, gVertexDataSizeInBytes + gNormalDataSizeInBytes, 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, gVertexDataSizeInBytes, vertexPos);
	glBufferSubData(GL_ARRAY_BUFFER, gVertexDataSizeInBytes, gNormalDataSizeInBytes, vertexNor);

	glBufferData(GL_ELEMENT_ARRAY_BUFFER, allIndexSize, 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, gTriangleIndexDataSizeInBytes, indices);
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, gTriangleIndexDataSizeInBytes, gLineIndexDataSizeInBytes, indicesLines);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(gVertexDataSizeInBytes));
}

void init() 
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // polygon offset is used to prevent z-fighting between the cube and its borders
    glPolygonOffset(0.5, 0.5);
    glEnable(GL_POLYGON_OFFSET_FILL);

    initShaders();
    initVBO();
    initFonts(gWidth, gHeight);
}

void drawCube()
{
	glUseProgram(gProgram[0]);
    glUniform3fv(kdLoc[0], 1, glm::value_ptr(kdCubes)); //color
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0); //12 triangles * 3 vertices
}

void drawCubeEdges()
{
    glLineWidth(3);

	glUseProgram(gProgram[1]);

    for (int i = 0; i < 6; ++i)
    {
	    glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, BUFFER_OFFSET(gTriangleIndexDataSizeInBytes + i * 4 * sizeof(GLuint)));
    }
}

void drawBigCube(int i, int j, int k) { //left-bottom-near is 0,0,0
    glUseProgram(gProgram[0]);
    glUniform3fv(kdLoc[0], 1, glm::value_ptr(kdCubes)); //color

    // Spacing between cubes in the grid
    float spacing = 1.0f;

    // Iterate through the 3x3 grid
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                glm::mat4 currentModelMatrix = glm::rotate(modelingMatrix, glm::radians(current_rot_angle), y_axis);

                currentModelMatrix = glm::translate(currentModelMatrix, glm::vec3(
                    (-4.5+i+x) * spacing, (-6.5+j+y) * spacing, (-4.5+k+z) * spacing)); //translate applied first

                // Pass the updated modeling matrix to the shader
                glUniformMatrix4fv(modelingMatrixLoc[0], 1, GL_FALSE, glm::value_ptr(currentModelMatrix));

                // Draw the cube
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0); // 12 triangles * 3 vertices
            }
        }
    }
}

void drawBigCubeEdges(int i, int j, int k)
{
    glLineWidth(3);

	glUseProgram(gProgram[1]);

    float spacing = 1.0f;

    // Iterate through the 3x3 grid
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                glm::mat4 currentModelMatrix = glm::rotate(modelingMatrix, glm::radians(current_rot_angle), y_axis);

                currentModelMatrix = glm::translate(currentModelMatrix, glm::vec3(
                    (-4.5+i+x) * spacing, (-6.5+j+y) * spacing, (-4.5+k+z) * spacing)); //translate applied first

                // Pass the updated modeling matrix to the shader
                glUniformMatrix4fv(modelingMatrixLoc[1], 1, GL_FALSE, glm::value_ptr(currentModelMatrix));

                // Draw the lines
                for (int i = 0; i < 6; ++i)
                {
                    glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, BUFFER_OFFSET(gTriangleIndexDataSizeInBytes + i * 4 * sizeof(GLuint)));
                }

            }
        }
    }

}

void drawPlatform()
{
    glUseProgram(gProgram[0]);
    glUniform3fv(kdLoc[0], 1, glm::value_ptr(kdGround)); //color

    float spacing = 1.0f;

    for (int x = 0; x < 9; ++x) {
        for (int z = 0; z < 9; ++z) {

            glm::mat4 currentModelMatrix = glm::rotate(modelingMatrix, glm::radians(current_rot_angle), y_axis); //rotate applied last

            currentModelMatrix = glm::translate(currentModelMatrix, glm::vec3(
                (-4.5+x) * spacing, (-7) * spacing, (-4.5+z) * spacing));

            currentModelMatrix = glm::scale(currentModelMatrix, glm::vec3(
                1, 0.5, 1));

            glUniformMatrix4fv(modelingMatrixLoc[0], 1, GL_FALSE, glm::value_ptr(currentModelMatrix));

            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }       
    }
}

void drawPlatformEdges()
{
    glLineWidth(3);

    glUseProgram(gProgram[1]);

    float spacing = 1.0f;

    for (int x = 0; x < 9; ++x) {
        for (int z = 0; z < 9; ++z) {

            glm::mat4 currentModelMatrix = glm::rotate(modelingMatrix, glm::radians(current_rot_angle), y_axis); //rotate applied last

            currentModelMatrix = glm::translate(currentModelMatrix, glm::vec3(
                (-4.5+x) * spacing, (-7) * spacing, (-4.5+z) * spacing));

            currentModelMatrix = glm::scale(currentModelMatrix, glm::vec3(
                1, 0.5, 1));

            glUniformMatrix4fv(modelingMatrixLoc[1], 1, GL_FALSE, glm::value_ptr(currentModelMatrix));

            for (int i = 0; i < 6; ++i)
            {
                glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, BUFFER_OFFSET(gTriangleIndexDataSizeInBytes + i * 4 * sizeof(GLuint)));
            }
        }       
    }
}

void renderText(const std::string& text, GLfloat x, GLfloat y, GLfloat scale, glm::vec3 color)
{
    // Activate corresponding render state	
    glUseProgram(gProgram[2]);
    glUniform3f(glGetUniformLocation(gProgram[2], "textColor"), color.x, color.y, color.z);
    //glGetUniformLocation(gProgram[2], "textColor") returns location of uniform variable textColor in gProgram[2]
    //glUniform3f assigns a vec3 value to that variable
    glActiveTexture(GL_TEXTURE0);

    // Iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++) 
    {
        Character ch = Characters[*c];

        GLfloat xpos = x + ch.Bearing.x * scale;
        GLfloat ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        GLfloat w = ch.Size.x * scale;
        GLfloat h = ch.Size.y * scale;

        // Update VBO for each character
        GLfloat vertices[6][4] = {
            { xpos,     ypos + h,   0.0, 0.0 },            
            { xpos,     ypos,       0.0, 1.0 },
            { xpos + w, ypos,       1.0, 1.0 },

            { xpos,     ypos + h,   0.0, 0.0 },
            { xpos + w, ypos,       1.0, 1.0 },
            { xpos + w, ypos + h,   1.0, 0.0 }           
        };

        // Render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);

        // Update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, gTextVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // Be sure to use glBufferSubData and not glBufferData

        //glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // Now advance cursors for next glyph (note that advance is number of 1/64 pixels)

        x += (ch.Advance >> 6) * scale; // Bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}


void display()
{
    glClearColor(0, 0, 0, 1);
    glClearDepth(1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawPlatform();
    drawPlatformEdges();

    //draw current cubes
    for (Cube cube : cubes)
    {
        glm::vec3 pos = cube.position;
        drawBigCube(pos.x, pos.y, pos.z);
        drawBigCubeEdges(pos.x, pos.y, pos.z);
    }

    if (game_over) renderText("Game Over", gWidth/2 - 130, gHeight/2 - 60, 1, glm::vec3(1, 1, 0));
    renderText(get_mode_string(viewing_mode), gWidth/2 - 200, gHeight - 30, 0.5, glm::vec3(1, 1, 0));
    renderText(lastPressedKey, gWidth/2 - 200, gHeight - 60, 0.5, glm::vec3(1, 0, 0));
    string point_string = "Points: " + to_string(points);
    renderText(point_string, gWidth/2 + 100, gHeight - 30, 0.5, glm::vec3(1, 1, 0));

    assert(glGetError() == GL_NO_ERROR);
}

bool row_complete()
{
    float y_value = cubes.back().position.y;
    int cube_count = 0;
    for (Cube cube : cubes) 
    {
        if (cube.position.y == y_value) {
            cube_count++;
            if (cube_count == 9) break;
        }
    }

    if (cube_count != 9) return false;

    for (int i = 0; i < cubes.size(); ) 
    {
        if (cubes[i].position.y == y_value) cubes.erase(cubes.begin() + i);
        else ++i;
    }

    for (int i = 0; i < cubes.size(); i++) 
    {
        if (cubes[i].position.y > y_value) cubes[i].position.y -= 3;
    }

    points += 243;

    return true;
}

void animate()
{
    if (rotating)
    {
        if (abs(target_rot_angle-current_rot_angle) < rotation_step)
        {
            current_rot_angle = target_rot_angle;
            rotating = false;
        }
        else if (current_rot_angle < target_rot_angle) 
        {
            current_rot_angle += rotation_step;
        }
        else 
        {
            current_rot_angle -= rotation_step;
        }
    }

    if (new_cube)
    {
        new_cube = false;
        cubes.push_back(Cube(3,12,3));
        
    }

    if (s_pressed == false) return;

    glm::vec3 pos = cubes.back().position;

    //check touch
    bool touches = false;
    for (Cube cube : cubes) 
    {
        glm::vec3 pos2 = cube.position; //check against static cube
        if ((pos2.x - 3 < pos.x && pos.x < pos2.x + 3)&&
            (pos2.z - 3 < pos.z && pos.z < pos2.z + 3)&&
            (pos2.y == pos.y - 3))
        {
            touches = true;
            break;
        }
    }            
    if (pos.y==0) touches = true;

    if (touches)
    {
        if (!row_complete() && pos.y == 12) game_over = true;
        else new_cube = true;
    }
    else { //doesn't touch
        if (timer_alert)
        {
            cubes.back().position = glm::vec3(pos.x, pos.y - 1, pos.z); 
            timer_alert = false;
        }
    }

    


}


void reshape(GLFWwindow* window, int w, int h)
{
    w = w < 1 ? 1 : w;
    h = h < 1 ? 1 : h;

    gWidth = w;
    gHeight = h;

    glViewport(0, 0, w, h);

	// Use perspective projection

	float fovyRad = (float) (45.0 / 180.0) * M_PI;
	projectionMatrix = glm::perspective(fovyRad, gWidth / (float) gHeight, 1.0f, 100.0f);

    // always look toward (0, 0, 0) !!
	viewingMatrix = glm::lookAt(eyePos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

    for (int i = 0; i < 2; ++i)
    {
        glUseProgram(gProgram[i]);
        glUniformMatrix4fv(projectionMatrixLoc[i], 1, GL_FALSE, glm::value_ptr(projectionMatrix));
        glUniformMatrix4fv(viewingMatrixLoc[i], 1, GL_FALSE, glm::value_ptr(viewingMatrix));
    }

    glm::mat4 textProjection = glm::ortho(0.0f, static_cast<GLfloat>(gWidth), 0.0f, static_cast<GLfloat>(gHeight));
    glUseProgram(gProgram[2]);
    glUniformMatrix4fv(glGetUniformLocation(gProgram[2], "projection"), 1, GL_FALSE, glm::value_ptr(textProjection));
}

glm::vec3 find_new_coords(int dir)
{
    glm::vec3 new_pos = cubes.back().position;
    int x_change = 0, z_change = 0;

    switch (viewing_mode)
    {
    case FRONT:
        x_change = (dir == RIGHT)? 1 : -1;
        break;
    case BACK:
        x_change = (dir == RIGHT)? -1 : 1;
        break;
    case RIGHT:
        z_change = (dir == RIGHT)? -1 : 1;
        break;
    case LEFT:
        z_change = (dir == RIGHT)? 1 : -1;
        break;
    default:
        cout << "unexpected viewing_mode" << endl;
        break;
    }

    new_pos.x += x_change;
    new_pos.z += z_change;
    
    return new_pos;
}

void move(glm::vec3 new_pos)
{
    bool conflict = false;
    //check bounds
    if ((new_pos.x < 0 || new_pos.x > 6)||(new_pos.z < 0 || new_pos.z > 6)) conflict = true;
    else 
    {
        for (int i = 0; i < cubes.size() - 1; ++i) //don't compare against itself
        {
            glm::vec3 pos2 = cubes[i].position;

            if ((new_pos.x < pos2.x + 3)&&(new_pos.x > pos2.x - 3)&&
                (new_pos.y < pos2.y + 3)&&(new_pos.y > pos2.y - 3)&&
                (new_pos.z < pos2.z + 3)&&(new_pos.z > pos2.z - 3)) 
            {
                conflict = true;
                break;
            }    
        }
    }

    if (conflict == false) cubes.back().position = new_pos;

}

void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS) return;

    lastPressedKey = std::string(1, (char)key);

    switch (key) {
        case GLFW_KEY_Q: 
        case GLFW_KEY_ESCAPE:
            {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                break;
            }
        case GLFW_KEY_A:
            {
                if (game_over || new_cube) break;
                //Move the block left with respect to the current view
                glm::vec3 new_coords = find_new_coords(LEFT);
                move(new_coords);
                break;
            }
        case GLFW_KEY_D:
            {
                if (game_over || new_cube) break;
                //Move the block right with respect to the current view
                glm::vec3 new_coords = find_new_coords(RIGHT);
                move(new_coords);
                break;
            }
        case GLFW_KEY_S:
            {
                s_pressed = true;
                //Speed up the falling speed of the block
                speedMultiplier += 0.12;
                break;
            }
        case GLFW_KEY_W:
            {
                //Slow down the falling speed of the block
                if (speedMultiplier < 0.35) 
                {
                    speedMultiplier = 0;
                }
                else speedMultiplier -= 0.1;
                break;
            }
        case GLFW_KEY_H:
            {
                if (game_over) break;
                //Turn around the game area in the left direction
                if (!rotating)
                {
                    rotating = true;
                    target_rot_angle = current_rot_angle + 90.0;
                    viewing_mode = mod(viewing_mode - 1, 4);
                }
                break;
            }
        case GLFW_KEY_K:
            {
                if (game_over) break;
                //Turn around the game area in the right direction
                if (!rotating)
                {
                    rotating = true;
                    target_rot_angle = current_rot_angle - 90.0;            
                    viewing_mode = mod(viewing_mode + 1, 4);
                }
                break;
            }
            
    }

}

void mainLoop(GLFWwindow* window)
{
    viewing_mode = FRONT;
    points = 0;
    new_cube = true;
    game_over = false;

    std::thread timerThread([&]() {
        controllableTimer();
    });

    while (!glfwWindowShouldClose(window))
    {
        display();
        if (!game_over) animate();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    stop_timer = true;
    if (timerThread.joinable()) {
        timerThread.join();
    }
}

int main(int argc, char** argv)   // Create Main Function For Bringing It All Together
{
    GLFWwindow* window;
    if (!glfwInit())
    {
        exit(-1);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(gWidth, gHeight, "tetrisGL", NULL, NULL);

    if (!window)
    {
        glfwTerminate();
        exit(-1);
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); //to swap buffers at refresh interval (prevent tearing)

    // Initialize GLEW to setup the OpenGL Function pointers
    if (GLEW_OK != glewInit()) //glew is necessary to access OpenGL extensions
    {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return EXIT_FAILURE;
    }

    char rendererInfo[512] = {0};
    strcpy(rendererInfo, (const char*) glGetString(GL_RENDERER));
    strcat(rendererInfo, " - ");
    strcat(rendererInfo, (const char*) glGetString(GL_VERSION));
    glfwSetWindowTitle(window, rendererInfo);

    init();

    glfwSetKeyCallback(window, keyboard);
    glfwSetWindowSizeCallback(window, reshape); //registers the reshape() function as a callback for window resizing events

    reshape(window, gWidth, gHeight); // need to call this once ourselves
    mainLoop(window); // this does not return unless the window is closed

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}