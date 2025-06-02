file(READ "${SOURCE}" TEXT)
string(REPLACE
    "#include <kj/windows-sanity.h>"
    "#include <kj/windows-sanity.h>\n#include <vk_sdk/Preamble.hpp>"
    TEXT "${TEXT}")
string(REGEX REPLACE
    "CAPNP_DECLARE_SCHEMA\\(([a-f0-9]*)\\)"
    "VK_SDK_API extern ::capnp::word const* const bp_\\1;\\nVK_SDK_API extern const ::capnp::_::RawSchema s_\\1"
    TEXT "${TEXT}")
file(WRITE "${TARGET}" "${TEXT}")