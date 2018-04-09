 // Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <queue>
#include <stack> 

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <glfw3.h>
GLFWwindow* g_pWindow;
unsigned int g_nWidth = 1024, g_nHeight = 768;

// Include AntTweakBar
#include <AntTweakBar.h>
TwBar *g_pToolBar;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "glm/ext.hpp"
using namespace glm;

#include <shader.hpp>
#include <texture.hpp>
#include <controls.hpp>
#include <objloader.hpp>
#include <vboindexer.hpp>
#include <glerror.hpp>

typedef struct e {
	unsigned short vertex1;
	unsigned short vertex2;
	float distance;

	bool operator<(const e &outro) const
	{
		return distance > outro.distance;
	}
	bool operator>(const e &outro) const
	{
		return distance < outro.distance;
	}
}edge;

typedef struct r {
	std::vector<unsigned short> indices_history;
	std::vector<glm::vec3> vertices_history;
}history;

void WindowSizeCallBack(GLFWwindow *pWindow, int nWidth, int nHeight) {

	g_nWidth = nWidth;
	g_nHeight = nHeight;
	glViewport(0, 0, g_nWidth, g_nHeight);
	TwWindowSize(g_nWidth, g_nHeight);
}

void CalculateDistances(std::vector<glm::vec3>& indexed_vertices, std::vector<unsigned short>& indices, std::vector<edge>& edges);
void shortest_shared_edge(std::vector<glm::vec3>& indexed_vertices, std::vector<unsigned short>& indices, std::vector<edge>& edges);

