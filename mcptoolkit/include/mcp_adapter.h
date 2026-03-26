#pragma once

#if defined(_WIN32) && defined(MCPTOOLKIT_SHARED)
  #ifdef MCPTOOLKIT_EXPORTS
    #define MCPTOOLKIT __declspec(dllexport)
  #else
    #define MCPTOOLKIT __declspec(dllimport)
  #endif
#elif defined(MCPTOOLKIT_SHARED) && defined(__GNUC__)
  #define MCPTOOLKIT __attribute__((visibility("default")))
#else
  #define MCPTOOLKIT
#endif

namespace mcptoolkit {

class MCPTOOLKIT MCPAdapter {
public:
    MCPAdapter();
    virtual ~MCPAdapter() = default;

    void init();
    void run();

protected:

private:
};

} // namespace mcptoolkit