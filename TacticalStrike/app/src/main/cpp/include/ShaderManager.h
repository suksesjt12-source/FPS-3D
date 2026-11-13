#pragma once

#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <string>
#include <unordered_map>

namespace ts {

class ShaderManager {
public:
    ~ShaderManager();

    bool loadProgram(AAssetManager* mgr,
                     const std::string& name,
                     const std::string& vertPath,
                     const std::string& fragPath);

    GLuint getProgram(const std::string& name) const;
    GLint getUniformLocation(GLuint program, const char* uniformName);
    void clear();

private:
    GLuint compileShader(GLenum type, const std::string& source, const std::string& label);

    std::unordered_map<std::string, GLuint> programs_;
    std::unordered_map<GLuint, std::unordered_map<std::string, GLint>> uniformCache_;
};

} // namespace ts