int main(void)
{
	int nUseMouse = 0;

	// Initialise GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	g_pWindow = glfwCreateWindow(g_nWidth, g_nHeight, "CG UFPel", NULL, NULL);
	if (g_pWindow == NULL){
		fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(g_pWindow);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		return -1;
	}

	check_gl_error();//OpenGL error from GLEW

	// Initialize the GUI
	TwInit(TW_OPENGL_CORE, NULL);
	TwWindowSize(g_nWidth, g_nHeight);

	// Set GLFW event callbacks. I removed glfwSetWindowSizeCallback for conciseness
	glfwSetMouseButtonCallback(g_pWindow, (GLFWmousebuttonfun)TwEventMouseButtonGLFW); // - Directly redirect GLFW mouse button events to AntTweakBar
	glfwSetCursorPosCallback(g_pWindow, (GLFWcursorposfun)TwEventMousePosGLFW);          // - Directly redirect GLFW mouse position events to AntTweakBar
	glfwSetScrollCallback(g_pWindow, (GLFWscrollfun)TwEventMouseWheelGLFW);    // - Directly redirect GLFW mouse wheel events to AntTweakBar
	glfwSetKeyCallback(g_pWindow, (GLFWkeyfun)TwEventKeyGLFW);                         // - Directly redirect GLFW key events to AntTweakBar
	glfwSetCharCallback(g_pWindow, (GLFWcharfun)TwEventCharGLFW);                      // - Directly redirect GLFW char events to AntTweakBar
	glfwSetWindowSizeCallback(g_pWindow, WindowSizeCallBack);

	//create the toolbar
	g_pToolBar = TwNewBar("CG UFPel ToolBar");
	// Add 'speed' to 'bar': it is a modifable (RW) variable of type TW_TYPE_DOUBLE. Its key shortcuts are [s] and [S].
	double speed = 0.0;
	TwAddVarRW(g_pToolBar, "speed", TW_TYPE_DOUBLE, &speed, " label='Rot speed' min=0 max=2 step=0.01 keyIncr=s keyDecr=S help='Rotation speed (turns/second)' ");
	// Add 'bgColor' to 'bar': it is a modifable variable of type TW_TYPE_COLOR3F (3 floats color)
	vec3 oColor(0.0f);
	TwAddVarRW(g_pToolBar, "bgColor", TW_TYPE_COLOR3F, &oColor[0], " label='Background color' ");

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(g_pWindow, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetCursorPos(g_pWindow, g_nWidth / 2, g_nHeight / 2);

	// Dark blue background
	glClearColor(0.0f, 0.0f, 0.4f, 0.0f);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS);

	// Cull triangles which normal is not towards the camera
	glEnable(GL_CULL_FACE);

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	// Create and compile our GLSL program from the shaders
	GLuint programID = LoadShaders("shaders/StandardShading.vertexshader", "shaders/StandardShading.fragmentshader");

	// Get a handle for our "MVP" uniform
	GLuint MatrixID      = glGetUniformLocation(programID, "MVP");
	GLuint ViewMatrixID  = glGetUniformLocation(programID, "V");
	GLuint ModelMatrixID = glGetUniformLocation(programID, "M");

	// Load the texture
	GLuint Texture = loadDDS("mesh/uvmap.DDS");

	// Get a handle for our "myTextureSampler" uniform
	GLuint TextureID = glGetUniformLocation(programID, "myTextureSampler");

	// Read our .obj file
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	bool res = loadOBJ("mesh/suzanne.obj", vertices, uvs, normals);

	//int i = 0;
	/*int count=0;
	for (auto i : vertices)
	{
		std::cout << glm::to_string(i) << std::endl;
		count++;
	}
	std::cout << count << std::endl;*/

	std::vector<unsigned short> indices;
	std::vector<glm::vec3> indexed_vertices;
	std::vector<glm::vec2> indexed_uvs;
	std::vector<glm::vec3> indexed_normals;
	indexVBO(vertices, uvs, normals, indices, indexed_vertices, indexed_uvs, indexed_normals);
	
	//std::priority_queue<edge> shortest_edge;
	std::vector<edge> edges;
	shortest_shared_edge(indexed_vertices, indices, edges);
	if (edges[0].distance == -1)
	{
		std::cout<< "couldnt find any to simplify" << std::endl;
	}
	//std::make_heap(edges.begin(), edges.end());
	/*
	unsigned short i = 0;
	for (auto it = begin(indices); it != end(indices); it += 3, i++)
	{
		bool flag = true;
		edge edge1_2;
		edge1_2.vertex1 = *it;
		edge1_2.vertex2 = *(it + 1);
		if (edges.empty())
		{
			//std::cout << "vazio" << std::endl;
			edge1_2.distance = distance(indexed_vertices[edge1_2.vertex1], indexed_vertices[edge1_2.vertex2]);
			edges.push_back(edge1_2);
		}
		else {
			for (auto j : edges)
			{
				//std::cout << "i= " << i << " j= " << glm::to_string(j.vertex1) << std::endl;
				if ((j.vertex1 == edge1_2.vertex1 && j.vertex2 == edge1_2.vertex2) ||
					(j.vertex1 == edge1_2.vertex2 && j.vertex2 == edge1_2.vertex1))
				{
					flag = false;
					//std::cout << "aresta ja encontrada" << std::endl;
				}
			}
			if (flag)
			{
				edge1_2.distance = distance(indexed_vertices[edge1_2.vertex1], indexed_vertices[edge1_2.vertex2]);
				edges.push_back(edge1_2);
				flag = false;
			}
		}


		edge edge1_3;
		edge1_3.vertex1 = *it;
		edge1_3.vertex2 = *(it + 2);
		flag = true;
		for (auto j : edges)
		{
			if ((j.vertex1 == edge1_3.vertex1 && j.vertex2 == edge1_3.vertex2) ||
				(j.vertex1 == edge1_3.vertex2 && j.vertex2 == edge1_3.vertex1))
			{
				flag = false;
			}
		}
		if (flag)
		{
			edge1_3.distance = distance(indexed_vertices[edge1_2.vertex1], indexed_vertices[edge1_2.vertex2]);
			edges.push_back(edge1_3);
		}

		flag = true;
		edge edge2_3;
		edge2_3.vertex1 = *(it + 1);
		edge2_3.vertex2 = *(it + 2);

		for (auto j : edges)
		{
			if ((j.vertex1 == edge2_3.vertex1 && j.vertex2 == edge2_3.vertex2) ||
				(j.vertex1 == edge2_3.vertex2 && j.vertex2 == edge2_3.vertex1))
			{
				flag = false;
				//std::cout << "aresta ja encontrada" << std::endl;
			}
		}
		if (flag)
		{
			edge2_3.distance = distance(indexed_vertices[edge1_2.vertex1], indexed_vertices[edge1_2.vertex2]);
			edges.push_back(edge2_3);

		}
	}
	for (auto it : edges)
	{
		//if((indexed_vertices[it.vertex1].x == -1.0 && indexed_vertices[it.vertex1].y == 1.0 && indexed_vertices[it.vertex1].z == 1.0)
		//	|| (indexed_vertices[it.vertex2].x == -1.0 && indexed_vertices[it.vertex2].y == 1.0 && indexed_vertices[it.vertex2].z == 1.0))
		std::cout <<it.vertex1 <<" "<< it.vertex2 << std::endl;
	}

		//std::cout << "i " << i << std::endl;
		/*newEdge2.vertex1 = *it;
		newEdge2.vertex2 = *(it + 2);
		newEdge2.distance = distance(newEdge.vertex1, newEdge.vertex2);
		edges.push_back(newEdge2);
		edge newEdge3;
		newEdge3.vertex1 = *(it + 1);
		newEdge3.vertex2 = *(it + 2);
		newEdge3.distance = distance(newEdge.vertex1, newEdge.vertex2);
		edges.push_back(newEdge);*/

		/*edge *aresta1 = (edge*)malloc(sizeof(edge));
		(*aresta1).vertex1 = *it;
		(*aresta1).vertex2 = *(it + 1);
		if(distance((*aresta1).vertex1, (*aresta1).vertex2) < 0)
		std::cout << distance((*aresta1).vertex1, (*aresta1).vertex2) << std::endl;
		shortest_edge.push(*aresta1);*/
		//std::vector<edge> edges;

		//std::cout << distance(newEdge.vertex1, newEdge.vertex2) << std :: endl;*/

	//}*/
	//std::cout << "size " << indices.size() << std::endl;
	//std::cout << "size " << edges.size() << " first: "<< edges[0].vertex1<< " second: " << edges[0].vertex2 <<std::endl;

	// std::priority_queue<edge, std::vector<edge>, std::less<edge>> shortest_edge(edges.begin(), edges.end());
	/*for (int i = 0; i <(edges.size()); i++)
	{
		edge ex = shortest_edge.top();
		shortest_edge.pop();
		std::cout << "ultimo1: " << ex.distance <<"vertex : " << (ex.vertex1) << "vertex2 : " << (ex.vertex2)<< i << std::endl;
	}*/
	/*edge removed = shortest_edge.top();
	std::cout << removed.distance << std::endl;
	shortest_edge.pop();*/
	

	

	//int count = 0;
	/*for (auto i : indexed_uvs)
	{
		std::cout << glm::to_string(i) << std::endl;
		count++;
	}*/

	/*for (auto i : indices)
	{
		std::cout << i << std::endl;
		count++;
	}
	std::cout << count << std::endl;
	count = 0;*/
	
	//std::cout << size(indexed_vertices)<< std::endl;
	//unsigned int range = 0, flag = 0;
	/*unsigned short max = 0;
	for (auto it = begin(indices); it != end(indices) ; it = it + 3 )
	{
		if (*it > max)
		{
			max = *it;
		}
		std::cout <<(*it) << " " << std::endl;
		count++;
	}
	std::cout <<"count: "<< count << "max : " << max << std::endl;

	count = 0;
	for (auto it = begin(indexed_vertices); it != end(indexed_vertices); it = it + 1)
	{
		//std::cout << glm::to_string(*it) << " " << std::endl;
		count++;
	}
	std::cout << "count: " << count << "max : " << max << std::endl;*/

	/*for (auto i = begin(indexed_vertices); i != end(indexed_vertices); ++i)
	{
		int count_i = 0;
		for (auto j = begin(indexed_vertices); j != end(indexed_vertices); ++j)
		{
			int count_j = 0;
			if (count_i != count_j && )
			{

			}
		}
	}*/

	// Load it into a VBO

	GLuint vertexbuffer;
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_vertices.size() * sizeof(glm::vec3), &indexed_vertices[0], GL_STATIC_DRAW);

	GLuint uvbuffer;
	glGenBuffers(1, &uvbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_uvs.size() * sizeof(glm::vec2), &indexed_uvs[0], GL_STATIC_DRAW);

	GLuint normalbuffer;
	glGenBuffers(1, &normalbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_normals.size() * sizeof(glm::vec3), &indexed_normals[0], GL_STATIC_DRAW);

	// Generate a buffer for the indices as well
	GLuint elementbuffer;
	glGenBuffers(1, &elementbuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

	// Get a handle for our "LightPosition" uniform
	glUseProgram(programID);
	GLuint LightID = glGetUniformLocation(programID, "LightPosition_worldspace");

	// For speed computation
	double lastTime = glfwGetTime();
	int nbFrames    = 0;

	double lastTimePress = glfwGetTime();

	std::stack<history> step_register;

	do{
        check_gl_error();

        //use the control key to free the mouse
		if (glfwGetKey(g_pWindow, GLFW_KEY_LEFT_CONTROL) != GLFW_PRESS)
			nUseMouse = 1;
		else
			nUseMouse = 0;

		// Measure speed
		double currentTime = glfwGetTime();
		nbFrames++;
		if (currentTime - lastTime >= 1.0){ // If last prinf() was more than 1sec ago
			// printf and reset
	//		printf("%f ms/frame\n", 1000.0 / double(nbFrames));
			nbFrames  = 0;
			lastTime += 1.0;
		}

		//my code
		double timePress = glfwGetTime();

		if (glfwGetKey(g_pWindow, GLFW_KEY_M) == GLFW_PRESS)
		{
			if ( (timePress - lastTimePress) >= 0.001)
			{
				edge ex = edges[0];
				if (ex.distance == -1)
				{
					std::cout << "no more to simplify" << std::endl;
				}
				else
				{
					history step;
					step.indices_history  = indices;
					step.vertices_history = indexed_vertices;
					step_register.push(step);

					//shortest_edge.pop();
					vec3 midpoint;
					midpoint.x = (indexed_vertices[ex.vertex1].x + indexed_vertices[ex.vertex2].x) / 2;

					midpoint.y = (indexed_vertices[ex.vertex1].y + indexed_vertices[ex.vertex2].y) / 2;

					midpoint.z = (indexed_vertices[ex.vertex1].z + indexed_vertices[ex.vertex2].z) / 2;

					//std::cout << glm::to_string(indexed_vertices[ex.vertex1]) << std::endl;
					//std::cout << glm::to_string(indexed_vertices[ex.vertex2]) << std::endl;
					//std::cout << glm::to_string(midpoint) << std::endl;

					//std::cout << "old size: " << indexed_vertices.size() << std::endl;
					indexed_vertices.push_back(midpoint);
					//std::cout << "new size: " << indexed_vertices.size() <<std::endl;
					unsigned short newVertexIndex = indexed_vertices.size() - 1;
					// std::cout << "new vertex: " << indices.size() << std::endl;
					std::cout << "antigos triangulos" << std::endl;
					/*for (int i = 0; i < indices.size(); i += 3)
					{
						std::cout << "1: " << indices[i] << " 2: " << indices[i + 1] << " 3: " << indices[i + 2] << std::endl;
					}*/
					for (int i = 0; i < indices.size(); i++)
					{
						if (indices[i] == ex.vertex1 || indices[i] == ex.vertex2)
						{
							indices[i] = newVertexIndex;
						}
					}
					std::cout << "novos triangulos" << std::endl;
					std::cout << std::endl << std::endl;
					/*for (int i = 0; i < indices.size(); i += 3)
					{
						std::cout << "1: " << indices[i] << " 2: " << indices[i + 1] << " 3: " << indices[i + 2] << std::endl;
					}*/


					lastTimePress = glfwGetTime();

					glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
					glBufferData(GL_ARRAY_BUFFER, indexed_vertices.size() * sizeof(glm::vec3), &indexed_vertices[0], GL_STATIC_DRAW);

					glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
					glBufferData(GL_ARRAY_BUFFER, indexed_uvs.size() * sizeof(glm::vec2), &indexed_uvs[0], GL_STATIC_DRAW);

					glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
					glBufferData(GL_ARRAY_BUFFER, indexed_normals.size() * sizeof(glm::vec3), &indexed_normals[0], GL_STATIC_DRAW);

					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
					glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

					

					edges.clear();
					shortest_shared_edge(indexed_vertices, indices, edges);
				}
			}

		}

		if (glfwGetKey(g_pWindow, GLFW_KEY_R) == GLFW_PRESS)
		{
			if ((timePress - lastTimePress) >= 0.001)
			{
				if (!step_register.empty())
				{
					history step = step_register.top();
					step_register.pop();

					indexed_vertices = step.vertices_history;
					indices = step.indices_history;

					glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
					glBufferData(GL_ARRAY_BUFFER, indexed_vertices.size() * sizeof(glm::vec3), &indexed_vertices[0], GL_STATIC_DRAW);

					glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
					glBufferData(GL_ARRAY_BUFFER, indexed_uvs.size() * sizeof(glm::vec2), &indexed_uvs[0], GL_STATIC_DRAW);

					glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
					glBufferData(GL_ARRAY_BUFFER, indexed_normals.size() * sizeof(glm::vec3), &indexed_normals[0], GL_STATIC_DRAW);

					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
					glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);
				}
				lastTimePress = glfwGetTime();
			}
		}

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Use our shader
		glUseProgram(programID);

		// Compute the MVP matrix from keyboard and mouse input
		computeMatricesFromInputs(nUseMouse, g_nWidth, g_nHeight);
		glm::mat4 ProjectionMatrix = getProjectionMatrix();
		glm::mat4 ViewMatrix       = getViewMatrix();
		glm::mat4 ModelMatrix      = glm::mat4(1.0);
		glm::mat4 MVP              = ProjectionMatrix * ViewMatrix * ModelMatrix;

		// Send our transformation to the currently bound shader,
		// in the "MVP" uniform
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
		glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);

		glm::vec3 lightPos = glm::vec3(4, 4, 4);
		glUniform3f(LightID, lightPos.x, lightPos.y, lightPos.z);

		// Bind our texture in Texture Unit 0
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Texture);
		// Set our "myTextureSampler" sampler to user Texture Unit 0
		glUniform1i(TextureID, 0);

		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(
			0,                  // attribute
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
			);

		// 2nd attribute buffer : UVs
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		glVertexAttribPointer(
			1,                                // attribute
			2,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
			);

		// 3rd attribute buffer : normals
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
		glVertexAttribPointer(
			2,                                // attribute
			3,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
			);

		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);

		static int pressed = 0;

		if (glfwGetKey(g_pWindow, GLFW_KEY_W) == GLFW_PRESS)
			pressed = 1;

		if (glfwGetKey(g_pWindow, GLFW_KEY_W) == GLFW_RELEASE)
			pressed = 0;

		if (pressed)
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			glDisable(GL_CULL_FACE);
		}

		// Draw the triangles !
		glDrawElements(
			GL_TRIANGLES,        // mode
			indices.size(),      // count
			GL_UNSIGNED_SHORT,   // type
			(void*)0             // element array buffer offset
			);

		if (pressed)
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glEnable(GL_CULL_FACE);
		}

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);

		// Draw tweak bars
		TwDraw();

		// Swap buffers
		glfwSwapBuffers(g_pWindow);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (glfwGetKey(g_pWindow, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
	glfwWindowShouldClose(g_pWindow) == 0);

	// Cleanup VBO and shader
	glDeleteBuffers(1, &vertexbuffer);
	glDeleteBuffers(1, &uvbuffer);
	glDeleteBuffers(1, &normalbuffer);
	glDeleteBuffers(1, &elementbuffer);
	glDeleteProgram(programID);
	glDeleteTextures(1, &Texture);
	glDeleteVertexArrays(1, &VertexArrayID);

	// Terminate AntTweakBar and GLFW
	TwTerminate();
	glfwTerminate();

	return 0;
}

