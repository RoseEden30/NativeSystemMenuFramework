#pragma once

namespace Logging
{
    // Sets up the spdlog logger under the SKSE log folder. Call first thing
    // in SKSEPluginLoad, before Config is available.
    void Init();

    // Switches between debug and info level. Callable any time, so Verbose
    // can be flipped from the menu without a restart.
    void SetVerbose(bool a_verbose = true);
}
