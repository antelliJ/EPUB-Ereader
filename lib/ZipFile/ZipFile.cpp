#ifndef UNIT_TEST
#include <esp_log.h>
#else
#define ESP_LOGE(args...)
#define ESP_LOGI(args...)
#endif
#include "ZipFile.h"

#include "miniz.h"

#include <esp_heap_caps.h>

#define TAG "ZIP"

// read_file_to_memory2 is probably better for larger files since that uses psram
uint8_t *ZipFile::read_file_to_memory(const char *filename, size_t *size)
{
  // open up the epub file using miniz
  mz_zip_archive zip_archive;
  memset(&zip_archive, 0, sizeof(zip_archive));
  bool status = mz_zip_reader_init_file(&zip_archive, m_filename.c_str(), 0);
  if (!status)
  {
    ESP_LOGE(TAG, "mz_zip_reader_init_file() failed!\n");
    ESP_LOGE(TAG, "Error %s\n", mz_zip_get_error_string(zip_archive.m_last_error));
    return nullptr;
  }
  // find the file
  mz_uint32 file_index = 0;
  if (!mz_zip_reader_locate_file_v2(&zip_archive, filename, nullptr, 0, &file_index))
  {
    ESP_LOGE(TAG, "Could not find file %s", filename);
    mz_zip_reader_end(&zip_archive);
    return nullptr;
  }
  // get the file size - we do this all manually so we can add a null terminator to any strings
  mz_zip_archive_file_stat file_stat;
  if (!mz_zip_reader_file_stat(&zip_archive, file_index, &file_stat))
  {
    ESP_LOGE(TAG, "mz_zip_reader_file_stat() failed!\n");
    ESP_LOGE(TAG, "Error %s\n", mz_zip_get_error_string(zip_archive.m_last_error));
    mz_zip_reader_end(&zip_archive);
    return nullptr;
  }
  // allocate memory for the file
  size_t file_size = file_stat.m_uncomp_size;
  uint8_t *file_data = (uint8_t *)calloc(file_size + 1, 1);
  if (!file_data)
  {
    ESP_LOGE(TAG, "Failed to allocate memory for %s\n", file_stat.m_filename);
    mz_zip_reader_end(&zip_archive);
    return nullptr;
  }
  ESP_LOGE(TAG, "Extracting %s, uncompressed size %u\n", file_stat.m_filename, (unsigned)file_size);

  // read the file
  status = mz_zip_reader_extract_to_mem(&zip_archive, file_index, file_data, file_size, 0);
  if (!status)
  {
    ESP_LOGE(TAG, "mz_zip_reader_extract_to_mem() failed!\n");
    ESP_LOGE(TAG, "Error %s\n", mz_zip_get_error_string(zip_archive.m_last_error));
    free(file_data);
    mz_zip_reader_end(&zip_archive);
    return nullptr;
  }

  ESP_LOGE(TAG, "Successfully extracted %s\n", file_stat.m_filename);

  // Close the archive, freeing any resources it was using
  mz_zip_reader_end(&zip_archive);
  // return the size if required
  if (size)
  {
    *size = file_size;
  }

  ESP_LOGE(TAG, "Successfully extracted, reader has ended, returning data\n");
  return file_data;
}