void CalculateDistances(std::vector<glm::vec3>& indexed_vertices, std::vector<unsigned short>& indices, std::vector<edge>& edges)
{
	unsigned short i = 0;
	edge shortest;
	for (auto it = begin(indices); it != end(indices); it += 3, i++)
	{

		bool flag = true;
		edge edge1_2;
		edge1_2.vertex1 = *it;
		edge1_2.vertex2 = *(it + 1);
		if (edge1_2.vertex1 != edge1_2.vertex2)
		{
			if (edges.empty())
			{
				//std::cout << "vazio" << std::endl;
				edge1_2.distance = distance(indexed_vertices[edge1_2.vertex1], indexed_vertices[edge1_2.vertex2]);
				edges.push_back(edge1_2);
				shortest = edge1_2;
			}
			else {
				for (auto j : edges)
				{
					//std::cout << "i= " << i << " j= " << glm::to_string(j.vertex1) << std::endl;
					if ((j.vertex1 == edge1_2.vertex1 && j.vertex2 == edge1_2.vertex2) ||
						(j.vertex1 == edge1_2.vertex2 && j.vertex2 == edge1_2.vertex1))
					{
						flag = false;
						//std::cout << "aresta ja encontrada" << std::endl;
					}
				}
				if (flag)
				{
					edge1_2.distance = distance(indexed_vertices[edge1_2.vertex1], indexed_vertices[edge1_2.vertex2]);
					edges.push_back(edge1_2);
					flag = false;
					if (edge1_2.distance < shortest.distance)
					{
						shortest = edge1_2;
					}
				}
			}
		}

		edge edge1_3;
		edge1_3.vertex1 = *it;
		edge1_3.vertex2 = *(it + 2);
		if (edge1_3.vertex1 != edge1_3.vertex2)
		{
			if (edges.empty())
			{
				//std::cout << "vazio" << std::endl;
				edge1_3.distance = distance(indexed_vertices[edge1_3.vertex1], indexed_vertices[edge1_3.vertex2]);
				edges.push_back(edge1_3);
				shortest = edge1_3;
			}
			else
			{
				flag = true;
				for (auto j : edges)
				{
					if ((j.vertex1 == edge1_3.vertex1 && j.vertex2 == edge1_3.vertex2) ||
						(j.vertex1 == edge1_3.vertex2 && j.vertex2 == edge1_3.vertex1))
					{
						flag = false;
					}
				}
				if (flag)
				{
					edge1_3.distance = distance(indexed_vertices[edge1_2.vertex1], indexed_vertices[edge1_2.vertex2]);
					edges.push_back(edge1_3);
					if (edge1_3.distance < shortest.distance)
					{
						shortest = edge1_3;
					}
				}
			}
		}

		flag = true;
		edge edge2_3;
		edge2_3.vertex1 = *(it + 1);
		edge2_3.vertex2 = *(it + 2);

		if (edge2_3.vertex1 != edge2_3.vertex2)
		{
			if (edges.empty())
			{
				//std::cout << "vazio" << std::endl;
				edge2_3.distance = distance(indexed_vertices[edge2_3.vertex1], indexed_vertices[edge2_3.vertex2]);
				edges.push_back(edge2_3);
				shortest = edge2_3;
			}
			else
			{
				for (auto j : edges)
				{
					if ((j.vertex1 == edge2_3.vertex1 && j.vertex2 == edge2_3.vertex2) ||
						(j.vertex1 == edge2_3.vertex2 && j.vertex2 == edge2_3.vertex1))
					{
						flag = false;
						//std::cout << "aresta ja encontrada" << std::endl;
					}
				}
				if (flag)
				{
					edge2_3.distance = distance(indexed_vertices[edge1_2.vertex1], indexed_vertices[edge1_2.vertex2]);
					edges.push_back(edge2_3);
					if (edge2_3.distance < shortest.distance)
					{
						shortest = edge2_3;
					}

				}

			}

		}
	} 
	if (edges.empty())
	{
		edges[0].distance = -1;
	}
	else
		edges[0] = shortest;
	/*for (auto it : edges)
	{
		//if((indexed_vertices[it.vertex1].x == -1.0 && indexed_vertices[it.vertex1].y == 1.0 && indexed_vertices[it.vertex1].z == 1.0)
		//	|| (indexed_vertices[it.vertex2].x == -1.0 && indexed_vertices[it.vertex2].y == 1.0 && indexed_vertices[it.vertex2].z == 1.0))
		std::cout << it.vertex1 << " " << it.vertex2 << std::endl;
	}*/
	
}


