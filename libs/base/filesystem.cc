#include "libs/base/filesystem.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/semphr.h"
#include "third_party/nxp/rt1176-sdk/components/flash/nand/fsl_nand_flash.h"
#include <cstdio>

extern "C" nand_handle_t* BOARD_GetNANDHandle(void);

namespace valiant {

namespace filesystem {

static lfs_t lfs_handle_;
static lfs_config lfs_config_;
static SemaphoreHandle_t lfs_semaphore_;
constexpr const int kPagesPerBlock = 64;
constexpr const int kFilesystemBasePage = (0xC * kPagesPerBlock);
constexpr const int kPageSize = 2048;

static int LfsRead(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    nand_handle_t* nand_handle = BOARD_GetNANDHandle();
    if (!nand_handle) {
        return LFS_ERR_IO;
    }

    assert((off % kPageSize) == 0);
    int page_offset = off / kPageSize;
    status_t status = Nand_Flash_Read_Page_Partial(nand_handle, kFilesystemBasePage + (block * kPagesPerBlock) + page_offset, 0, reinterpret_cast<uint8_t*>(buffer), size);
    if (status != kStatus_Success) {
        return LFS_ERR_IO;
    }

    return LFS_ERR_OK;
}

static int LfsProg(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
    return LFS_ERR_INVAL;
}

static int LfsErase(const struct lfs_config *c, lfs_block_t block) {
    return LFS_ERR_INVAL;
}

static int LfsSync(const struct lfs_config *c) {
    return LFS_ERR_INVAL;
}

bool Init() {
    int ret;
    memset(&lfs_config_, 0, sizeof(lfs_config_));
    lfs_config_.read = LfsRead;
    lfs_config_.prog = LfsProg;
    lfs_config_.erase = LfsErase;
    lfs_config_.sync = LfsSync;
    lfs_config_.read_size = 2048;
    lfs_config_.prog_size = 2048;
    lfs_config_.block_size = 131072;
    lfs_config_.block_count = 32;
    lfs_config_.block_cycles = -1;
    lfs_config_.cache_size = 2048;
    lfs_config_.lookahead_size = 2048;
    ret = lfs_mount(&lfs_handle_, &lfs_config_);
    if (ret < 0) {
        return false;
    }

    lfs_semaphore_ = xSemaphoreCreateMutex();
    return true;
}

bool Open(lfs_file_t* handle, const char *path) {
    int ret;
    xSemaphoreTake(lfs_semaphore_, portMAX_DELAY);
    ret = lfs_file_open(&lfs_handle_, handle, path, LFS_O_RDONLY);
    xSemaphoreGive(lfs_semaphore_);

    return (ret == LFS_ERR_OK) ? true : false;
}

int Read(lfs_file_t* handle, void *buffer, size_t size) {
    int ret;
    xSemaphoreTake(lfs_semaphore_, portMAX_DELAY);
    ret = lfs_file_read(&lfs_handle_, handle, buffer, size);
    xSemaphoreGive(lfs_semaphore_);

    return ret;
}

bool Seek(lfs_file_t* handle, size_t off, int whence) {
    int ret;
    xSemaphoreTake(lfs_semaphore_, portMAX_DELAY);
    ret = lfs_file_seek(&lfs_handle_, handle, off, whence);
    xSemaphoreGive(lfs_semaphore_);

    return (ret >= 0) ? true : false;
}

bool Close(lfs_file_t* handle) {
    int ret;
    xSemaphoreTake(lfs_semaphore_, portMAX_DELAY);
    ret = lfs_file_close(&lfs_handle_, handle);
    xSemaphoreGive(lfs_semaphore_);

    return (ret == LFS_ERR_OK) ? true : false;
}

}  // namespace filesystem

}  // namespace valiant