// read a file from the zip file allocating the required memory for the data
// This function uses psram for the file data, which is much larger than the internal heap, and is suitable for large files like images or HTML content
uint8_t *ZipFile::read_file_to_memory2(const char *filename, size_t *size)
{  // open up the epub file using miniz
  mz_zip_archive zip_archive;
  memset(&zip_archive, 0, sizeof(zip_archive));
  bool status = mz_zip_reader_init_file(&zip_archive, m_filename.c_str(), 0);
  if (!status)
  {
    ESP_LOGE(TAG, "mz_zip_reader_init_file() failed!\n");
    ESP_LOGE(TAG, "Error %s\n", mz_zip_get_error_string(zip_archive.m_last_error));
    return nullptr;
  }
  // find the file
  mz_uint32 file_index = 0;
  if (!mz_zip_reader_locate_file_v2(&zip_archive, filename, nullptr, 0, &file_index))
  {
    ESP_LOGE(TAG, "Could not find file %s", filename);
    mz_zip_reader_end(&zip_archive);
    return nullptr;
  }
  // get the file size - we do this all manually so we can add a null terminator to any strings
  mz_zip_archive_file_stat file_stat;
  if (!mz_zip_reader_file_stat(&zip_archive, file_index, &file_stat))
  {
    ESP_LOGE(TAG, "mz_zip_reader_file_stat() failed!\n");
    ESP_LOGE(TAG, "Error %s\n", mz_zip_get_error_string(zip_archive.m_last_error));
    mz_zip_reader_end(&zip_archive);
    return nullptr;
  }
  // allocate memory for the file
  size_t file_size = file_stat.m_uncomp_size;

  // Allocate memory in PSRAM using heap_caps_malloc
  uint8_t *file_data = (uint8_t *)heap_caps_calloc(file_size + 1, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!file_data) {
    ESP_LOGE(TAG, "Failed to allocate PSRAM memory for %s (size: %u)\n", file_stat.m_filename, (unsigned)file_size);
    mz_zip_reader_end(&zip_archive);
    return nullptr;
  }

  ESP_LOGI(TAG, "Extracting %s, uncompressed size %u\n", file_stat.m_filename, (unsigned)file_size);

  // make miniz use heap allocation (not stack)
  // this is so that miniz has a decompression buffer
  mz_zip_reader_extract_iter_state* pState = mz_zip_reader_extract_iter_new(&zip_archive, file_index, 0);
  if (!pState) {
    ESP_LOGE(TAG, "mz_zip_reader_extract_iter_new() failed!\n");
    ESP_LOGE(TAG, "Error %s\n", mz_zip_get_error_string(zip_archive.m_last_error));
    heap_caps_free(file_data);
    mz_zip_reader_end(&zip_archive);
    return nullptr;
  }

  // Read in chunks
  size_t bytes_read = 0;
  const size_t chunk_size = 8192; // 8KB chunk size
  while (bytes_read < file_size) {
    size_t bytes_to_read = ((file_size - bytes_read) < chunk_size) ? (file_size - bytes_read) : chunk_size;
    int read = mz_zip_reader_extract_iter_read(pState, file_data + bytes_read, bytes_to_read);
    if (read == 0) {
      ESP_LOGE(TAG, "mz_zip_reader_extract_iter_read() failed with error %d!\n", read);
      ESP_LOGE(TAG, "Error %s\n", mz_zip_get_error_string(zip_archive.m_last_error));
      heap_caps_free(file_data);
      mz_zip_reader_extract_iter_free(pState);
      mz_zip_reader_end(&zip_archive);
      return nullptr;
    }
    bytes_read += read;
  }

  status = mz_zip_reader_extract_iter_free(pState);
  if (!status || bytes_read != file_size) {
    ESP_LOGE(TAG, "Extraction incomplete or failed! Expected %u bytes, read %u bytes.\n", (unsigned)file_size, (unsigned)bytes_read);
    heap_caps_free(file_data);
    mz_zip_reader_end(&zip_archive);
    return nullptr;
  }

  ESP_LOGI(TAG, "Successfully extracted %s\n", file_stat.m_filename);

  mz_zip_reader_end(&zip_archive);

  if (size) {
    *size = file_size;
  }
  return file_data;

}
bool ZipFile::read_file_to_file(const char *filename, const char *dest)
{
  mz_zip_archive zip_archive;
  memset(&zip_archive, 0, sizeof(zip_archive));
  bool status = mz_zip_reader_init_file(&zip_archive, m_filename.c_str(), 0);
  if (!status)
  {
    ESP_LOGE(TAG, "mz_zip_reader_init_file() failed!\n");
    ESP_LOGE(TAG, "Error %s\n", mz_zip_get_error_string(zip_archive.m_last_error));
    return false;
  }
  // Run through the archive and find the requiested file
  for (int i = 0; i < (int)mz_zip_reader_get_num_files(&zip_archive); i++)
  {
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
    {
      ESP_LOGE(TAG, "mz_zip_reader_file_stat() failed!\n");
      ESP_LOGE(TAG, "Error %s\n", mz_zip_get_error_string(zip_archive.m_last_error));
      mz_zip_reader_end(&zip_archive);
      return false;
    }
    // is this the file we're looking for?
    if (strcmp(filename, file_stat.m_filename) == 0)
    {
      ESP_LOGI(TAG, "Extracting %s\n", file_stat.m_filename);
      mz_zip_reader_extract_file_to_file(&zip_archive, file_stat.m_filename, dest, 0);
      mz_zip_reader_end(&zip_archive);
      return true;
    }
  }
  mz_zip_reader_end(&zip_archive);
  return false;
}