//IR PERCORRENDO A LISTA E COLOCANDO OS PARES NO EDGES
//SEMPRE QUE ACHAR UM PAR QUE JA TA NO EDGES, COMPARA PRA VER SE É O MENOR DO QUE O SHORTEST ATUAL
//COLOCAR PRA RETORNAR UM BOOL INDICANDO SUCESSO OU FALHA

void shortest_shared_edge(std::vector<glm::vec3>& indexed_vertices, std::vector<unsigned short>& indices, std::vector<edge>& edges)
{
	int i = 0;
	edge shortest;
	shortest.distance = -1;

	for (auto it = begin(indices); it != end(indices); it += 3, i++)
	{

		//COMEÇO DO TESTE DO PAR 1 2
		edge edge1_2;
		edge1_2.vertex1 = *it;
		edge1_2.vertex2 = *(it + 1);

		if (edge1_2.vertex1 != edge1_2.vertex2)
		{
			if (edges.empty())
			{
				edge1_2.distance = distance(indexed_vertices[edge1_2.vertex1], indexed_vertices[edge1_2.vertex2]);
				edges.push_back(edge1_2);
			}

			else
			{
				edge1_2.distance = distance(indexed_vertices[edge1_2.vertex1], indexed_vertices[edge1_2.vertex2]);
				for (auto j : edges)
				{
					if ((j.vertex1 == edge1_2.vertex1 && j.vertex2 == edge1_2.vertex2) ||
						(j.vertex1 == edge1_2.vertex2 && j.vertex2 == edge1_2.vertex1))
					{

						if (edge1_2.distance < shortest.distance || shortest.distance == -1)
						{
							shortest = edge1_2;
						}

					}
				}

				edges.push_back(edge1_2);

			}
		}
		//FIM DO TESTE DO PAR 1 2

		//COMECO DO TESTE DO PAR 1 3
		edge edge1_3;
		edge1_3.vertex1 = *it;
		edge1_3.vertex2 = *(it + 2);

		if (edge1_3.vertex1 != edge1_3.vertex2)
		{
			if (edges.empty())
			{
				edge1_3.distance = distance(indexed_vertices[edge1_3.vertex1], indexed_vertices[edge1_3.vertex2]);
				edges.push_back(edge1_3);
			}

			else
			{
				edge1_3.distance = distance(indexed_vertices[edge1_3.vertex1], indexed_vertices[edge1_3.vertex2]);

				for (auto j : edges)
				{
					
					if ((j.vertex1 == edge1_3.vertex1 && j.vertex2 == edge1_3.vertex2) ||
						(j.vertex1 == edge1_3.vertex2 && j.vertex2 == edge1_3.vertex1))
					{
						if ( (edge1_3.vertex1 != edge1_2.vertex1 && edge1_3.vertex2 != edge1_2.vertex2) ||
							 (edge1_3.vertex1 != edge1_2.vertex2 && edge1_3.vertex2 != edge1_2.vertex1))
						{
							if (edge1_3.distance < shortest.distance || shortest.distance == -1)
							{
								shortest = edge1_3;
							}
						}

					}
				}

				edges.push_back(edge1_3);

			}
		}
		//FIM DO TESTE DO PAR 1 3

		//COMEÇO DO TESTE DO PAR 2 3
		edge edge2_3;
		edge2_3.vertex1 = *(it + 1);
		edge2_3.vertex2 = *(it + 2);

		if (edge2_3.vertex1 != edge2_3.vertex2)
		{
			if (edges.empty())
			{
				edge2_3.distance = distance(indexed_vertices[edge2_3.vertex1], indexed_vertices[edge2_3.vertex2]);
				edges.push_back(edge2_3);
			}

			else
			{
				edge2_3.distance = distance(indexed_vertices[edge2_3.vertex1], indexed_vertices[edge2_3.vertex2]);
				for (auto j : edges)
				{
					if ((j.vertex1 == edge2_3.vertex1 && j.vertex2 == edge2_3.vertex2) ||
						(j.vertex1 == edge2_3.vertex2 && j.vertex2 == edge2_3.vertex1))
					{
						if ((edge2_3.vertex1 != edge1_2.vertex1 && edge2_3.vertex2 != edge1_2.vertex2) ||
							(edge2_3.vertex1 != edge1_2.vertex2 && edge2_3.vertex2 != edge1_2.vertex1))
						{
							if ((edge2_3.vertex1 != edge1_3.vertex1 && edge2_3.vertex2 != edge1_3.vertex2) ||
								(edge2_3.vertex1 != edge1_3.vertex2 && edge2_3.vertex2 != edge1_3.vertex1))
							{
								if (edge2_3.distance < shortest.distance || shortest.distance == -1)
								{
									shortest = edge2_3;
								}
							}
						}

					}
				}

				edges.push_back(edge2_3);

			}
		}
	}
	edges[0] = shortest;

}