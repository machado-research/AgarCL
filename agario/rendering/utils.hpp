#pragma once

#ifdef __LINUX__
  #include <GL/glut.h>
  #include <GL/gl.h>
  #include <GL/glu.h>
#endif

class FBOException : public std::runtime_error {
  // using runtime_error::runtime_error;
  public:
    using std::runtime_error::runtime_error;

    FBOException(const std::string &message) : std::runtime_error(message) {
        std::cout << "FBOException: " << message << std::endl;
    }

    FBOException(const char *message) : std::runtime_error(message) {
        std::cout << "FBOException: " << message << std::endl;
    }
};


void exception_on_gl_error(std::string error){
  auto glstatus = glGetError();
    if (glstatus != GL_NO_ERROR)
    {
      std::string es((const char *)gluErrorString(glstatus));
      throw FBOException("GL Error in copy "+ error + ' '+ std::to_string(glstatus)+ ", "+ es);
      }
}
