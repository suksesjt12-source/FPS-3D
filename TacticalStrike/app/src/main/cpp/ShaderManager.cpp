#include "ShaderManager.h"
#include "AssetLoader.h"
#include <vector>

namespace ts {

ShaderManager::~ShaderManager() {
    clear();
}

void ShaderManager::clear() {
    for (auto& kv : programs_) {
        if (kv.second != 0) {
            glDeleteProgram(kv.second);
        }
    }
    programs_.clear();
    uniformCache_.clear();
}

GLuint ShaderManager::compileShader(GLenum type, const std::string& source, const std::string& label) {
    GLuint shader = glCreateShader(type);
    if (!shader) {
        TS_LOGE("ShaderManager: glCreateShader failed for %s", label.c_str());
        return 0;
    }
    const char* src = source.c_str();
    GLint len = static_cast<GLint>(source.size());
    glShaderSource(shader, 1, &src, &len);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        std::string log(static_cast<size_t>(infoLen > 0 ? infoLen : 1), '\0');
        if (infoLen > 0) {
            glGetShaderInfoLog(shader, infoLen, nullptr, &log[0]);
        }
        TS_LOGE("ShaderManager: compile error in %s: %s", label.c_str(), log.c_str());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool ShaderManager::loadProgram(AAssetManager* mgr,
                                const std::string& name,
                                const std::string& vertPath,
                                const std::string& fragPath) {
    std::string vertSrc;
    std::string fragSrc;
    if (!AssetLoader::readText(mgr, vertPath, vertSrc)) return false;
    if (!AssetLoader::readText(mgr, fragPath, fragSrc)) return false;

    GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc, vertPath);
    if (!vs) return false;

    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc, fragPath);
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint infoLen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &infoLen);
        std::string log(static_cast<size_t>(infoLen > 0 ? infoLen : 1), '\0');
        if (infoLen > 0) {
            glGetProgramInfoLog(prog, infoLen, nullptr, &log[0]);
        }
        TS_LOGE("ShaderManager: link error for %s: %s", name.c_str(), log.c_str());
        glDeleteProgram(prog);
        return false;
    }

    auto existing = programs_.find(name);
    if (existing != programs_.end() && existing->second != 0) {
        uniformCache_.erase(existing->second);
        glDeleteProgram(existing->second);
    }
    programs_[name] = prog;
    TS_LOGI("ShaderManager: compiled program '%s' (id=%u)", name.c_str(), prog);
    return true;
}

GLuint ShaderManager::getProgram(const std::string& name) const {
    auto it = programs_.find(name);
    return it != programs_.end() ? it->second : 0;
}

GLint ShaderManager::getUniformLocation(GLuint program, const char* uniformName) {
    if (program == 0) return -1;
    auto& cache = uniformCache_[program];
    auto it = cache.find(uniformName);
    if (it != cache.end()) return it->second;
    GLint loc = glGetUniformLocation(program, uniformName);
    cache[uniformName] = loc;
    return loc;
}

} // namespace ts
