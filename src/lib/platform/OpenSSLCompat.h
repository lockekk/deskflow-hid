/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#pragma once

namespace deskflow::platform {

// Initializes OpenSSL providers (if necessary) and sets up environment variables.
// This should be called early in the application startup (main).
void initializeOpenSSL();

} // namespace deskflow::platform
