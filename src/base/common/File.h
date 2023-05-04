#pragma once

#include "ErrorUtils.h"

#include <filesystem>
#include <fstream>

namespace hestia {

/**
 * @brief Convenience class for a filesystem File
 *
 * A filesystem File with some convenience wrappers for opening, closing
 * and error handling.
 */
class File {
  public:
    using Path = std::filesystem::path;

    /**
     * Constructor
     *
     * @param path Path to the file
     */
    File(const Path& path = {});

    /**
     * Destructor
     */
    ~File();

    /**
     * Close the file
     * @return Status of the close operation, reports if e.g. a stream flush fails.
     */
    OpStatus close();

    struct ReadState {
        std::size_t m_size_read{0};
        bool m_finished{false};
    };

    /**
     * Read from the file - the file is opened if needed
     * @param data buffer to be read into
     * @param length length of the buffer to read into
     * @return Status of the read operation, number of bytes read and if EOF is hit
     */
    std::pair<OpStatus, ReadState> read(char* data, std::size_t length);

    /**
     * Write to the file - the file is opened if needed
     * @param data buffer to write from
     * @param length length of the buffer to write from
     * @return Status of the write operation
     */
    OpStatus write(const char* data, std::size_t length);

  private:
    OpStatus open_for_read() noexcept;
    OpStatus open_for_write() noexcept;

    Path m_path;
    std::ofstream m_out_stream;
    std::ifstream m_in_stream;
};
}  // namespace hestia