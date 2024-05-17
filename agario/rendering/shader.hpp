#pragma once
#include "glad/glad.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

class ShaderException : public std::runtime_error {
  using runtime_error::runtime_error;
};

class ShaderCompilationException : public ShaderException {
  using ShaderException::ShaderException;
};

class ShaderLinkingException : public ShaderException {
  using ShaderException::ShaderException;
};

class Shader {
public:
  GLuint program;

  Shader() : program(0) {};

  Shader(const char *vertex_path, const char *fragment_path) : program(0) {
    generate_shader(vertex_path, fragment_path);
  }

  void generate_shader(const char *vertex_path, const char *fragment_path) {
    std::string vertex_code = get_file_contents(vertex_path);
    std::string fragment_code = get_file_contents(fragment_path);
    compile_shaders(vertex_code, fragment_code);
  }

  std::string get_file_contents(const char *path) {
    std::string contents;
    std::ifstream file;

    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
      file.open(path);
      std::stringstream stream;
      stream << file.rdbuf();
      file.close();
      return stream.str();
    } catch (std::ifstream::failure &e) {
      std::stringstream ss;
      ss << "SHADER FILE \"" << path << "\" not successfully read";
      throw ShaderException(ss.str());
    }
    return std::string();
  }

  void compile_shaders(const std::string &vertex_code, const std::string &fragment_code) {
    const char *vShaderCode = vertex_code.c_str();
    const char *fShaderCode = fragment_code.c_str();

    unsigned int vertex, fragment;

    //Check whether it is already compiled
    std::cout << "Compiling vertex shader" << std::endl;

      if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return;
        
    }
    gladLoadGL();

    // vertex shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, nullptr);
    glCompileShader(vertex);
    check_compile_errors(vertex, "VERTEX");

    // fragment Shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, nullptr);
    glCompileShader(fragment);
    check_compile_errors(fragment, "FRAGMENT");

    // shader Program
    program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    check_compile_errors(program, "PROGRAM");
    // delete the shaders as they're linked into our program now and no longer necessary
    glDeleteShader(vertex);
    glDeleteShader(fragment);
  }

  void use() {
    glUseProgram(program);
  }
  void cleanup() {
    glDeleteProgram(program);
  }
  void setBool(const std::string &name, bool value) const {
    glUniform1i(glGetUniformLocation(program, name.c_str()), (int) value);
  }

  void setInt(const std::string &name, int value) const {
    glUniform1i(glGetUniformLocation(program, name.c_str()), value);
  }

  void setFloat(const std::string &name, float value) const {
    glUniform1f(glGetUniformLocation(program, name.c_str()), value);
  }

  void setVec3(const std::string &name, float value1, float value2, float value3) const {
    glUniform3f(glGetUniformLocation(program, name.c_str()), value1, value2, value3);
  }

  void setVec4(const std::string &name, float value1, float value2, float value3, float value4) const {
    glUniform4f(glGetUniformLocation(program, name.c_str()), value1, value2, value3, value4);
  }

  void setMat4(const std::string &name, const glm::mat4 &matrix) {
    GLint location = glGetUniformLocation(program, name.c_str());
    glUniformMatrix4fv(location, 1, GL_FALSE, &matrix[0][0]);
  }

  std::string version() {
    return std::string((const char *) glGetString(GL_SHADING_LANGUAGE_VERSION));
  }

  ~Shader() { 
  }

private:

  // utility function for checking shader compilation/linking errors.
  void check_compile_errors(unsigned int shader, const std::string &type) {
    int success;
    char infoLog[1024];
    if (type != "PROGRAM") {
      glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
      if (!success) {
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::stringstream ss;
        ss << type << " " << infoLog;
        throw ShaderCompilationException(ss.str());
      }
    } else {
      glGetProgramiv(shader, GL_LINK_STATUS, &success);
      if (!success) {
        glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
        std::stringstream ss;
        ss << type << " " << infoLog;
        throw ShaderLinkingException(ss.str());
      }
    }
  }
};
