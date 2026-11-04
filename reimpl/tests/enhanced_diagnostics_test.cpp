#include "enhanced_diagnostics.hpp"
#include <sstream>
#include <stdexcept>

int main() try {
#ifdef _WIN32
    _putenv_s("STELLAR7_HOST_DIAGNOSTICS","1");
#else
    setenv("STELLAR7_HOST_DIAGNOSTICS","1",1);
#endif
    std::ostringstream output;
    auto* previous=std::cerr.rdbuf(output.rdbuf());
    {
        stellar7::EnhancedHostDiagnostics diagnostics;
        const stellar7::EnhancedHostOperation outer("source step");
        const stellar7::EnhancedHostOperation inner("simulated device block");
        std::this_thread::sleep_for(std::chrono::milliseconds(1150));
    }
    std::cerr.rdbuf(previous);
    if(output.str().find("operation=simulated device block completed=0")==std::string::npos) {
        throw std::runtime_error("heartbeat did not identify the blocked operation");
    }
    if(std::strcmp(stellar7::enhancedHostDetail::operation.load(),"idle")!=0
        ||stellar7::enhancedHostDetail::completed.load()!=2) {
        throw std::runtime_error("diagnostic scope did not restore nested operation");
    }
} catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';return 1;
}
