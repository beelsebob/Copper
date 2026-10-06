#pragma once

#include <expected>
#include <mutex>
#include <string>

#include <hdf5.h>

namespace copper {

/// Serializes all access to this process's HDF5 library. The framework can keep several SWMR
/// readers open while its writer publishes another frame, so this must be shared by every reader
/// and writer rather than being local to one file handle. It is recursive because RAII cleanup can
/// re-enter HDF5 while a public operation already holds the boundary.
std::recursive_mutex& hdf5ApiMutex();

/// Registers the standard HDF5 Blosc2 filter id (32026) in-process. The implementation uses
/// Blosc2's LZ4 codec at clevel 1 with bitshuffle and its internal worker pool. Registering it
/// directly avoids a runtime HDF5_PLUGIN_PATH dependency while producing ordinary Blosc2 chunks
/// that the published HDF5 plugin can decode too. The decoder remains codec-agnostic, so files
/// previously written with Zstd remain readable.
std::expected<void, std::string> registerHDF5Blosc2Filter();

/// Adds the registered Blosc2 filter to a dataset-creation property list. Chunking must already be
/// configured on `dcpl`; HDF5 invokes this filter once for each complete component chunk.
std::expected<void, std::string> setHDF5Blosc2Filter(hid_t dcpl);

} // namespace copper
